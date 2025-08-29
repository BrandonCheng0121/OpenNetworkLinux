#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>

#include "switch_led_driver.h"

#define SWITCH_MXSONIC_SYSLED_VERSION "0.0.0.1"

unsigned int loglevel = 0;

enum led_status {
    LED_DARK,
    LED_GREEN_ON,
    LED_YELLOW_ON,
    LED_RED_ON,
    LED_GREEN_FLASH,
    LED_YELLOW_FLASH,
    LED_RED_FLASH,
    LED_BLUE_ON,
    LED_BLUE_FLASH,
    LED_HW_CONTROL,
};
// 0: dark(灭)
// 1: green(绿色)
// 2: yellow(黄色)
// 3: red(红色)
// 4: green light flashing(绿色闪烁)
// 5: yellow light flashing(黄色闪烁)
// 6: red light flashing(红色闪烁)
// 7: blue(篮色)
// 8: blue light flashing(蓝色闪烁)

struct led_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct led_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct led_attribute *attr, const char *buf, size_t count);
};

struct led_drivers_t *cb_func = NULL;

/* For mxsonic */
extern struct kobject *mx_switch_kobj;
static struct kobject *sysled_kobj;

enum led_attrs {
    DEBUG,
    LOGLEVEL,
    SYS_LED_STATUS,
    // MST_LED_STATUS,
    BMC_LED_STATUS,
    // FAN_LED_STATUS,
    // PSU_LED_STATUS,
    ID_LED_STATUS,
    ACT_LED_STATUS,
    NUM_LED_ATTR,
};

static ssize_t mxsonic_debug_help(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->debug_help(buf);
}

static ssize_t mxsonic_debug(struct kobject *kobj, struct led_attribute *attr, const char *buf, size_t count)
{
    return count;
}

static ssize_t mxsonic_get_loglevel(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", loglevel);
}

static ssize_t mxsonic_set_loglevel(struct kobject *kobj, struct led_attribute *attr, const char *buf, size_t count)
{
    int retval;
    long lev;

    if(cb_func == NULL)
        return -1;

    retval = kstrtol(buf, 10, &lev);
    if(retval == 0)
    {
        LED_DEBUG("[%s] lev:%ld \n", __func__, lev);
    }
    else
    {
        LED_DEBUG("[%s] Error:%d, lev:%s \n", __func__, retval, buf);
        return -1;
    }

    loglevel = (unsigned int)lev;

    cb_func->set_loglevel(lev);

    return count;
}

static ssize_t mxsonic_get_sys_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    int retval;
    unsigned int led;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-ENXIO);
    }

    retval = cb_func->get_sys_led(&led);
    if(retval == false)
    {
        LED_DEBUG("[%s] Get sys led failed.\n", __func__);
        return -1;
    }

    switch(led)
    {
        case SYS_LED_GREEN_ON:
            led = LED_GREEN_ON;
            break;
        case SYS_LED_RED_ON:
            led = LED_RED_ON;
            break;
        case SYS_LED_GREEN_FLASH_FAST:
            led = LED_GREEN_FLASH;
            break;
        case SYS_LED_GREEN_FLASH_SLOW:
            led = LED_GREEN_FLASH;
            break;
        default:
            led = LED_HW_CONTROL;
            break;
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", led);
}

static ssize_t mxsonic_set_sys_led(struct kobject *obj, struct led_attribute *attr, const char *buf, size_t count)
{
    int retval;
    unsigned int led;

    if(cb_func == NULL)
        return -1;

    retval = kstrtoint(buf, 10, &led);
    if(retval == 0)
    {
        LED_DEBUG("[%s] led:%d \n", __func__, led);
    }
    else
    {
        LED_DEBUG("[%s] Error:%d, led:%s \n", __func__, retval, buf);
        return -1;
    }

    switch(led)
    {
        case LED_GREEN_ON:
            retval = cb_func->set_sys_led(SYS_LED_GREEN_ON);
            break;
        case LED_RED_ON:
            retval = cb_func->set_sys_led(SYS_LED_RED_ON);
            break;
        case LED_GREEN_FLASH:
            retval = cb_func->set_sys_led(SYS_LED_GREEN_FLASH_SLOW);
            break;
        case LED_HW_CONTROL:
            retval = cb_func->set_hw_control();
            break;
        default:
            break;
    }

    if(retval == false)
    {
        LED_DEBUG("[%s] Set sys led failed.\n", __func__);
        return -1;
    }

    return count;
}

static ssize_t mxsonic_get_mst_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    int retval;
    unsigned int led;

    if(cb_func == NULL)
        return -1;

    retval = cb_func->get_mst_led(&led);
    if(retval == false)
    {
        LED_DEBUG("[%s] Get mst led failed.\n", __func__);
        return -1;
    }

    switch(led)
    {
        case MST_LED_OFF:
            led = LED_DARK;
            break;
        case MST_LED_GREEN_ON:
            led = LED_GREEN_ON;
            break;
        case MST_LED_RED_ON:
            led = LED_RED_ON;
            break;
        default:
            led = LED_HW_CONTROL;
            break;
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", led);
}

