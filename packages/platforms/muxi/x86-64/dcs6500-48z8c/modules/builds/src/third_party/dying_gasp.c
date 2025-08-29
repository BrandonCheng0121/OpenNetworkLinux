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
 #include <linux/device.h>
 #include <linux/cdev.h>
 #include <linux/platform_device.h>
 #include <linux/acpi.h>
 #include <linux/jiffies.h>
 #include <linux/pci.h>
 #include <linux/timer.h>
 #include <linux/timex.h>
 #include <linux/rtc.h>
 #include <linux/file.h>
 #include <linux/fs.h>
 #include <asm/nmi.h>
 #include "libboundscheck/include/securec.h"
 
 
 ///////////////////////////////////////////////////////////////////////////////
 
 #define PREFIX "ACPI: "
 
 #define MODULE_NAME                         "dying_gasp"
 
 #define ACPI_DYING_GASP_CLASS               "dying_gasp"
 
 #define LOG_FILE                            "/var/log/dying_gasp.log"
 
 #define INTERRUPT_NAME  "gpio_intr"
 #define INTERRUPT_CT    1
 
 
 
 ///////////////////////////////////////////////////////////////////////////////
 // LPC PCIE registers
 #define LPC_ICH_VENDOR_ID                   0x8086
 #define LPC_ICH_DEVICE_ID                   0x8C54
 #define GPIOBASE_OFFSET                     0x48
 #define GPI_ROUT_GPIO9_OFFSET               0xba
 #define GPI_ROUT_GPIO9_NMI                  0x0c
 #define GPI_ROUT_GPIO12_OFFSET              0xbb
 #define GPI_ROUT_GPIO12_NMI                 0x03
 #define RCBABASE_OFFSET                     0xf0
 
 // GPIO IO registers
 #define GPI_NMI_EN_OFFSET                   0x28
 #define GPI_NMI_STS_OFFSET                  0x2a
 #define GPI_INV_OFFSET                      0x2c
 #define GP_IO_SEL_OFFSET                    0x04
 #define GPI_NMI_INV_9_OFFSET                9
 #define GPI_NMI_INV_12_OFFSET               12
 #define GPI_NMI_STS_GPIO9_ACTIVE            0x200
 #define GPI_NMI_STS_GPIO12_ACTIVE           0x1000
 
 // PRSTS register.
 #define LPC_ICH_MEM_LEN                     0x4000
 #define LPC_ICH_PRSTS                       0x3310
 #define LPC_ICH_RPFN                        0x0404
 
 ///////////////////////////////////////////////////////////////////////////////
 static void dying_gasp(struct work_struct *pdying_gasp_work );
 
 static struct work_struct dying_gasp_work;
 struct mutex  update_lock;
 
 static DECLARE_WORK(dying_gasp_work, dying_gasp);
 ///////////////////////////////////////////////////////////////////////////////
 struct interrupt_struct{
     int major;
     dev_t devid;
     struct cdev cdev;
     struct class *class;
     struct device *device;
     struct device_node *nd;
     int gpio_num;
 
     struct timer_list timer;
 
     atomic_t key_value;
     atomic_t key_free;
 };
 
 struct interrupt_struct interrupt_dev;
 
 
 #if 0
 #define DYING_GASP_DEBUG(...)
 #else
 #define DYING_GASP_DEBUG(...) printk(KERN_ALERT __VA_ARGS__)
 #endif
 
 ACPI_MODULE_NAME("dying_gasp");
 
 MODULE_DESCRIPTION("S3IP Dying Gasp GPIO NMI Driver");
 MODULE_LICENSE("GPL");
 
 
 ///////////////////////////////////////////////////////////////////////////////
 
 static char log_buf[4096] = {0};
 u32 lpc_ich_rcba_addr = 0;
 u32 lpc_ich_gpioba_addr = 0;
 
 static int msr_mc_banks[] = {
     0, 1, 2, 3, 4, 7, 9, 10, 17, 18, 19,
 };
 
 static char* msr_mc_register[] = {
     "STATUS", "ADDR", "MISC",
 };
 
 #define MSR_MC_BANK_NUM   sizeof(msr_mc_banks)/sizeof(int)
 #define MSR_MC_REG_NUM    sizeof(msr_mc_register)/sizeof(char*)
 struct pci_dev* lpc_ich_dev = NULL;
 
 ///////////////////////////////////////////////////////////////////////////////
 
 /* MSR access wrappers used for error injection */
 static u64 mce_rdmsrl(u32 msr)
 {
     u64 v = 0;
 
     if (rdmsrl_safe(msr, &v))
     {
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
 
 static inline void append_logbuf(char* buf, int str_len)
 {
     int len = 0;
     int log_len = 0;
 
     len = strlen(buf);
     if (len != str_len){
         DYING_GASP_DEBUG("dying gasp: append_logbuf fail. len is not right.\n");
         return ;
     }
     log_len = strlen(log_buf);
     if(likely( (log_len + len) < sizeof(log_buf) )) {
         strcat_s(log_buf, 4096+1, buf);
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
     snprintf_s(buf, sizeof(buf), sizeof(buf) - 1, "\nUTC time :%4d-%02d-%02dT%02d:%02d:%02d\n",
         tm.tm_year+1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
     buf[sizeof(buf) - 1] = 0;
     append_logbuf(buf, strlen(buf));
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
     snprintf_s(buf, sizeof(buf), (sizeof(buf) - 1), "%s ", "MSR_MCx");
     append_logbuf(buf, strlen(buf));
     for(index = 0; index < MSR_MC_BANK_NUM; index++) {
         msr_mc_index = msr_mc_banks[index];
         memset_s(msr_mc_val, sizeof(msr_mc_val)+1, 0, sizeof(msr_mc_val));
         msr_mc_read(index, msr_mc_val);
         snprintf_s(buf, sizeof(buf), (sizeof(buf) - 1), "[%d: 0x%llX  0x%llX  0x%llX] ", msr_mc_index, msr_mc_val[0], msr_mc_val[1], msr_mc_val[2]);
         append_logbuf(buf, strlen(buf));
     }
 
     // MCG_STATUS
     mcg_status = mce_rdmsrl(MSR_IA32_MCG_STATUS);
     snprintf_s(buf, sizeof(buf), (sizeof(buf) - 1), "; MCG_STATUS [0x%llX]", mcg_status);
     append_logbuf(buf, strlen(buf));
 
     // PRSTS
     if(likely(0 != lpc_ich_rcba_addr)) {
         lpc_ich_rcba_mem = (u8*)ioremap(lpc_ich_rcba_addr, LPC_ICH_MEM_LEN);
         if(likely(NULL != lpc_ich_rcba_mem)) {
             prsts = *((u32*)(lpc_ich_rcba_mem + LPC_ICH_PRSTS));
             rpfn  = *((u32*)(lpc_ich_rcba_mem + LPC_ICH_RPFN));
             snprintf_s(buf, sizeof(buf), (sizeof(buf) - 1), "; PRSTS [0x%X] RPFN [0x%X]\n", prsts, rpfn);
             append_logbuf(buf, strlen(buf));
 
             iounmap(lpc_ich_rcba_mem);
         }
         else {
             DYING_GASP_DEBUG("Cannot remap LPC-ICH memory region\n");
         }
     }
     else {
         DYING_GASP_DEBUG("Cannot get LPC-ICH address\n");
     }
 
 }
 
 static inline void save_log_in_file(char* buf, int buf_len)
 {
     struct file *fp = NULL;
 
     fp = filp_open(LOG_FILE, O_RDWR | O_APPEND | O_CREAT, 0640);
     if (unlikely(NULL == fp)) {
         DYING_GASP_DEBUG("%s: %s(): fail to open log file", MODULE_NAME, __func__);
         return;
     }
 
     kernel_write(fp, buf, buf_len, &fp->f_pos);
     vfs_fsync(fp, 0);
 
     filp_close(fp, NULL);
     fp = NULL;
 
     DYING_GASP_DEBUG("%s: %s(): done", MODULE_NAME, __func__);
 }
 
 ///////////////////////////////////////////////////////////////////////////////
 
 
 /* --------------------------------------------------------------------------
                                    Driver Model
    -------------------------------------------------------------------------- */
 
 static void set_gpio_io_reg(u8 offset, int mask, int value)
 {
     int val;
 
     val = inl(lpc_ich_gpioba_addr + offset);
     val &= ~(1 << mask);
     val |= (value << mask);
     outl(val, lpc_ich_gpioba_addr + offset);
 }
 
 static void config_gpio9_nmi(void)
 {
     // Set GPI_ROUT[19:18] to 11'b
     pci_write_config_byte(lpc_ich_dev, GPI_ROUT_GPIO9_OFFSET, GPI_ROUT_GPIO9_NMI);
 
     mutex_lock(&update_lock);
 
     // Set GPI_INV[9] to 1'b
     set_gpio_io_reg(GPI_INV_OFFSET, GPI_NMI_INV_9_OFFSET, 1);
 
     // Set GPI_NMI_EN[9] to 1'b
     set_gpio_io_reg(GPI_NMI_EN_OFFSET, GPI_NMI_INV_9_OFFSET, 1);
 
     mutex_unlock(&update_lock);
 }
 
 static int check_gpio9_nmi_sts(void)
 {
     int ret;
 
     mutex_lock(&update_lock);
 
     ret = inl(lpc_ich_gpioba_addr + GPI_NMI_STS_OFFSET) & GPI_NMI_STS_GPIO9_ACTIVE;
 
     mutex_unlock(&update_lock);
     return ret;
 }
 
 static void clear_gpio9_nmi_sts(void)
 {
     mutex_lock(&update_lock);
 
     set_gpio_io_reg(GPI_NMI_STS_OFFSET, GPI_NMI_INV_9_OFFSET, 1);
 
     mutex_unlock(&update_lock);
 }
 
 
 static void config_gpio12_nmi(void)
 {
     // Set GPI_ROUT[19:18] to 11'b
     pci_write_config_byte(lpc_ich_dev, GPI_ROUT_GPIO12_OFFSET, GPI_ROUT_GPIO12_NMI);
 
     mutex_lock(&update_lock);
 
     // Set GP_IO_SEL[12] to 1'b
     set_gpio_io_reg(GP_IO_SEL_OFFSET, GPI_NMI_INV_12_OFFSET, 1);
 
     // Set GPI_INV[12] to 1'b
     set_gpio_io_reg(GPI_INV_OFFSET, GPI_NMI_INV_12_OFFSET, 1);
 
     // Set GPI_NMI_EN[12] to 1'b
     set_gpio_io_reg(GPI_NMI_EN_OFFSET, GPI_NMI_INV_12_OFFSET, 1);
 
     mutex_unlock(&update_lock);
 }
 
 static int check_gpio12_nmi_sts(void)
 {
     int ret;
 
     mutex_lock(&update_lock);
 
     ret = inl(lpc_ich_gpioba_addr + GPI_NMI_STS_OFFSET) & GPI_NMI_STS_GPIO12_ACTIVE;
 
     mutex_unlock(&update_lock);
     return ret;
 }
 
 static void clear_gpio12_nmi_sts(void)
 {
     mutex_lock(&update_lock);
 
     set_gpio_io_reg(GPI_NMI_STS_OFFSET, GPI_NMI_INV_12_OFFSET, 1);
 
     mutex_unlock(&update_lock);
 }
 
 static void dying_gasp(struct work_struct *pdying_gasp_work )
 {
     unsigned long start = 0, end = 0;
     start = jiffies;
     // DYING_GASP_DEBUG("%s: %s(): start", MODULE_NAME, __func__);
     // Save current time.
     dump_timestamp();
     // Save register content.
     dump_dbg_register();
     // Save all debug data into the log file, i.e. "/var/log/dying_gasp.log".
     save_log_in_file(log_buf, strlen(log_buf));
     end = jiffies;
     DYING_GASP_DEBUG("%s: %s(): debug information collected, cost (%ld/%d) second.\n", MODULE_NAME, __func__, (end - start), HZ);
 }
 
 /*
  *  NMI Handler
  */
 static int gpio_intr_handler(unsigned int ulReason, struct pt_regs *regs)
 
 {
     /*Interrupt the top half*/
 
     int status = check_gpio9_nmi_sts();
     // int status = check_gpio12_nmi_sts();
     if(status)
     {
         printk("%s %d\n", __func__, status);
         clear_gpio9_nmi_sts();
         // clear_gpio12_nmi_sts();
         schedule_work(&dying_gasp_work);
     }
 
     return NMI_HANDLED;
 }
 
 static int __init _dying_gasp_init(void)
 {
     int ret = 0;
     u32 base_addr_cfg = 0;
 
     ret = register_nmi_handler(NMI_UNKNOWN, gpio_intr_handler, 0, "dying_gasp_nmi");
 
     if(ret < 0){
         printk("dying gasp dev: register_nmi_handler failed. (%d)\r\n", ret);
         goto nmi_err;
     }
 
     lpc_ich_dev = pci_get_device(LPC_ICH_VENDOR_ID, LPC_ICH_DEVICE_ID, NULL);
     if (lpc_ich_dev == NULL){
         ret = -EINVAL;
         printk("dying gasp dev: pci get device failed. (%d)\r\n", ret);
         goto pci_err;
     }
 
     // For the register PRSTS.
     ret = pci_read_config_dword(lpc_ich_dev, RCBABASE_OFFSET, &base_addr_cfg);
     if(ret != 0){
         printk("dying gasp dev: pci_read_config_dword RCBABASE failed. (%d)\r\n", ret);
         goto pci_read_err;
     }
     lpc_ich_rcba_addr = base_addr_cfg & 0xffffc000;
 
     ret = pci_read_config_dword(lpc_ich_dev, GPIOBASE_OFFSET, &base_addr_cfg);
     if(ret != 0){
         printk("dying gasp dev: pci_read_config_dword GPIOBASE failed. (%d)\r\n", ret);
         goto pci_read_err;
     }
     lpc_ich_gpioba_addr = base_addr_cfg & 0xff80;
 
     mutex_init(&update_lock);
 
     config_gpio9_nmi();
     // config_gpio12_nmi();
 
     // ret = inl(lpc_ich_gpioba_addr + 0x00);
     // printk("GPIO_USE_SEL = %x ", ret);
 
     // ret = inl(lpc_ich_gpioba_addr + 0x04);
     // printk("GP_IO_SEL = %x ", ret);
 
     return 0;
 pci_read_err:
     pci_dev_put(lpc_ich_dev);
 pci_err:
     unregister_nmi_handler(NMI_UNKNOWN, "dying_gasp_nmi");
 nmi_err:
     return ret;
 }
 
 static void __exit _dying_gasp_exit(void)
 {
     printk("dying gasp dev: remove dying gasp driver.\r\n");
     mutex_destroy(&update_lock);
     unregister_nmi_handler(NMI_UNKNOWN, "dying_gasp_nmi");
     pci_dev_put(lpc_ich_dev);
 }
 
 module_init(_dying_gasp_init);
 module_exit(_dying_gasp_exit);
 