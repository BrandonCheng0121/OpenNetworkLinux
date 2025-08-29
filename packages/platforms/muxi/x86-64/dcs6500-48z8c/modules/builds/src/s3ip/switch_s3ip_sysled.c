#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>

#include "switch_led_driver.h"

#define SWITCH_S3IP_SYSLED_VERSION "0.0.0.1"

unsigned int loglevel = 0;

enum led_status {
    LED_DARK = 0,
    LED_GREEN_ON = 1,
    LED_YELLOW_ON = 2,
    LED_RED_ON = 3,
    LED_BLUE_ON = 4,

    LED_GREEN_FLASH_SLOW = 5,
    LED_YELLOW_FLASH_SLOW = 6,
    LED_RED_FLASH_SLOW = 7,
    LED_BLUE_FLASH_SLOW = 8,

    LED_GREEN_FLASH_FAST = 9,
    LED_YELLOW_FLASH_FAST = 10,
    LED_RED_FLASH_FAST = 11,
    LED_BLUE_FLASH_FAST = 12,
    LED_HW_CONTROL,
};

struct led_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct led_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct led_attribute *attr, const char *buf, size_t count);
    u8 index;
};

struct led_drivers_t *cb_func = NULL;

/* For s3ip */
extern struct kobject *switch_kobj;
static struct kobject *sysled_kobj;

enum led_attrs {
    SYS_LED_STATUS,
    MST_LED_STATUS,
    ID_LED_STATUS,
    ACT_LED_STATUS,
    NUM_LED_ATTR,
};

static ssize_t s3ip_debug_help(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->debug_help(buf);
}

static ssize_t s3ip_get_loglevel(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", loglevel);
}

static ssize_t s3ip_set_loglevel(struct kobject *kobj, struct led_attribute *attr, const char *buf, size_t count)
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

static ssize_t s3ip_get_sys_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    int retval;
    unsigned int led;

    if(cb_func == NULL)
        return -1;

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
            led = LED_GREEN_FLASH_FAST;
            break;
        case SYS_LED_GREEN_FLASH_SLOW:
            led = LED_GREEN_FLASH_SLOW;
            break;
        default:
            led = LED_HW_CONTROL;
            break;
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", led);
}

static ssize_t s3ip_set_sys_led(struct kobject *obj, struct led_attribute *attr, const char *buf, size_t count)
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
        case LED_GREEN_FLASH_FAST:
            retval = cb_func->set_sys_led(SYS_LED_GREEN_FLASH_FAST);
            break;
        case LED_GREEN_FLASH_SLOW:
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

static ssize_t s3ip_get_mst_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
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

static ssize_t s3ip_set_mst_led(struct kobject *obj, struct led_attribute *attr, const char *buf, size_t count)
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

static ssize_t s3ip_get_fan_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    unsigned int led;
    led = LED_HW_CONTROL;
    return sprintf_s(buf, PAGE_SIZE, "%d\n", led);
}

static ssize_t s3ip_get_psu_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
{
    unsigned int led;
    led = LED_HW_CONTROL;
    return sprintf_s(buf, PAGE_SIZE, "%d\n", led);
}

static ssize_t s3ip_get_id_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
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

static ssize_t s3ip_set_id_led(struct kobject *obj, struct led_attribute *attr, const char *buf, size_t count)
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

static ssize_t s3ip_get_act_led(struct kobject *kobj, struct led_attribute *attr, char *buf)
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
            led = LED_GREEN_FLASH_FAST;
            break;
        default:
            led = LED_HW_CONTROL;
            break;
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", led);
}

static ssize_t s3ip_set_act_led(struct kobject *obj, struct led_attribute *attr, const char *buf, size_t count)
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
        case LED_GREEN_FLASH_FAST:
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
    [SYS_LED_STATUS] = {{.name = "sys_led_status",  .mode = S_IRUGO | S_IWUSR}, s3ip_get_sys_led, s3ip_set_sys_led},
    [MST_LED_STATUS] = {{.name = "bmc_led_status",  .mode = S_IRUGO | S_IWUSR}, s3ip_get_mst_led, s3ip_set_mst_led},
    [ID_LED_STATUS]  = {{.name = "id_led_status",   .mode = S_IRUGO | S_IWUSR}, s3ip_get_id_led,  s3ip_set_id_led},
    [ACT_LED_STATUS] = {{.name = "act_led_status",  .mode = S_IRUGO | S_IWUSR}, s3ip_get_act_led, s3ip_set_act_led},
};

void s3ip_led_drivers_register(struct led_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(s3ip_led_drivers_register);

void s3ip_led_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(s3ip_led_drivers_unregister);

static int __init switch_sysled_init(void)
{
    int err;
    int retval;
    int i,num;
    char tmp_name[32];

    /* For S3IP */
    sysled_kobj = kobject_create_and_add("sysled", switch_kobj);
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
            goto sysfs_create_s3ip_attr_failed;
        }
    }

    return 0;

sysfs_create_s3ip_attr_failed:
    if(sysled_kobj)
    {
        kobject_put(sysled_kobj);
        sysled_kobj = NULL;
    }

    return err;
}

static void __exit switch_sysled_exit(void)
{
    int i,num;
    char tmp_name[32];
    /* For S3IP */
    for(i=0; i < NUM_LED_ATTR; i++){
        sysfs_remove_file(sysled_kobj, &led_attr[i].attr);
    }
    if(sysled_kobj)
    {
        kobject_put(sysled_kobj);
        sysled_kobj = NULL;
    }

    cb_func = NULL;

    return;
}

MODULE_AUTHOR("Weichen Chen <weichen_chen@accton.com>");
MODULE_DESCRIPTION("Huarong Switch S3IP SYSLED Driver");
MODULE_VERSION(SWITCH_S3IP_SYSLED_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_sysled_init);
module_exit(switch_sysled_exit);
