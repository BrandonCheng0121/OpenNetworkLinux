#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include "cpld_lpc_driver.h"
// #include "cpld_i2c_driver.h"
#include "switch_led_driver.h"

#define DRVNAME "drv_led_driver"
#define SWITCH_LED_DRIVER_VERSION "0.0.1"


unsigned int loglevel;
static struct platform_device *drv_led_device;

static int led_cpld_read(u8 reg)
{
    return lpc_read_cpld( LCP_DEVICE_ADDRESS1, reg);
}

static int led_cpld_write(u8 reg, u8 value)
{
    return lpc_write_cpld( LCP_DEVICE_ADDRESS1, reg, value);
}


bool drv_get_location_led(unsigned int *loc)
{
	return true;
}

bool drv_set_location_led(unsigned int loc)
{
	return true;
}

int drv_get_port_led_num(void)
{
	return true;
}

bool drv_get_sys_led(unsigned int *led)
{
    int led_value = 0;

    led_value = led_cpld_read(CPLD_SYSLED_REG);
    if(led_value < 0){
        LED_ERR("get cpld_read 0x%x\n",led_value);
        return -1;
    }
    *led = (led_value>>CPLD_SYSLED_OFFSET) & CPLD_SYSLED_REG_MASK;
	return true;
}

bool drv_set_sys_led(unsigned int led)
{
    int led_value = 0;
    int ret = 0;
    led_value = led_cpld_read(CPLD_SYSLED_REG);
    if(led_value < 0){
        LED_ERR("get cpld_read 0x%x\n",led_value);
        return -1;
    }
    led_value = led_value & ~(CPLD_SYSLED_REG_MASK<<CPLD_SYSLED_OFFSET);
    led_value = (u8)led_value | ((led&CPLD_SYSLED_REG_MASK) << CPLD_SYSLED_OFFSET);
    ret = led_cpld_write(CPLD_SYSLED_REG, led_value);
    if(ret < 0){
        LED_ERR("set cpld_read 0x%x\n",ret);
        return -1;
    }
    return true;
}

bool drv_get_mst_led(unsigned int *led)
{
    int led_value = 0;

    led_value = led_cpld_read(CPLD_MSTLED_REG);
    if(led_value < 0){
        LED_ERR("get cpld_read 0x%x\n",led_value);
        return -1;
    }
    *led = (led_value>>CPLD_MSTLED_OFFSET) & CPLD_MSTLED_REG_MASK;
    return true;
}

bool drv_set_mst_led(unsigned int led)
{
    int led_value = 0;
    int ret = 0;
    led_value = led_cpld_read(CPLD_MSTLED_REG);
    if(led_value < 0){
        LED_ERR("get cpld_read 0x%x\n",led_value);
        return -1;
    }
    led_value = led_value & ~(CPLD_MSTLED_REG_MASK<<CPLD_MSTLED_OFFSET);
    led_value = (u8)led_value | ((led&CPLD_MSTLED_REG_MASK) << CPLD_MSTLED_OFFSET);
    ret = led_cpld_write(CPLD_MSTLED_REG, led_value);
    if(ret < 0){
        LED_ERR("set cpld_read 0x%x\n",ret);
        return -1;
    }
    return true;
}

bool drv_get_id_led(unsigned int *led, unsigned int cs)
{
    int led_value = 0;

    led_value = led_cpld_read(CPLD_IDLED_REG);
    if(led_value < 0){
        LED_ERR("get cpld_read 0x%x\n",led_value);
        return -1;
    }
    *led = (led_value>>CPLD_IDLED_OFFSET) & CPLD_IDLED_REG_MASK;
    return true;
}

bool drv_set_id_led(unsigned int led, unsigned int cs)
{
    int led_value = 0;
    int ret = 0;
    led_value = led_cpld_read(CPLD_IDLED_REG);
    if(led_value < 0){
        LED_ERR("get cpld_read 0x%x\n",led_value);
        return -1;
    }
    led_value = led_value & ~(CPLD_IDLED_REG_MASK<<CPLD_IDLED_OFFSET);
    led_value = (u8)led_value | ((led&CPLD_IDLED_REG_MASK) << CPLD_IDLED_OFFSET);
    ret = led_cpld_write(CPLD_IDLED_REG, led_value);
    if(ret < 0){
        LED_ERR("set cpld_read 0x%x\n",ret);
        return -1;
    }
    return true;
}

bool drv_get_usb_led(unsigned int *led)
{
    int led_value = 0;

    led_value = led_cpld_read(CPLD_ACTLED_REG);
    if(led_value < 0){
        LED_ERR("get cpld_read 0x%x\n",led_value);
        return -1;
    }
    *led = (led_value>>CPLD_ACTLED_OFFSET) & CPLD_ACTLED_REG_MASK;
	return true;
}

