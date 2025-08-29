#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include "fpga_driver.h"
#include "switch_fpga_driver.h"

#define DRVNAME "drv_fpga_driver"
#define SWITCH_FPGA_DRIVER_VERSION "0.0.1"

unsigned int loglevel = 0;
//module_param(loglevel,int,S_IRUGO);

static struct platform_device *drv_fpga_device;


static char *fpga_alias_name[FPGA_TOTAL_NUM] = {
    "sys_fpga",
};

static char *fpga_type_name[FPGA_TOTAL_NUM] = {
    "Xilinx",//Xilinx xc7a35tfgg484-2
};

/*
ssize_t drv_get_sw_version(char *buf)
{
    unsigned int ver;
    unsigned int cs;
    unsigned char val[2];
    int len, count=0;
    char *temp;

    temp = (char *) kzalloc(128 * FPGA_TOTAL_NUM, GFP_KERNEL);
    if(!temp)
    {
        FPGA_DEBUG("[%s]: Memory allocate failed.\n", __func__);
        return -ENOMEM;
    }

    for(cs=0; cs<FPGA_TOTAL_NUM; cs++)
    {
        fpga_pcie_read(cs, FPGA_VER_OFFSET, val);
        ver = (val[1] << 8) | val[0];
        len = sprintf_s(temp + count, 128 * FPGA_TOTAL_NUM, "%s version: 0x%04x\n", fpga_str[cs] , ver);
        count += len;
    }
    len = sprintf_s(buf, PAGE_SIZE, "%s\n", temp);

    kfree(temp);

    return len;
}
*/

ssize_t drv_get_reboot_cause(char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "NA\n");
}

ssize_t drv_get_alias(unsigned int index, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%s\n", fpga_alias_name[index]);
}

ssize_t drv_get_type(unsigned int index, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%s\n", fpga_type_name[index]);
}

ssize_t drv_get_hw_version(unsigned int index, char *buf)
{
    unsigned int val;
    int len;
    char *temp;

    temp = (char *) kzalloc(128 * FPGA_TOTAL_NUM, GFP_KERNEL);
    if(!temp)
    {
        FPGA_DEBUG("[%s]: Memory allocate failed.\n", __func__);
        return -ENOMEM;
    }


    val = fpga_pcie_read(FPGA_VER_OFFSET);
    len = sprintf_s(buf, PAGE_SIZE, "%x.%x\n", (val>>24)&0xff,(val>>16)&0xff);

    kfree(temp);

    return len;
}

ssize_t drv_get_board_version(unsigned int index, char *buf)
{
    unsigned int val;
    int len;
    char *temp;

    temp = (char *) kzalloc(128 * FPGA_TOTAL_NUM, GFP_KERNEL);
    if(!temp)
    {
        FPGA_DEBUG("[%s]: Memory allocate failed.\n", __func__);
        return -ENOMEM;
    }


    val = fpga_pcie_read(FPGA_VER_OFFSET);


    len = sprintf_s(buf, PAGE_SIZE, "%x\n", (val>>8)&0xff);

    kfree(temp);

    return len;
}

ssize_t drv_get_fmea_selftest_status(unsigned int index, char *buf)
{
    unsigned short test;
    unsigned short val;
    unsigned int offset = 0;

    test = 0x5555;
    offset = FPGA_TEST_REG_OFFSET;
    fpga_pcie_write(offset, test);
    val = fpga_pcie_read(offset);
    if(test != val)
        return sprintf_s(buf, PAGE_SIZE, "0x1 Selftest Failed, Reg(0x%04x) = 0x%04x\n", offset, val);
    test = 0xaaaa;
    offset = FPGA_TEST_REG_OFFSET;
    fpga_pcie_write(offset, test);
    val = fpga_pcie_read(offset);
    if(test != val)
        return sprintf_s(buf, PAGE_SIZE, "0x1 Selftest Failed, Reg(0x%04x) = 0x%04x\n", offset, val);
    return sprintf_s(buf, PAGE_SIZE, "0x0\n");
}

ssize_t drv_get_fmea_corrosion_status(unsigned int index, char *buf)
{
    unsigned short val = 0;
    unsigned short status = 0;
    unsigned short corrosion = 0;
    unsigned short vulcanization = 0;
    unsigned short wet_dust = 0;

    val = fpga_pcie_read(FPGA_REG_TCORROSION_DET);

    corrosion = (val & FPGA_MASK_CORROSION)>>FPGA_OFFSET_CORROSION;
    vulcanization = (val & FPGA_MASK_VULCANIZATION)>>FPGA_OFFSET_VULCANIZATION;
    wet_dust = (val & FPGA_MASK_WET_DUST)>>FPGA_OFFSET_WET_DUST;

    if(corrosion == CORROSION_ABNORMAL)
        status |= S3IP_FMEA_CORROSION;
    if(vulcanization == VULCANIZATION_ABNORMAL)
        status |= S3IP_FMEA_VULCANIZATION;
    if(wet_dust == WET_DUST_ABNORMAL)
        status |= S3IP_FMEA_WET_DUST;

    return sprintf_s(buf, PAGE_SIZE, "%d\n", status);
}

