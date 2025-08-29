#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "switch_fan_driver.h"
#include "switch_led_driver.h"

#define SWITCH_MXSONIC_CPLD_VERSION "0.0.0.1"

unsigned int loglevel = 0;

struct fan_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct fan_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct fan_attribute *attr, const char *buf, size_t count);
};

struct fan_drivers_t *cb_func = NULL;

/* For MXSONiC */
extern struct kobject *mx_switch_kobj;
static struct kobject *fan_kobj;
static struct kobject *fan_index_kobj[MAX_FAN_NUM];
static struct kobject *motor_index_kobj[MAX_FAN_NUM][MAX_MOTOR_NUM];

enum fan_attrs {
    DEBUG,
    LOGLEVEL,
    NUM,
    MODEL_NAME,
    SERIAL_NUMBER,
    VENDOR,
    PART_NUMBER,
    HARDWARE_VERSION,
    NUM_MOTORS,
    STATUS,
    LED_STATUS,
    HI_DIRECTION,
    HI_RATIO,
    HI_SPEED,   
    NUM_FAN_ATTR,
};

enum motor_attrs {
    SPEED,
    SPEED_TOLERANCE,
    SPEED_TARGET,
    SPEED_MAX,
    SPEED_MIN,
    RATIO,
    DIRECTION,
    NUM_MOTOR_ATTR,
};

int get_fan_index(struct kobject *kobj)
{
    int retval;
    unsigned int fan_index;
    char fan_index_str[2] = {0};

    if(memcpy_s(fan_index_str, 2, (kobject_name(kobj)+3), 1) != 0)
    {
        return -ENOMEM;
    }
    retval = kstrtoint(fan_index_str, 10, &fan_index);
    if(retval == 0)
    {
        FAN_DEBUG("fan_index:%d \n", fan_index);
    }
    else
    {
        FAN_DEBUG("Error:%d, fan_index:%s \n", retval, fan_index_str);
        return -1;
    }

    return (fan_index-FAN_INDEX_START);
}

int get_fan_index_from_motor_kobj(struct kobject *kobj)
{
    int retval;
    unsigned int fan_index;
    char fan_index_str[2] = {0};

    if(memcpy_s(fan_index_str, 2, (kobject_name(kobj->parent)+3), 1) != 0)
    {
        return -ENOMEM;
    }
    retval = kstrtoint(fan_index_str, 10, &fan_index);
    if(retval == 0)
    {
        FAN_DEBUG("fan_index:%d \n", fan_index);
    }
    else
    {
        FAN_DEBUG("Error:%d, fan_index:%s \n", retval, fan_index_str);
        return -1;
    }

    return (fan_index-FAN_INDEX_START);
}

unsigned int get_motor_index(struct kobject *kobj)
{
    int retval;
    unsigned int motor_index;
    char motor_index_str[2] = {0};

    if(memcpy_s(motor_index_str, 2, (kobject_name(kobj)+5), 1) != 0)
    {
        return -ENOMEM;
    }
    retval = kstrtoint(motor_index_str, 10, &motor_index);
    if(retval == 0)
    {
        FAN_DEBUG("motor_index:%d \n", motor_index);
    }
    else
    {
        FAN_DEBUG("Error:%d, motor_index:%s \n", retval, motor_index_str);
        return -1;
    }

    return (motor_index-FAN_MOTOR_INDEX_START);
}