bool drv_set_usb_led(unsigned int led)
{
    int led_value = 0;
    int ret = 0;
    led_value = led_cpld_read(CPLD_ACTLED_REG);
    if(led_value < 0){
        LED_ERR("get cpld_read 0x%x\n",led_value);
        return -1;
    }
    led_value = led_value & ~(CPLD_ACTLED_REG_MASK<<CPLD_ACTLED_OFFSET);
    led_value = (u8)led_value | ((led&CPLD_ACTLED_REG_MASK) << CPLD_ACTLED_OFFSET);
    ret = led_cpld_write(CPLD_ACTLED_REG, led_value);
    if(ret < 0){
        LED_ERR("set cpld_read 0x%x\n",ret);
        return -1;
    }
	return true;
}

bool drv_get_bmc_led(unsigned int *led)
{
    return true;
}

bool drv_set_bmc_led(unsigned int led)
{
    return true;
}

bool drv_get_fan_led(unsigned int *led)
{
    return true;
}

bool drv_set_fan_led(unsigned int led)
{
    return true;
}
EXPORT_SYMBOL_GPL(drv_set_fan_led);

bool drv_get_psu_led(unsigned int *led)
{
    return true;
}

bool drv_set_psu_led(unsigned int led)
{
    return true;
}
EXPORT_SYMBOL_GPL(drv_set_psu_led);

bool drv_set_hw_control(void)
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
        "i2cget -y -f <bus> <addr> <reg>; i2cset -y -f <bus> <addr> <reg> <val>\n"
        "sys_cpld: i2cget -y -f 157 0x62 <reg>; i2cset -y -f 157 0x62 <reg> <val>\n");
}

ssize_t drv_debug(const char *buf, int count)
{
	return 0;
}

static struct led_drivers_t pfunc = {
    .get_sys_led = drv_get_sys_led,
    .set_sys_led = drv_set_sys_led,
    .get_mst_led = drv_get_mst_led,
    .set_mst_led = drv_set_mst_led,
    .get_bmc_led = drv_get_bmc_led,
    .get_fan_led = drv_get_fan_led,
    .set_fan_led = drv_set_fan_led,
    .get_psu_led = drv_get_psu_led,
    .set_psu_led = drv_set_psu_led,
    .get_id_led = drv_get_id_led,
    .set_id_led = drv_set_id_led,
    .get_act_led = drv_get_usb_led,
    .set_act_led = drv_set_usb_led,
    .set_hw_control = drv_set_hw_control,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
    .debug_help = drv_debug_help,
};

static int drv_led_probe(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    s3ip_led_drivers_register(&pfunc);
    mxsonic_led_drivers_register(&pfunc);
#else
    hisonic_led_drivers_register(&pfunc);
    kssonic_led_drivers_register(&pfunc);
#endif

    return 0;
}

static int drv_led_remove(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    s3ip_led_drivers_unregister();
    mxsonic_led_drivers_unregister();
#else
    hisonic_led_drivers_unregister();
    kssonic_led_drivers_unregister();
#endif

    return 0;
}

static struct platform_driver drv_led_driver = {
    .driver = {
        .name   = DRVNAME,
        .owner = THIS_MODULE,
    },
    .probe      = drv_led_probe,
    .remove     = drv_led_remove,
};

static int __init drv_led_init(void)
{
    int err;
    int retval;

    drv_led_device = platform_device_alloc(DRVNAME, 0);
    if(!drv_led_device)
    {
        LED_DEBUG("%s(#%d): platform_device_alloc fail\n", __func__, __LINE__);
        return -ENOMEM;
    }

    retval = platform_device_add(drv_led_device);
    if(retval)
    {
        LED_DEBUG("%s(#%d): platform_device_add failed\n", __func__, __LINE__);
        err = -retval;
        goto dev_add_failed;
    }

    retval = platform_driver_register(&drv_led_driver);
    if(retval)
    {
        LED_DEBUG("%s(#%d): platform_driver_register failed(%d)\n", __func__, __LINE__, err);
        err = -retval;
        goto dev_reg_failed;
    }

    return 0;

dev_reg_failed:
    platform_device_unregister(drv_led_device);
    return err;
dev_add_failed:
    platform_device_put(drv_led_device);
    return err;
}

static void __exit drv_led_exit(void)
{
    platform_driver_unregister(&drv_led_driver);
    platform_device_unregister(drv_led_device);

    return;
}

MODULE_DESCRIPTION("Huarong LED Driver");
MODULE_VERSION(SWITCH_LED_DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(drv_led_init);
module_exit(drv_led_exit);