static ssize_t mxsonic_set_mst_led(struct kobject *obj, struct led_attribute *attr, const char *buf, size_t count)
{
    int retval;
    unsigned int led;

    if(cb_func == NULL)
        return -1;

    retval = kstrtoint(buf, 10, &led);
    if(retval == 0)
    {
        LED_DEBUG("[%s] led:%d \n", __func__, led);
    }
    else
    {
        LED_DEBUG("[%s] Error:%d, led:%s \n", __func__, retval, buf);
        return -1;
    }

    switch(led)
    {
        case LED_DARK:
            retval = cb_func->set_mst_led(MST_LED_OFF);
            break;
        case LED_GREEN_ON:
            retval = cb_func->set_mst_led(MST_LED_GREEN_ON);
            break;
        case LED_RED_ON:
            retval = cb_func->set_mst_led(MST_LED_RED_ON);
            break;
        case LED_HW_CONTROL:
            retval = cb_func->set_hw_control();
            break;
        default:
            break;
    }

    if(retval == false)
    {
        LED_DEBUG("[%s] Set mst led failed.\n", __func__);
        return -1;
    }

    return count;
}

static ssize_t mxsonic_get_bmc_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    int retval;
    unsigned int led;

    if(cb_func == NULL)
        return -1;

    retval = cb_func->get_mst_led(&led);
    if(retval == false)
    {
        LED_DEBUG("[%s] Get mst led failed.\n", __func__);
        return -1;
    }

    switch(led)
    {
        case MST_LED_OFF:
            led = LED_DARK;
            break;
        case MST_LED_GREEN_ON:
            led = LED_GREEN_ON;
            break;
        case MST_LED_RED_ON:
            led = LED_RED_ON;
            break;
        case MST_LED_YELLOW_ON:
            led = LED_YELLOW_ON;
            break;
        case MST_LED_GREEN_FLASH:
            led = LED_GREEN_FLASH;
            break;
        default:
            led = LED_HW_CONTROL;
            break;
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", led);
}

static ssize_t mxsonic_get_fan_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    int retval;
    unsigned int led;

    if(cb_func == NULL)
        return -1;

    return sprintf_s(buf, PAGE_SIZE, "NA\n");
}


static ssize_t mxsonic_get_psu_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    int retval;
    unsigned int led;

    if(cb_func == NULL)
        return -1;

    return sprintf_s(buf, PAGE_SIZE, "NA\n");
}



static ssize_t mxsonic_get_id_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    int retval;
    unsigned int led;

    if(cb_func == NULL)
        return -1;

    retval = cb_func->get_id_led(&led, 0);
    if(retval == false)
    {
        LED_DEBUG("[%s] Get id led failed.\n", __func__);
        return -1;
    }

    switch(led)
    {
        case ID_LED_OFF:
            led = LED_DARK;
            break;
        case ID_LED_BLUE_ON:
            led = LED_BLUE_ON;
            break;
        default:
            led = LED_HW_CONTROL;
            break;
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", led);
}

static ssize_t mxsonic_set_id_led(struct kobject *obj, struct led_attribute *attr, const char *buf, size_t count)
{
    int retval;
    unsigned int led;

    if(cb_func == NULL)
        return -1;

    retval = kstrtoint(buf, 10, &led);
    if(retval == 0)
    {
        LED_DEBUG("[%s] led:%d \n", __func__, led);
    }
    else
    {
        LED_DEBUG("[%s] Error:%d, led:%s \n", __func__, retval, buf);
        return -1;
    }

    switch(led)
    {
        case LED_DARK:
            retval = cb_func->set_id_led(ID_LED_OFF, 0);
            break;
        case LED_BLUE_ON:
            retval = cb_func->set_id_led(ID_LED_BLUE_ON, 0);
            break;
        case LED_HW_CONTROL:
            retval = cb_func->set_hw_control();
            break;
        default:
            break;
    }

    if(retval == false)
    {
        LED_DEBUG("[%s] Set id led failed.\n", __func__);
        return -1;
    }

    return count;
}

static ssize_t mxsonic_get_act_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    int retval;
    unsigned int led;

    if(cb_func == NULL)
        return -1;

    retval = cb_func->get_act_led(&led);
    if(retval == false)
    {
        LED_DEBUG("[%s] Get act led failed.\n", __func__);
        return -1;
    }

    switch(led)
    {
        case ACT_LED_OFF:
            led = LED_DARK;
            break;
        case ACT_LED_GREEN_ON:
            led = LED_GREEN_ON;
            break;
        case ACT_LED_RED_ON:
            led = LED_RED_ON;
            break;
        case ACT_LED_GREEN_FLASH_FAST:
            led = LED_GREEN_FLASH;
            break;
        default:
            led = LED_HW_CONTROL;
            break;
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", led);
}

