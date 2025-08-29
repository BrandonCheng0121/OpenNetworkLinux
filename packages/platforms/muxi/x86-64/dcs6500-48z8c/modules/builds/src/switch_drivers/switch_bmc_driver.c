#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#include "cpld_lpc_driver.h"
#include "cpld_i2c_driver.h"
#include "switch_bmc_driver.h"

#define DRVNAME "drv_bmc_driver"
#define SWITCH_BMC_DRIVER_VERSION "0.0.1"

unsigned int loglevel = 0;
static struct platform_device *drv_bmc_device;

struct mutex     update_lock;

static int sys_cpld_read(u8 reg)
{
    return lpc_read_cpld( LCP_DEVICE_ADDRESS1, reg);
}

ssize_t drv_get_status(char *buf)
{
    int val;
    int bmc_present = -1;
    int bmc_heart = -1;

    val = sys_cpld_read(SYS_CPLD_BMC_STATUS_REG);
    if(val < 0)
        return -1;

    bmc_present = (val >> BMC_PRESENT_OFFSET) & BMC_PRESENT_MASK;
    bmc_heart =  (val >> BMC_HEART_OFFSET) & BMC_HEART_MASK;

    if(bmc_present == BMC_NOT_PRESENT) //BMC is not present
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", S3IP_BMC_STATUS_ABSENT);
    }
    else if(bmc_present == BMC_PRESENT)
    {
        if(bmc_heart == BMC_HEART_FRQ_2HZ)
            return sprintf_s(buf, PAGE_SIZE, "%d\n", S3IP_BMC_STATUS_OK);
        else
            return sprintf_s(buf, PAGE_SIZE, "%d\n", S3IP_BMC_STATUS_ERR);
    }

    return -1;
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

ssize_t drv_get_enable(char *buf)
{
    int val;
    int bmc_enable = -1;
    int bmc_present = -1;
    int bmc_heart = -1;

    val = sys_cpld_read(SYS_CPLD_BMC_EN_REG);
    if(val < 0)
        return -1;
    bmc_enable = (val >> BMC_ENABLE_OFFSET) & BMC_ENABLE_MASK;

    val = sys_cpld_read(SYS_CPLD_BMC_STATUS_REG);
    if(val < 0)
        return -1;
    bmc_present = (val >> BMC_PRESENT_OFFSET) & BMC_PRESENT_MASK;
    bmc_heart =  (val >> BMC_HEART_OFFSET) & BMC_HEART_MASK;

    if((bmc_enable == BMC_EN_ENABLE) && (bmc_present == BMC_PRESENT) && (bmc_heart == BMC_HEART_FRQ_2HZ))
    {
        return sprintf_s(buf, PAGE_SIZE, "1\n");
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "0\n");
    }

    return -1;
}

void drv_set_enable(long enable)
{
    return;
}

static struct bmc_drivers_t pfunc = {
    .get_status = drv_get_status,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
    .get_enable = drv_get_enable,
    .set_enable = drv_set_enable,
};

static int drv_bmc_probe(struct platform_device *pdev)
{
    mxsonic_bmc_drivers_register(&pfunc);

    return 0;
}

static int drv_bmc_remove(struct platform_device *pdev)
{
    mxsonic_bmc_drivers_unregister();

    return 0;
}

static struct platform_driver drv_bmc_driver = {
    .driver = {
        .name   = DRVNAME,
        .owner = THIS_MODULE,
    },
    .probe      = drv_bmc_probe,
    .remove     = drv_bmc_remove,
};

static int __init drv_bmc_init(void)
{
    int err;
    int retval;

    drv_bmc_device = platform_device_alloc(DRVNAME, 0);
    if(!drv_bmc_device)
    {
        BMC_DEBUG("%s(#%d): platform_device_alloc fail\n", __func__, __LINE__);
        return -ENOMEM;
    }

    retval = platform_device_add(drv_bmc_device);
    if(retval)
    {
        BMC_DEBUG("%s(#%d): platform_device_add failed\n", __func__, __LINE__);
        err = -retval;
        goto dev_add_failed;
    }

    retval = platform_driver_register(&drv_bmc_driver);
    if(retval)
    {
        BMC_DEBUG("%s(#%d): platform_driver_register failed(%d)\n", __func__, __LINE__, err);
        err = -retval;
        goto dev_reg_failed;
    }

    mutex_init(&update_lock);

    return 0;

dev_reg_failed:
    platform_device_unregister(drv_bmc_device);
    return err;

dev_add_failed:
    platform_device_put(drv_bmc_device);
    return err;
}

static void __exit drv_bmc_exit(void)
{
    platform_driver_unregister(&drv_bmc_driver);
    platform_device_unregister(drv_bmc_device);

    mutex_destroy(&update_lock);

    return;
}

MODULE_DESCRIPTION("S3IP BMC Driver");
MODULE_VERSION(SWITCH_BMC_DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(drv_bmc_init);
module_exit(drv_bmc_exit);