static ssize_t mxsonic_debug_help(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->debug_help(buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_debug(struct kobject *kobj, struct fan_attribute *attr, const char *buf, size_t count)
{
    return count;
}

static ssize_t mxsonic_get_loglevel(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    long lev;

    if(cb_func == NULL)
        return -1;

    cb_func->get_loglevel(&lev);

    return sprintf_s(buf, PAGE_SIZE, "%ld\n", lev);
}

static ssize_t mxsonic_set_loglevel(struct kobject *kobj, struct fan_attribute *attr, const char *buf, size_t count)
{
    int retval;
    long lev;

    if(cb_func == NULL)
        return -1;

    retval = kstrtol(buf, 0, &lev);
    if(retval == 0)
    {
        FAN_DEBUG("lev:%ld \n", lev);
    }
    else
    {
        FAN_DEBUG("Error:%d, lev:%s \n", retval, buf);
        return -1;
    }

    loglevel = (unsigned int)lev;

    cb_func->set_loglevel(lev);

    return count;
}

static ssize_t mxsonic_get_num(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", MAX_FAN_NUM);
}

static ssize_t mxsonic_get_model_name(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    int fan_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    fan_index = get_fan_index(kobj);
    if(fan_index < 0)
    {
        FAN_DEBUG("Get fan index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_model_name(fan_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_get_serial_number(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    int fan_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    fan_index = get_fan_index(kobj);
    if(fan_index < 0)
    {
        FAN_DEBUG("Get fan index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_sn(fan_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_get_vendor(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    int fan_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    fan_index = get_fan_index(kobj);
    if(fan_index < 0)
    {
        FAN_DEBUG("Get fan index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_vendor(fan_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_get_part_number(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    int fan_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    fan_index = get_fan_index(kobj);
    if(fan_index < 0)
    {
        FAN_DEBUG("Get fan index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_part_number(fan_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_get_hardware_version(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    int fan_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    fan_index = get_fan_index(kobj);
    if(fan_index < 0)
    {
        FAN_DEBUG("Get fan index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_hw_version(fan_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_get_num_motors(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", MAX_MOTOR_NUM);
}

extern bool drv_set_fan_led(unsigned int led);

static ssize_t mxsonic_get_status(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int fan_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    //get current fan status
    fan_index = get_fan_index(kobj);
    if(fan_index < 0)
    {
        FAN_DEBUG("Get fan index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    return cb_func->get_status(fan_index, buf);
}

static ssize_t mxsonic_get_led_status(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    unsigned int val;
    int fan_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    fan_index = get_fan_index(kobj);
    if(fan_index < 0)
    {
        FAN_DEBUG("Get fan index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_led_status(fan_index, &val);
    if(retval == false)
    {
        FAN_DEBUG("Get fan%d led status failed.\n", fan_index);
        ERROR_RETURN_NA(-1);
    }

    if(val == 0xFF)
        return sprintf_s(buf, PAGE_SIZE, "NA\n");
    else
        return sprintf_s(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t mxsonic_get_speed(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    int fan_index, motor_index;
    unsigned int speed;
    unsigned int speed0, speed1;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    if(strstr(kobject_name(kobj), "fan")) // /sys/switch/fan/fan*
    {
        fan_index = get_fan_index(kobj);
        if(fan_index < 0)
        {
            FAN_DEBUG("Get fan index failed.\n");
            ERROR_RETURN_NA(-EINVAL);
        }

        retval = cb_func->get_speed(fan_index, 0, &speed0);
        if(retval < 0)
        {
            FAN_DEBUG("Get fan%d motor0 speed failed.\n", fan_index);
            ERROR_RETURN_NA(-1);
        }

        retval = cb_func->get_speed(fan_index, 1, &speed1);
        if(retval < 0)
        {
            FAN_DEBUG("Get fan%d motor1 speed failed.\n", fan_index);
            ERROR_RETURN_NA(-1);
        }

        return sprintf_s(buf, PAGE_SIZE, "%d\n", ((speed0 + speed1) / 2));
    }
    else // /sys/switch/fan/fan*/motor/*
    {
        fan_index = get_fan_index_from_motor_kobj(kobj);
        if(fan_index < 0)
        {
            FAN_DEBUG("Get fan index failed.\n");
            ERROR_RETURN_NA(-EINVAL);
        }
        
        motor_index = get_motor_index(kobj);
        if(motor_index < 0)
        {
            FAN_DEBUG("Get motor index failed.\n");
            ERROR_RETURN_NA(-1);
        }
    
        retval = cb_func->get_speed(fan_index, motor_index, &speed);
        if(retval < 0)
        {
            FAN_DEBUG("Get fan%d motor%d speed failed.\n", fan_index, motor_index);
            ERROR_RETURN_NA(-1);
        }

        return sprintf_s(buf, PAGE_SIZE, "%d\n", speed);
    }
}

static ssize_t mxsonic_get_speed_tolerance(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    int fan_index, motor_index;
    unsigned int speed_tolerance;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    fan_index = get_fan_index_from_motor_kobj(kobj);
    if(fan_index < 0)
    {
        FAN_DEBUG("Get fan index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    motor_index = get_motor_index(kobj);
    if(motor_index < 0)
    {
        FAN_DEBUG("Get motor index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_speed_tolerance(fan_index, motor_index, &speed_tolerance);
    if(retval < 0)
    {
        FAN_DEBUG("Get fan%d motor%d speed tolerance failed.\n", fan_index, motor_index);
        ERROR_RETURN_NA(-1);
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", speed_tolerance);
}

static ssize_t mxsonic_get_speed_target(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    int fan_index, motor_index;
    unsigned int speed_target;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    fan_index = get_fan_index_from_motor_kobj(kobj);
    if(fan_index < 0)
    {
        FAN_DEBUG("Get fan index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    motor_index = get_motor_index(kobj);
    if(motor_index < 0)
    {
        FAN_DEBUG("Get motor index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_speed_target(fan_index, motor_index, &speed_target);
    if(retval < 0)
    {
        FAN_DEBUG("Get fan%d motor%d speed target failed.\n", fan_index, motor_index);
        ERROR_RETURN_NA(-1);
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", speed_target);
}


static ssize_t mxsonic_get_speed_max(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    unsigned int fan_index, motor_index;
    unsigned int speed_max;

    if(cb_func == NULL)
        return -1;

    fan_index = get_fan_index_from_motor_kobj(kobj);
    if(fan_index < 0)
    {
        FAN_DEBUG("Get fan index failed.\n");
        return -1;
    }

    motor_index = get_motor_index(kobj);
    if(motor_index < 0)
    {
        FAN_DEBUG("Get motor index failed.\n");
        return -1;
    }

    retval = cb_func->get_speed_max(fan_index, motor_index, &speed_max);
    if(retval < 0)
    {
        FAN_DEBUG("Get fan%d motor%d speed tolerance failed.\n", fan_index, motor_index);
        return -1;
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", speed_max);
}

static ssize_t mxsonic_get_speed_min(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    unsigned int fan_index, motor_index;
    unsigned int speed_min;

    if(cb_func == NULL)
        return -1;

    fan_index = get_fan_index_from_motor_kobj(kobj);
    if(fan_index < 0)
    {
        FAN_DEBUG("Get fan index failed.\n");
        return -1;
    }

    motor_index = get_motor_index(kobj);
    if(motor_index < 0)
    {
        FAN_DEBUG("Get motor index failed.\n");
        return -1;
    }

    retval = cb_func->get_speed_min(fan_index, motor_index, &speed_min);
    if(retval < 0)
    {
        FAN_DEBUG("Get fan%d motor%d speed tolerance failed.\n", fan_index, motor_index);
        return -1;
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", speed_min);
}


static ssize_t mxsonic_get_ratio(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    int fan_index;
    int pwm;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    if(strstr(kobject_name(kobj), "fan")) // /sys/switch/fan/fan*
    {
        fan_index = get_fan_index(kobj);
        if(fan_index < 0)
        {
            FAN_DEBUG("Get fan index failed.\n");
            ERROR_RETURN_NA(-EINVAL);
        }
    }
    else // /sys/switch/fan/fan*/motor/*
    {
        fan_index = get_fan_index_from_motor_kobj(kobj);
        if(fan_index < 0)
        {
            FAN_DEBUG("Get fan index failed.\n");
            ERROR_RETURN_NA(-EINVAL);
        }
    }

    retval = cb_func->get_pwm(fan_index, &pwm);
    if(retval == false)
    {
        FAN_DEBUG("Get fan %d pwm failed.\n", fan_index);
        ERROR_RETURN_NA(-1);
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", pwm);
}

static ssize_t mxsonic_set_ratio(struct kobject *kobj, struct fan_attribute *attr, const char *buf, size_t count)
{
    int retval;
    int fan_index;
    int pwm;

    if(cb_func == NULL)
    {
        return -EIO;
    }

    if(strstr(kobject_name(kobj), "fan")) // /sys/switch/fan/fan*
    {
        fan_index = get_fan_index(kobj);
        if(fan_index < 0)
        {
            FAN_DEBUG("Get fan index failed.\n");
            return -EINVAL;
        }
    }
    else // /sys/switch/fan/fan*/motor/*
    {
        fan_index = get_fan_index_from_motor_kobj(kobj);
        if(fan_index < 0)
        {
            FAN_DEBUG("Get fan index failed.\n");
            return -EINVAL;
        }
    }

    retval = kstrtoint(buf, 10, &pwm);
    if(retval == 0)
    {
        FAN_DEBUG("pwm:%d \n", pwm);
    }
    else
    {
        FAN_DEBUG("Error:%d, pwm:%s \n", retval, buf);
        return -EIO;
    }

    retval = cb_func->set_pwm(fan_index, pwm);
    if(retval == false)
    {
        FAN_DEBUG("Get fan %d pwm failed.\n", fan_index);
        return -EIO;
    }

    return count;
}

static ssize_t mxsonic_get_direction(struct kobject *kobj, struct fan_attribute *attr, char *buf)
{
    int retval;
    int fan_index;
    unsigned int direction;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    if(strstr(kobject_name(kobj), "fan")) // /sys/switch/fan/fan*
    {
        fan_index = get_fan_index(kobj);
        if(fan_index < 0)
        {
            FAN_DEBUG("Get fan index failed.\n");
            ERROR_RETURN_NA(-EINVAL);
        }
    }
    else // /sys/switch/fan/fan*/motor/*
    {
        fan_index = get_fan_index_from_motor_kobj(kobj);
        if(fan_index < 0)
        {
            FAN_DEBUG("Get fan index failed.\n");
            ERROR_RETURN_NA(-EINVAL);
        }
    }

    retval = cb_func->get_wind(fan_index, &direction);
    if(retval == false)
    {
        FAN_DEBUG("Get fan %d direction failed.\n", fan_index);
        ERROR_RETURN_NA(-EINVAL);
    }

    return sprintf_s(buf, PAGE_SIZE, "%d\n", direction);
}

static struct fan_attribute fan_attr[NUM_FAN_ATTR] = {
    [DEBUG]                     = {{.name = "debug",                    .mode = S_IRUGO | S_IWUSR},     mxsonic_debug_help,                     mxsonic_debug},
    [LOGLEVEL]                  = {{.name = "loglevel",                 .mode = S_IRUGO | S_IWUSR},     mxsonic_get_loglevel,                   mxsonic_set_loglevel},
    [NUM]                       = {{.name = "num",                      .mode = S_IRUGO},               mxsonic_get_num,                        NULL},
    [MODEL_NAME]                = {{.name = "model_name",               .mode = S_IRUGO},               mxsonic_get_model_name,                 NULL},
    [SERIAL_NUMBER]             = {{.name = "serial_number",            .mode = S_IRUGO},               mxsonic_get_serial_number,              NULL},
    [VENDOR]                    = {{.name = "vendor",                   .mode = S_IRUGO},               mxsonic_get_vendor,                     NULL},
    [PART_NUMBER]               = {{.name = "part_number",              .mode = S_IRUGO},               mxsonic_get_part_number,                NULL},
    [HARDWARE_VERSION]          = {{.name = "hardware_version",         .mode = S_IRUGO},               mxsonic_get_hardware_version,           NULL},
    [NUM_MOTORS]                = {{.name = "num_motors",               .mode = S_IRUGO},               mxsonic_get_num_motors,                 NULL},
    [STATUS]                    = {{.name = "status",                   .mode = S_IRUGO},               mxsonic_get_status,                     NULL},
    [LED_STATUS]                = {{.name = "led_status",               .mode = S_IRUGO},               mxsonic_get_led_status,                 NULL},
    [HI_DIRECTION]              = {{.name = "direction",                .mode = S_IRUGO},               mxsonic_get_direction,                  NULL},
    [HI_RATIO]                  = {{.name = "ratio",                    .mode = S_IRUGO | S_IWUSR},     mxsonic_get_ratio,                      mxsonic_set_ratio},
    [HI_SPEED]                  = {{.name = "speed",                    .mode = S_IRUGO},               mxsonic_get_speed,                      NULL},
};

static struct fan_attribute motor_attr[NUM_MOTOR_ATTR] = {
    [SPEED]                     = {{.name = "speed",                    .mode = S_IRUGO},               mxsonic_get_speed,                      NULL},
    [SPEED_TOLERANCE]           = {{.name = "speed_tolerance",          .mode = S_IRUGO},               mxsonic_get_speed_tolerance,            NULL},
    [SPEED_TARGET]              = {{.name = "speed_target",             .mode = S_IRUGO},               mxsonic_get_speed_target,               NULL},
    [SPEED_MAX]                 = {{.name = "speed_max",                .mode = S_IRUGO},               mxsonic_get_speed_max,                  NULL},
    [SPEED_MIN]                 = {{.name = "speed_min",                .mode = S_IRUGO},               mxsonic_get_speed_min,                  NULL},
    [RATIO]                     = {{.name = "ratio",                    .mode = S_IRUGO | S_IWUSR},     mxsonic_get_ratio,                      mxsonic_set_ratio},
    [DIRECTION]                 = {{.name = "direction",                .mode = S_IRUGO},               mxsonic_get_direction,                  NULL},
};

void mxsonic_fan_drivers_register(struct fan_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_fan_drivers_register);

void mxsonic_fan_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_fan_drivers_unregister);

static int __init switch_fan_init(void)
{
    int err;
    int retval;
    int i;
    int fan_index, motor_index;
    char *fan_index_str;
    char *motor_index_str;

    fan_index_str = (char *)kzalloc(8*sizeof(char), GFP_KERNEL);
    if (!fan_index_str)
    {
        FAN_DEBUG( "Fail to alloc fan_index_str memory\n");
        return -ENOMEM;
    }

    motor_index_str = (char *)kzalloc(8*sizeof(char), GFP_KERNEL);
    if (!motor_index_str)
    {
        FAN_DEBUG( "Fail to alloc motor_index_str memory\n");
        kfree(motor_index_str);
        return -ENOMEM;
    }

    /* For MXSONiC */
    fan_kobj = kobject_create_and_add("fan", mx_switch_kobj);
    if(!fan_kobj)
    {
        FAN_DEBUG( "Failed to create 'fan'\n");
        err = -ENOMEM;
        goto sysfs_create_kobject_fan_failed;
    }

    for(i=0; i<=NUM; i++)
    {
        FAN_DEBUG( "sysfs_create_file /fan/%s\n", fan_attr[i].attr.name);
        retval = sysfs_create_file(fan_kobj, &fan_attr[i].attr);
        if(retval)
        {
            FAN_DEBUG( "Failed to create file '%s'\n", fan_attr[i].attr.name);
            err = -retval;
            goto sysfs_create_mxsonic_fan_attr_failed;
        }
    }

    for(fan_index=0; fan_index<MAX_FAN_NUM; fan_index++)
    {
        if(sprintf_s(fan_index_str, 8, "fan%d", fan_index+FAN_INDEX_START) < 0)
        {
             FAN_DEBUG( "Failed to sprintf_s 'fan%d'\n", fan_index+1);
             err = -ENOMEM;
             goto sysfs_create_kobject_fan_index_failed;
        }
        fan_index_kobj[fan_index] = kobject_create_and_add(fan_index_str, fan_kobj);
        if(!fan_index_kobj[fan_index])
        {
            FAN_DEBUG( "Failed to create 'fan%d'\n", fan_index+FAN_INDEX_START);
            err = -ENOMEM;
            goto sysfs_create_kobject_fan_index_failed;
        }

        for(i=MODEL_NAME; i<NUM_FAN_ATTR; i++)
        {
            FAN_DEBUG( "sysfs_create_file /switch/fan/fan%d/%s\n", fan_index+FAN_INDEX_START, fan_attr[i].attr.name);

            retval = sysfs_create_file(fan_index_kobj[fan_index], &fan_attr[i].attr);
            if(retval)
            {
                printk(KERN_ERR "Failed to create file '%s'\n", fan_attr[i].attr.name);
                err = -retval;
                goto sysfs_create_mxsonic_fan_attrs_failed;
            }
        }

        for(motor_index=0; motor_index<MAX_MOTOR_NUM; motor_index++)
        {
            if(sprintf_s(motor_index_str, 8, "motor%d", motor_index+FAN_MOTOR_INDEX_START) < 0)
            {
                err = -ENOMEM;
                goto sysfs_create_kobject_switch_motor_index_failed;
            }
            motor_index_kobj[fan_index][motor_index] = kobject_create_and_add(motor_index_str, fan_index_kobj[fan_index]);
            if(!motor_index_kobj[fan_index][motor_index])
            {
                FAN_DEBUG( "Failed to create 'motor%d'\n", motor_index);
                err = -ENOMEM;
                goto sysfs_create_kobject_switch_motor_index_failed;
            }

            for(i=SPEED; i<NUM_MOTOR_ATTR; i++)
            {
                FAN_DEBUG( "sysfs_create_file /switch/fan/fan%d/%s/%s\n", fan_index+FAN_INDEX_START, motor_index_str, motor_attr[i].attr.name);

                retval = sysfs_create_file(motor_index_kobj[fan_index][motor_index], &motor_attr[i].attr);
                if(retval)
                {
                    printk(KERN_ERR "Failed to create file '%s'\n", motor_attr[i].attr.name);
                    err = -retval;
                    goto sysfs_create_mxsonic_motor_attrs_failed;
                }
            }
        }
    }

    kfree(fan_index_str);
    kfree(motor_index_str);

    return 0;

sysfs_create_mxsonic_motor_attrs_failed:
sysfs_create_kobject_switch_motor_index_failed:
sysfs_create_mxsonic_fan_attrs_failed:
sysfs_create_kobject_fan_index_failed:
    for(i=0; i<=NUM; i++)
        sysfs_remove_file(fan_kobj, &fan_attr[i].attr);

    for(fan_index=0; fan_index<MAX_FAN_NUM; fan_index++)
    {
        if(fan_index_kobj[fan_index])
        {
            for(i=MODEL_NAME; i < NUM_FAN_ATTR; i++)
                sysfs_remove_file(fan_index_kobj[fan_index], &fan_attr[i].attr);

            for(motor_index=0; motor_index<MAX_MOTOR_NUM; motor_index++)
            {
                if(motor_index_kobj[fan_index][motor_index])
                {
                    for(i=SPEED; i < NUM_MOTOR_ATTR; i++)
                        sysfs_remove_file(motor_index_kobj[fan_index][motor_index], &motor_attr[i].attr);
                }

                kobject_put(motor_index_kobj[fan_index][motor_index]);
                motor_index_kobj[fan_index][motor_index] = NULL;
            }
        }

        kobject_put(fan_index_kobj[fan_index]);
        fan_index_kobj[fan_index] = NULL;
    }

sysfs_create_mxsonic_fan_attr_failed:
    if(fan_kobj)
    {
        kobject_put(fan_kobj);
        fan_kobj = NULL;
    }

sysfs_create_kobject_fan_failed:
    kfree(fan_index_str);
    kfree(motor_index_str);

    return err;
}

static void __exit switch_fan_exit(void)
{
    int i;
    int fan_index;
    int motor_index;

    /* For MXSONiC */
    for(i=0; i<=NUM; i++)
        sysfs_remove_file(fan_kobj, &fan_attr[i].attr);

    for(fan_index=0; fan_index<MAX_FAN_NUM; fan_index++)
    {
        if(fan_index_kobj[fan_index])
        {
            for(i=MODEL_NAME; i < NUM_FAN_ATTR; i++)
                sysfs_remove_file(fan_index_kobj[fan_index], &fan_attr[i].attr);

            for(motor_index=0; motor_index<MAX_MOTOR_NUM; motor_index++)
            {
                if(motor_index_kobj[fan_index][motor_index])
                {
                    for(i=SPEED; i < NUM_MOTOR_ATTR; i++)
                        sysfs_remove_file(motor_index_kobj[fan_index][motor_index], &motor_attr[i].attr);

                    kobject_put(motor_index_kobj[fan_index][motor_index]);
                    motor_index_kobj[fan_index][motor_index] = NULL;
                }
            }

            kobject_put(fan_index_kobj[fan_index]);
            fan_index_kobj[fan_index] = NULL;
        }
    }

    if(fan_kobj)
    {
        kobject_put(fan_kobj);
        fan_kobj = NULL;
    }

    cb_func = NULL;

    return;
}

MODULE_DESCRIPTION("Huarong Switch MXSONiC Fan Driver");
MODULE_VERSION(SWITCH_MXSONIC_CPLD_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_fan_init);
module_exit(switch_fan_exit);