static ssize_t mxsonic_set_act_led(struct kobject *obj, struct led_attribute *attr, const char *buf, size_t count)
{
    int retval;
    unsigned int led;

    if(cb_func == NULL)
        return -1;

    retval = kstrtoint(buf, 10, &led);
    if(retval == 0)
    {
        LED_DEBUG("[%s] led:%d \n", __func__, led);
    }
    else
    {
        LED_DEBUG("[%s] Error:%d, led:%s \n", __func__, retval, buf);
        return -1;
    }

    switch(led)
    {
        case LED_DARK:
            retval = cb_func->set_act_led(ACT_LED_OFF);
            break;
        case LED_GREEN_ON:
            retval = cb_func->set_act_led(ACT_LED_GREEN_ON);
            break;
        case LED_RED_ON:
            retval = cb_func->set_act_led(ACT_LED_RED_ON);
            break;
        case LED_GREEN_FLASH:
            retval = cb_func->set_act_led(ACT_LED_GREEN_FLASH_FAST);
            break;
        case LED_HW_CONTROL:
            retval = cb_func->set_hw_control();
            break;
        default:
            LED_DEBUG("[%s] Not support this color led .\n", __func__);
            break;
    }

    if(retval == false)
    {
        LED_DEBUG("[%s] Set act led failed.\n", __func__);
        return -1;
    }

    return count;
}
static struct led_attribute led_attr[NUM_LED_ATTR] = {
    [DEBUG]                 = {{.name = "debug",                        .mode = S_IRUGO | S_IWUSR},             mxsonic_debug_help,             mxsonic_debug},
    [LOGLEVEL]              = {{.name = "loglevel",                     .mode = S_IRUGO | S_IWUSR},             mxsonic_get_loglevel,           mxsonic_set_loglevel},
    [SYS_LED_STATUS]        = {{.name = "sys_led_status",               .mode = S_IRUGO | S_IWUSR},             mxsonic_get_sys_led,            mxsonic_set_sys_led},
    [BMC_LED_STATUS]        = {{.name = "bmc_led_status",               .mode = S_IRUGO},                       mxsonic_get_bmc_led,            NULL},
    // [MST_LED_STATUS]        = {{.name = "mst_led_status",               .mode = S_IRUGO | S_IWUSR},             mxsonic_get_mst_led,            mxsonic_set_mst_led},
    // [FAN_LED_STATUS]        = {{.name = "fan_led_status",               .mode = S_IRUGO},                       mxsonic_get_fan_led,            NULL},
    // [PSU_LED_STATUS]        = {{.name = "psu_led_status",               .mode = S_IRUGO},                       mxsonic_get_psu_led,            NULL},
    [ID_LED_STATUS]         = {{.name = "id_led_status",                .mode = S_IRUGO | S_IWUSR},             mxsonic_get_id_led,             mxsonic_set_id_led},
    [ACT_LED_STATUS]        = {{.name = "act_led_status",               .mode = S_IRUGO | S_IWUSR},             mxsonic_get_act_led,            mxsonic_set_act_led},
};

void mxsonic_led_drivers_register(struct led_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_led_drivers_register);

void mxsonic_led_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_led_drivers_unregister);

static int __init switch_sysled_init(void)
{
    int err;
    int retval;
    int i;


    /* For MXSONiC */
    sysled_kobj = kobject_create_and_add("sysled", mx_switch_kobj);
    if(!sysled_kobj)
    {

        LED_DEBUG( "[%s]Failed to create 'sysled'\n", __func__);
        return -ENOMEM;
    }

    for(i=0; i < NUM_LED_ATTR; i++)
    {

        LED_DEBUG( "[%s]sysfs_create_file /sysled/%s\n", __func__, led_attr[i].attr.name);
        retval = sysfs_create_file(sysled_kobj, &led_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", led_attr[i].attr.name);
            err = -retval;
            goto sysfs_create_mxsonic_attr_failed;
        }
    }

    return 0;

sysfs_create_mxsonic_attr_failed:
    if(sysled_kobj)
    {
        kobject_put(sysled_kobj);
        sysled_kobj = NULL;
    }

    return err;
}

static void __exit switch_sysled_exit(void)
{
    int i;

    /* For MXSONiC */
    for(i=0; i < NUM_LED_ATTR; i++)
        sysfs_remove_file(sysled_kobj, &led_attr[i].attr);

    if(sysled_kobj)
    {
        kobject_put(sysled_kobj);
        sysled_kobj = NULL;
    }

    cb_func = NULL;

    return;
}

MODULE_DESCRIPTION("Huarong Switch MXSONiC SYSLED Driver");
MODULE_VERSION(SWITCH_MXSONIC_SYSLED_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_sysled_init);
module_exit(switch_sysled_exit);
