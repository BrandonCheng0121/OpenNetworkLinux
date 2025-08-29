/*
 *  acpi_ac.c - ACPI AC Adapter Driver ($Revision: 27 $)
 *
 *  Copyright (C) 2001, 2002 Andy Grover <andrew.grover@intel.com>
 *  Copyright (C) 2001, 2002 Paul Diefenbaugh <paul.s.diefenbaugh@intel.com>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or (at
 *  your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

 #include <linux/kernel.h>
 #include <linux/module.h>
 #include <linux/slab.h>
 #include <linux/init.h>
 #include <linux/types.h>
 #include <linux/delay.h>
 #include <linux/platform_device.h>
 #include <linux/acpi.h>
 #include <linux/jiffies.h>
 #include <linux/pci.h>
 #include <linux/timer.h>
 #include <linux/timex.h>
 #include <linux/rtc.h>
 #include <linux/file.h>
 #include <linux/fs.h>
 #include "libboundscheck/include/securec.h"
 
 ///////////////////////////////////////////////////////////////////////////////
 
 #define PREFIX "ACPI: "
 
 #define MODULE_NAME                         "dying_record"
 
 #define ACPI_DYING_RECORD_CLASS             "dying_record"
 #define ACPI_DYING_RECORD_NOTIFY_STATUS     0x80
 
 #define LOG_FILE                            "/var/log/dying_record.log"
 
 ///////////////////////////////////////////////////////////////////////////////
 #define LPC_ICH_VENDOR_ID                   0x8086
 #define LPC_ICH_DEVICE_ID                   0x8C54
 
 // PRSTS register.
 
 #define RCBABASE                            0xf0
 #define LPC_ICH_MEM_LEN                     0x4000
 #define LPC_ICH_PRSTS                       0x3310
 #define LPC_ICH_RPFN                        0x0404
 
 ///////////////////////////////////////////////////////////////////////////////
 
 
 #if 0
 #define DYING_RECORD_DEBUG(...)
 #else
 #define DYING_RECORD_DEBUG(...) printk(KERN_ALERT __VA_ARGS__)
 #endif
 
 ACPI_MODULE_NAME("dying_record");
 
 MODULE_DESCRIPTION("ACPI Dying Record Virtual Device Driver");
 MODULE_LICENSE("GPL");
 
 
 static int acpi_dying_record_add(struct acpi_device *device);
 static int acpi_dying_record_remove(struct acpi_device *device);
 static void acpi_dying_record_notify(struct acpi_device *device, u32 event);
 
 ///////////////////////////////////////////////////////////////////////////////
 
 static char log_buf[4096] = {0};
 u32 lpc_ich_rcba_addr = 0;
 
 static int msr_mc_banks[] = {
     0, 1, 2, 3, 4, 7, 9, 10, 17, 18, 19,
 };
 
 static char* msr_mc_register[] = {
     "STATUS", "ADDR", "MISC",
 };
 
 #define MSR_MC_BANK_NUM   sizeof(msr_mc_banks)/sizeof(int)
 #define MSR_MC_REG_NUM    sizeof(msr_mc_register)/sizeof(char*)
 
 ///////////////////////////////////////////////////////////////////////////////
 
 /* MSR access wrappers used for error injection */
 static u64 mce_rdmsrl(u32 msr)
 {
     u64 v;
 
     if (rdmsrl_safe(msr, &v))
     {
         //CPU_DEBUG("mce: Unable to read MSR 0x%x!\n", msr);
         v = 0xffffffffffffffff;
     }
 
     return v;
 }
 
 static inline void msr_mc_read(int index, u64* value)
 {
     value[0] = mce_rdmsrl(MSR_IA32_MCx_STATUS(index));
     value[1] = mce_rdmsrl(MSR_IA32_MCx_ADDR(index));
     value[2] = mce_rdmsrl(MSR_IA32_MCx_MISC(index));
 }
 
 static inline void append_logbuf(char* buf)
 {
     int len = 0;
     int log_len = 0;
 
     len = strlen(buf);
     log_len = strlen(log_buf);
     if(likely( (log_len + len) < sizeof(log_buf) )) {
         strcat_s(log_buf, 4096, buf);
     }
     log_buf[sizeof(log_buf) - 1] = 0;
 }
 
 static inline void dump_timestamp(void)
 {
     char buf[128] = {0};
     struct timespec64  txc;
     struct rtc_time tm;
 
     ktime_get_real_ts64(&txc);
     rtc_time64_to_tm(txc.tv_sec, &tm);
     snprintf_s(buf, 128, sizeof(buf) - 1, "\nUTC time :%4d-%02d-%02dT%02d:%02d:%02d\n",
             tm.tm_year+1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
     buf[sizeof(buf) - 1] = 0;
     append_logbuf(buf);
 }
 
 static inline void dump_dbg_register(void)
 {
     int index = 0;
     int msr_mc_index = 0;
     char buf[256] = {0};
     u64 msr_mc_val[MSR_MC_REG_NUM] = {0};
     u64 mcg_status = 0;
     u32 prsts = 0;
     u32 rpfn = 0;
     u8* lpc_ich_rcba_mem = NULL;
 
     // MCx
     snprintf_s(buf, 256, (sizeof(buf) - 1), "%s ", "MSR_MCx");
     append_logbuf(buf);
     for(index = 0; index < MSR_MC_BANK_NUM; index++) {
         msr_mc_index = msr_mc_banks[index];
         memset_s(msr_mc_val, MSR_MC_REG_NUM, 0, sizeof(msr_mc_val));
         msr_mc_read(index, msr_mc_val);
         snprintf_s(buf, 256, (sizeof(buf) - 1), "[%d: 0x%llX  0x%llX  0x%llX] ", msr_mc_index, msr_mc_val[0], msr_mc_val[1], msr_mc_val[2]);
         append_logbuf(buf);
     }
     //DYING_RECORD_DEBUG("%s: %s(): %s\n", MODULE_NAME, __func__, log_buf);
 
     // MCG_STATUS
     mcg_status = mce_rdmsrl(MSR_IA32_MCG_STATUS);
     snprintf_s(buf, 256, (sizeof(buf) - 1), "; MCG_STATUS [0x%llX]", mcg_status);
     append_logbuf(buf);
     //DYING_RECORD_DEBUG("%s: %s(): %s\n", MODULE_NAME, __func__, buf);
 
     // PRSTS
     if(likely(0 != lpc_ich_rcba_addr)) {
         lpc_ich_rcba_mem = (u8*)ioremap(lpc_ich_rcba_addr, LPC_ICH_MEM_LEN);
         if(likely(NULL != lpc_ich_rcba_mem)) {
             prsts = *((u32*)(lpc_ich_rcba_mem + LPC_ICH_PRSTS));
             rpfn  = *((u32*)(lpc_ich_rcba_mem + LPC_ICH_RPFN));
             snprintf_s(buf, 256, (sizeof(buf) - 1), "; PRSTS [0x%X] RPFN [0x%X]", prsts, rpfn);
             append_logbuf(buf);
             //DYING_RECORD_DEBUG("%s: %s(): %s\n", MODULE_NAME, __func__, buf);
 
             iounmap(lpc_ich_rcba_mem);
         }
         else {
             DYING_RECORD_DEBUG("Cannot remap LPC-ICH memory region\n");
         }
     }
     else {
         DYING_RECORD_DEBUG("Cannot get LPC-ICH address\n");
     }
 
     append_logbuf("\n");
 }
 
 static inline void save_log_in_file(char* buf, int buf_len)
 {
     struct file *fp = NULL;
 
     fp = filp_open(LOG_FILE, O_RDWR | O_APPEND | O_CREAT, 0640);
     if (unlikely(NULL == fp)) {
         DYING_RECORD_DEBUG("%s: %s(): fail to open log file", MODULE_NAME, __func__);
     }
     else {
         kernel_write(fp, buf, buf_len, &fp->f_pos);
         vfs_fsync(fp, 0);
         filp_close(fp, NULL);
         fp = NULL;
     }
 
     DYING_RECORD_DEBUG("%s: %s(): done", MODULE_NAME, __func__);
 }
 
 ///////////////////////////////////////////////////////////////////////////////
 
 static const struct acpi_device_id dying_record_device_ids[] = {
     {"ECN0D01", 0},
     {"", 0},
 };
 MODULE_DEVICE_TABLE(acpi, dying_record_device_ids);
 
 static struct acpi_driver acpi_dying_record_driver = {
     .name = "dying_record",
     .class = ACPI_DYING_RECORD_CLASS,
     .ids = dying_record_device_ids,
     .ops = {
         .add = acpi_dying_record_add,
         .remove = acpi_dying_record_remove,
         .notify = acpi_dying_record_notify,
         },
     .owner =    THIS_MODULE,
 };
 
 /* --------------------------------------------------------------------------
                                    Driver Model
    -------------------------------------------------------------------------- */
 
 static void acpi_dying_record_notify(struct acpi_device *device, u32 event)
 {
     unsigned long start = 0, end = 0;
 
     //DYING_RECORD_DEBUG("%s: %s(): acpi_dying_record_notify\n", MODULE_NAME, __func__);
 
     switch (event)
     {
         case ACPI_DYING_RECORD_NOTIFY_STATUS:
         default:
             start = jiffies;
             //DYING_RECORD_DEBUG("%s: %s(): Got dying gasp virtual device notify, event id = 0x%x\n", MODULE_NAME, __func__, event);
 
             log_buf[0] = 0;
 
             // Save current time.
             dump_timestamp();
             // Save register content.
             dump_dbg_register();
             // Save all debug data into the log file, i.e. "/var/log/dying_record.log".
             save_log_in_file(log_buf, strlen(log_buf));
 
             //DYING_RECORD_DEBUG("%s: %s(): dumping stack.\n", MODULE_NAME, __func__);
             //dump_stack();
 
             end = jiffies;
             DYING_RECORD_DEBUG("%s: %s(): debug information collected, cost (%ld/%d) second.\n", MODULE_NAME, __func__, (end - start), HZ);
             break;
     }
 
     return;
 }
 
 static int acpi_dying_record_add(struct acpi_device *device)
 {
     printk(KERN_WARNING "%s: %s(): acpi_dying_record_add\n", MODULE_NAME, __func__);
     return 0;
 }
 
 static int acpi_dying_record_remove(struct acpi_device *device)
 {
     printk(KERN_WARNING "%s: %s(): acpi_dying_record_remove\n", MODULE_NAME, __func__);
     return 0;
 }
 
 static int __init acpi_dying_record_init(void)
 {
     int result = 0;
     u32 base_addr_cfg = 0;
     struct pci_dev* lpc_ich_dev = NULL;
 
     memset_s(log_buf, 4096, 0, sizeof(log_buf));
 
     result = acpi_bus_register_driver(&acpi_dying_record_driver);
     if (result < 0) {
         DYING_RECORD_DEBUG("%s: %s(): Error registering driver\n", MODULE_NAME, __func__);
         return -ENODEV;
     }
 
     // For the register PRSTS.
     lpc_ich_dev = pci_get_device(LPC_ICH_VENDOR_ID, LPC_ICH_DEVICE_ID, NULL);
     printk(KERN_WARNING "%s: %s(): lpc_ich_dev = 0x%p\n", MODULE_NAME, __func__, (void*)lpc_ich_dev);
     if(NULL != lpc_ich_dev) {
         pci_read_config_dword(lpc_ich_dev, RCBABASE, &base_addr_cfg);
         lpc_ich_rcba_addr = base_addr_cfg & 0xffffc000;
     }
 
     return 0;
 }
 
 static void __exit acpi_dying_record_exit(void)
 {
     acpi_bus_unregister_driver(&acpi_dying_record_driver);
 }
 
 module_init(acpi_dying_record_init);
 module_exit(acpi_dying_record_exit);
 