ssize_t drv_get_fmea_voltage_status(unsigned int index, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "NA\n");
}

ssize_t drv_get_fmea_clock_status(unsigned int index, char *buf)
{
    unsigned int val;
    int result;

    val = fpga_pcie_read(FPGA_CLK_MONITOR_LATCH_OFFSET);
    if(val != 0x0)
    {
        result = 1;
        fpga_pcie_read(FPGA_CLK0_ALARM_CLR_OFFSET);
        fpga_pcie_read(FPGA_CLK1_ALARM_CLR_OFFSET);
    }
    else
    {
        result = 0;
    }

    return sprintf_s(buf, PAGE_SIZE, "%d 0x%04x\n", result, val);
}

ssize_t drv_get_fmea_battery_status(char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "NA\n");
}

ssize_t drv_get_fmea_reset(char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "NA\n");
}

bool drv_set_fmea_reset(int reset)
{
    return true;
}

void drv_get_loglevel(long *lev)
{
    *lev = (long)loglevel;
    return;
}

void drv_set_loglevel(long lev)
{
    loglevel = (unsigned int)lev;
    return;
}

ssize_t drv_debug_help(char *buf)
{
    return sprintf_s(buf, PAGE_SIZE,
        "Use the following command to debug:\n"
        "busybox devmem 0xfbd00000 32\n");
}

ssize_t drv_debug(const char *buf, int count)
{
    return 0;
}


static struct fpga_drivers_t pfunc = {
    .get_alias = drv_get_alias,
    .get_type = drv_get_type,
    .get_hw_version = drv_get_hw_version,
    .get_board_version = drv_get_board_version,
    .get_fmea_selftest_status = drv_get_fmea_selftest_status,
    .get_fmea_corrosion_status = drv_get_fmea_corrosion_status,
    .get_fmea_voltage_status = drv_get_fmea_voltage_status,
    .get_fmea_clock_status = drv_get_fmea_clock_status,
    .get_fmea_battery_status = drv_get_fmea_battery_status,
    .get_fmea_reset = drv_get_fmea_reset,
    .set_fmea_reset = drv_set_fmea_reset,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
    .debug_help = drv_debug_help,
};

static int drv_fpga_probe(struct platform_device *pdev)
{
    s3ip_fpga_drivers_register(&pfunc);
    mxsonic_fpga_drivers_register(&pfunc);
    return 0;
}

static int drv_fpga_remove(struct platform_device *pdev)
{
    s3ip_fpga_drivers_unregister();
    mxsonic_fpga_drivers_unregister();
    return 0;
}

static struct platform_driver drv_fpga_driver = {
    .driver = {
        .name   = DRVNAME,
        .owner = THIS_MODULE,
    },
    .probe      = drv_fpga_probe,
    .remove     = drv_fpga_remove,
};

static int __init drv_fpga_init(void)
{
    int err;
    int retval;

    drv_fpga_device = platform_device_alloc(DRVNAME, 0);
    if(!drv_fpga_device)
    {
        FPGA_DEBUG("%s(#%d): platform_device_alloc fail\n", __func__, __LINE__);
        return -ENOMEM;
    }

    retval = platform_device_add(drv_fpga_device);
    if(retval)
    {
        FPGA_DEBUG("%s(#%d): platform_device_add failed\n", __func__, __LINE__);
        err = -retval;
        goto dev_add_failed;
    }

    retval = platform_driver_register(&drv_fpga_driver);
    if(retval)
    {
        FPGA_DEBUG("%s(#%d): platform_driver_register failed(%d)\n", __func__, __LINE__, err);
        err = -retval;
        goto dev_reg_failed;
    }

    return 0;

dev_reg_failed:
    platform_device_unregister(drv_fpga_device);
    return err;

dev_add_failed:
    platform_device_put(drv_fpga_device);
    return err;
}

static void __exit drv_fpga_exit(void)
{
    platform_driver_unregister(&drv_fpga_driver);
    platform_device_unregister(drv_fpga_device);

    return;
}

MODULE_AUTHOR("Weichen Chen <weichen_chen@accton.com>");
MODULE_DESCRIPTION("Huarong FPGA Driver");
MODULE_VERSION(SWITCH_FPGA_DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(drv_fpga_init);
module_exit(drv_fpga_exit);
