#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "switch_sensor_driver.h"

#define SWITCH_S3IP_SENSOR_VERSION "0.0.0.1"

unsigned int loglevel = 0;
struct sensor_drivers_t *cb_func = NULL;
static int multiplier = 1000;

struct sensor_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct sensor_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct sensor_attribute *attr, const char *buf, size_t count);
};

/* For S3IP */
extern struct kobject *switch_kobj;
static struct kobject *sensor_kobj[TOTAL_SENSORS];
static struct kobject *temp_index_kobj[TEMP_TOTAL_NUM];
static struct kobject *in_index_kobj[IN_TOTAL_NUM];
static struct kobject *curr_index_kobj[CURR_TOTAL_NUM];

enum sensor_attrs {
    NUM_TEMP_SENSORS,
    NUM_IN_SENSORS,
    NUM_CURR_SENSORS,
    TEMP_ALIAS,
    TEMP_TYPE,
    TEMP_MAX,
    TEMP_CRIT,
    TEMP_MIN,
    TEMP_INPUT,
    IN_ALIAS,
    IN_TYPE,
    IN_MAX,
    IN_MIN,
    IN_INPUT,
    IN_RANGE,
    IN_ALARM,
    CURR_ALIAS,
    CURR_TYPE,
    CURR_MAX,
    CURR_MIN,
    CURR_INPUT,
    NUM_SENSOR_ATTR,
};

int get_temp_index(struct kobject *kobj)
{
    int retval;
    unsigned int temp_index;
    char temp_index_str[2] = {0};
    
    memcpy_s(temp_index_str, 2, (kobject_name(kobj)+4), 1);
    retval = kstrtoint(temp_index_str, 10, &temp_index);
    if(retval == 0)
    {
        SENSOR_DEBUG("temp_index:%d \n", temp_index);
    }
    else
    {
        SENSOR_DEBUG("Error:%d, temp_index:%s \n", retval, temp_index_str);
        return -1;
    }

    return (temp_index-SENSORS_INDEX_START);
}

int get_in_index(struct kobject *kobj)
{
    int retval;
    unsigned int in_index;
    char in_index_str[3] = {0};

    memcpy_s(in_index_str, 2, (kobject_name(kobj)+3), strlen(kobject_name(kobj))-3);
    retval = kstrtoint(in_index_str, 10, &in_index);
    if(retval == 0)
    {
        SENSOR_DEBUG("in_index:%d \n", in_index);
    }
    else
    {
        SENSOR_DEBUG("Error:%d, in_index:%s \n", retval, in_index_str);
        return -1;
    }

    return (in_index-SENSORS_INDEX_START);
}

int get_curr_index(struct kobject *kobj)
{
    int retval;
    unsigned int curr_index;
    char curr_index_str[2] = {0};

    memcpy_s(curr_index_str, 2, (kobject_name(kobj)+4), 1);
    retval = kstrtoint(curr_index_str, 10, &curr_index);
    if(retval == 0)
    {
        SENSOR_DEBUG("curr_index:%d \n", curr_index);
    }
    else
    {
        SENSOR_DEBUG("Error:%d, curr_index:%s \n", retval, curr_index_str);
        return -1;
    }

    return (curr_index-SENSORS_INDEX_START);
}

static ssize_t s3ip_debug_help(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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

static ssize_t s3ip_debug(struct kobject *kobj, struct sensor_attribute *attr, const char *buf, size_t count)
{
    return count;
}

static ssize_t s3ip_get_loglevel(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    long lev;

    if(cb_func == NULL)
        return -1;

    cb_func->get_loglevel(&lev);

    return sprintf_s(buf, PAGE_SIZE, "%ld\n", lev);
}

static ssize_t s3ip_set_loglevel(struct kobject *kobj, struct sensor_attribute *attr, const char *buf, size_t count)
{
    int retval;
    long lev;

    if(cb_func == NULL)
        return -1;

    retval = kstrtol(buf, 0, &lev);
    if(retval == 0)
    {
        SENSOR_DEBUG("lev:%ld \n", lev);
    }
    else
    {
        SENSOR_DEBUG("Error:%d, lev:%s \n", retval, buf);
        return -1;
    }

    loglevel = (unsigned int)lev;

    cb_func->set_loglevel(lev);

    return count;
}

static ssize_t s3ip_get_temp_num(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", TEMP_TOTAL_NUM);
}

static ssize_t s3ip_get_in_num(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", IN_TOTAL_NUM);
}

static ssize_t s3ip_get_curr_num(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", CURR_TOTAL_NUM);
}

static ssize_t s3ip_get_temp_alias(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int temp_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    temp_index = get_temp_index(kobj);
    if(temp_index < 0)
    {
        SENSOR_DEBUG("Get temp index failed.\n");
        return -1;
    }

    retval = cb_func->get_temp_alias(temp_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t s3ip_get_temp_type(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int temp_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    temp_index = get_temp_index(kobj);
    if(temp_index < 0)
    {
        SENSOR_DEBUG("Get temp index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_temp_type(temp_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t s3ip_get_temp_max(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int temp_index;
    long temp_max;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    temp_index = get_temp_index(kobj);
    if(temp_index < 0)
    {
        SENSOR_DEBUG("Get temp index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_temp_max(temp_index, &temp_max);
    if(retval == false)
    {
        SENSOR_DEBUG("Get temp max failed.\n");
        ERROR_RETURN_NA(-1);
    }
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", temp_max);
}

static ssize_t s3ip_get_temp_max_hyst(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int temp_index;
    long temp_max_hyst;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    temp_index = get_temp_index(kobj);
    if(temp_index < 0)
    {
        SENSOR_DEBUG("Get temp index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_temp_max_hyst(temp_index, &temp_max_hyst);
    if(retval == false)
    {
        SENSOR_DEBUG("Get temp max hyst failed.\n");
        ERROR_RETURN_NA(-1);
    }
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", temp_max_hyst);
}

static ssize_t s3ip_get_temp_min(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int temp_index;
    long temp_min;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    temp_index = get_temp_index(kobj);
    if(temp_index < 0)
    {
        SENSOR_DEBUG("Get temp index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_temp_min(temp_index, &temp_min);
    if(retval == false)
    {
        SENSOR_DEBUG("Get temp min failed.\n");
        ERROR_RETURN_NA(-1);
    }
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", temp_min);
}

static ssize_t s3ip_get_temp_input(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int temp_index;
    long temp_input;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    temp_index = get_temp_index(kobj);
    if(temp_index < 0)
    {
        SENSOR_DEBUG("Get temp index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_temp_input(temp_index, &temp_input);
    if(retval == false)
    {
        SENSOR_DEBUG("Get temp input failed.\n");
        ERROR_RETURN_NA(-1);
    }
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", temp_input);
}

static ssize_t s3ip_get_in_alias(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int in_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    in_index = get_in_index(kobj);
    if(in_index < 0)
    {
        SENSOR_DEBUG("Get in index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_in_alias(in_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t s3ip_get_in_type(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int in_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    in_index = get_in_index(kobj);
    if(in_index < 0)
    {
        SENSOR_DEBUG("Get in index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_in_type(in_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t s3ip_get_in_max(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int in_index;
    long in_max;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    in_index = get_in_index(kobj);
    if(in_index < 0)
    {
        SENSOR_DEBUG("Get in index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_in_max(in_index, &in_max);
    if(retval == false)
    {
        SENSOR_DEBUG("Get in max failed.\n");
        ERROR_RETURN_NA(-1);
    }
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", in_max);
}

static ssize_t s3ip_get_in_min(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int in_index;
    long in_min;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    in_index = get_in_index(kobj);
    if(in_index < 0)
    {
        SENSOR_DEBUG("Get in index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_in_min(in_index, &in_min);
    if(retval == false)
    {
        SENSOR_DEBUG("Get in min failed.\n");
        ERROR_RETURN_NA(-1);
    }
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", in_min);
}

static ssize_t s3ip_get_in_input(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int in_index;
    long in_input;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    in_index = get_in_index(kobj);
    if(in_index < 0)
    {
        SENSOR_DEBUG("Get in index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_in_input(in_index, &in_input);
    if(retval == false)
    {
        SENSOR_DEBUG("Get in input failed.\n");
        ERROR_RETURN_NA(-1);
    }
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", in_input);
}

static ssize_t s3ip_get_in_range(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    ERROR_RETURN_NA(0);
}

static ssize_t s3ip_get_in_alarm(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    unsigned int in_index;
    long in_alarm;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    in_index = get_in_index(kobj);
    if(in_index < 0)
    {
        SENSOR_DEBUG("Get in index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_in_alarm(in_index, &in_alarm);
    if(retval == false)
    {
        SENSOR_DEBUG("Get in alarm failed.\n");
        ERROR_RETURN_NA(-1);
    }
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", in_alarm);
}

static ssize_t s3ip_get_curr_alias(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int curr_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    curr_index = get_curr_index(kobj);
    if(curr_index < 0)
    {
        SENSOR_DEBUG("Get curr index failed.\n");
        return -1;
    }

    retval = cb_func->get_curr_alias(curr_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t s3ip_get_curr_type(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int curr_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    curr_index = get_curr_index(kobj);
    if(curr_index < 0)
    {
        SENSOR_DEBUG("Get curr index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_curr_type(curr_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t s3ip_get_curr_max(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int curr_index;
    long curr_max;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    curr_index = get_curr_index(kobj);
    if(curr_index < 0)
    {
        SENSOR_DEBUG("Get curr index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_curr_max(curr_index, &curr_max);
    if(retval == false)
    {
        SENSOR_DEBUG("Get curr max failed.\n");
        ERROR_RETURN_NA(-1);
    }
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", curr_max);
}

static ssize_t s3ip_get_curr_min(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int curr_index;
    long curr_min;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    curr_index = get_curr_index(kobj);
    if(curr_index < 0)
    {
        SENSOR_DEBUG("Get curr index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_curr_min(curr_index, &curr_min);
    if(retval == false)
    {
        SENSOR_DEBUG("Get curr min failed.\n");
        ERROR_RETURN_NA(-1);
    }
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", curr_min);
}

static ssize_t s3ip_get_curr_input(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int curr_index;
    long curr_input;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    curr_index = get_curr_index(kobj);
    if(curr_index < 0)
    {
        SENSOR_DEBUG("Get curr index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_curr_input(curr_index, &curr_input);
    if(retval == false)
    {
        SENSOR_DEBUG("Get curr input failed.\n");
        ERROR_RETURN_NA(-1);
    }
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", curr_input);
}

static ssize_t s3ip_get_curr_average(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    ERROR_RETURN_NA(0);
}

static struct sensor_attribute sensor_attr[NUM_SENSOR_ATTR] = {
    [NUM_TEMP_SENSORS]      = {{.name = "number",           .mode = S_IRUGO},                       s3ip_get_temp_num,           NULL},
    [NUM_IN_SENSORS]        = {{.name = "number",           .mode = S_IRUGO},                       s3ip_get_in_num,             NULL},
    [NUM_CURR_SENSORS]      = {{.name = "number",           .mode = S_IRUGO},                       s3ip_get_curr_num,           NULL},
    [TEMP_ALIAS]            = {{.name = "alias",            .mode = S_IRUGO},                       s3ip_get_temp_alias,         NULL},
    [TEMP_TYPE]             = {{.name = "type",             .mode = S_IRUGO},                       s3ip_get_temp_type,          NULL},
    [TEMP_MAX]              = {{.name = "max",              .mode = S_IRUGO | S_IWUSR},             s3ip_get_temp_max_hyst,      NULL},
    [TEMP_CRIT]             = {{.name = "crit",             .mode = S_IRUGO | S_IWUSR},             s3ip_get_temp_max,           NULL},
    [TEMP_MIN]              = {{.name = "min",              .mode = S_IRUGO | S_IWUSR},             s3ip_get_temp_min,           NULL},
    [TEMP_INPUT]            = {{.name = "value",            .mode = S_IRUGO},                       s3ip_get_temp_input,         NULL}, 
    [IN_ALIAS]              = {{.name = "alias",            .mode = S_IRUGO},                       s3ip_get_in_alias,           NULL},
    [IN_TYPE]               = {{.name = "type",             .mode = S_IRUGO},                       s3ip_get_in_type,            NULL},
    [IN_MAX]                = {{.name = "max",              .mode = S_IRUGO | S_IWUSR},             s3ip_get_in_max,             NULL},
    [IN_MIN]                = {{.name = "min",              .mode = S_IRUGO | S_IWUSR},             s3ip_get_in_min,             NULL},
    [IN_INPUT]              = {{.name = "value",            .mode = S_IRUGO},                       s3ip_get_in_input,           NULL},
    [IN_RANGE]              = {{.name = "range",            .mode = S_IRUGO},                       s3ip_get_in_range,           NULL},
    [IN_ALARM]              = {{.name = "nominal_value",    .mode = S_IRUGO},                       s3ip_get_in_alarm,           NULL},
    [CURR_ALIAS]            = {{.name = "alias",            .mode = S_IRUGO},                       s3ip_get_curr_alias,         NULL},
    [CURR_TYPE]             = {{.name = "type",             .mode = S_IRUGO},                       s3ip_get_curr_type,          NULL},
    [CURR_MAX]              = {{.name = "max",              .mode = S_IRUGO | S_IWUSR},             s3ip_get_curr_max,           NULL},
    [CURR_MIN]              = {{.name = "min",              .mode = S_IRUGO | S_IWUSR},             s3ip_get_curr_min,           NULL},
    [CURR_INPUT]            = {{.name = "value",            .mode = S_IRUGO},                       s3ip_get_curr_input,         NULL},
};

void s3ip_sensor_drivers_register(struct sensor_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(s3ip_sensor_drivers_register);

void s3ip_sensor_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(s3ip_sensor_drivers_unregister);

static int __init switch_sensor_init(void)
{
    int err;
    int retval;
    int i;
    int temp_index, in_index, curr_index;
    char *temp_index_str, *in_index_str, *curr_index_str;

    char *object_name[TOTAL_SENSORS] = {"temp_sensor","vol_sensor","curr_sensor"};

    temp_index_str = (char *)kzalloc(7*sizeof(char), GFP_KERNEL);
    if (!temp_index_str)
    {
        SENSOR_DEBUG( "Fail to alloc temp_index_str memory\n");
        return -ENOMEM;
    }

    in_index_str = (char *)kzalloc(7*sizeof(char), GFP_KERNEL);
    if (!in_index_str)
    {
        SENSOR_DEBUG( "Fail to alloc in_index_str memory\n");
        err = -ENOMEM;
        goto drv_allocate_in_index_str_failed;
    }

    curr_index_str = (char *)kzalloc(7*sizeof(char), GFP_KERNEL);
    if (!curr_index_str)
    {
        SENSOR_DEBUG( "Fail to alloc curr_index_str memory\n");
        err = -ENOMEM;
        goto drv_allocate_curr_index_str_failed;
    }

    for(i = 0;i < TOTAL_SENSORS;i++)
    {
        sensor_kobj[i] = kobject_create_and_add(object_name[i], switch_kobj);
        if(!sensor_kobj[i])
        {
            SENSOR_DEBUG( "Failed to create 'sensor'\n");
            err = -ENOMEM;
            goto sysfs_create_kobject_sensor_failed;
        }

    }
    for(i=0; i<=NUM_CURR_SENSORS; i++)
    {
        SENSOR_DEBUG( "sysfs_create_file /sensor/%s\n", sensor_attr[i].attr.name);
        retval = sysfs_create_file(sensor_kobj[i], &sensor_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", sensor_attr[i].attr.name);
            err = -retval;
            goto sysfs_create_s3ip_attr_num_temp_sensors_failed;
        }
    }

    for(temp_index=0; temp_index<TEMP_TOTAL_NUM; temp_index++)
    {
        if(sprintf_s(temp_index_str, 7, "temp%d", temp_index+SENSORS_INDEX_START) < 0)
        {
            err = -ENOMEM;
            goto sysfs_create_kobject_sensor_temp_index_failed;
        }
        temp_index_kobj[temp_index] = kobject_create_and_add(temp_index_str, sensor_kobj[NUM_TEMP_SENSORS]);
        if(!temp_index_kobj[temp_index])
        {
            SENSOR_DEBUG( "Failed to create '%s'\n", temp_index_str);
            err = -ENOMEM;
            goto sysfs_create_kobject_sensor_temp_index_failed;
        }

        for(i=TEMP_ALIAS; i<=TEMP_INPUT; i++)
        {
            SENSOR_DEBUG( "sysfs_create_file /sensor/%s/%s\n", temp_index_str, sensor_attr[i].attr.name);
            retval = sysfs_create_file(temp_index_kobj[temp_index], &sensor_attr[i].attr);
            if(retval)
            {
                printk(KERN_ERR "Failed to create file '%s'\n", sensor_attr[i].attr.name);
                err = -retval;
                goto sysfs_create_s3ip_sensor_temp_attr_failed;
            }
        }
    }

    for(in_index=0; in_index<IN_TOTAL_NUM; in_index++)
    {
        if(sprintf_s(in_index_str, 7, "vol%d", in_index+SENSORS_INDEX_START) < 0)
        {
            err = -ENOMEM;
            goto sysfs_create_kobject_sensor_in_index_failed;
        }
        in_index_kobj[in_index] = kobject_create_and_add(in_index_str, sensor_kobj[NUM_IN_SENSORS]);
        if(!in_index_kobj[in_index])
        {
            SENSOR_DEBUG( "Failed to create '%s'\n", in_index_str);
            err = -ENOMEM;
            goto sysfs_create_kobject_sensor_in_index_failed;
        }

        for(i=IN_ALIAS; i<=IN_ALARM; i++)
        {
            SENSOR_DEBUG( "sysfs_create_file /sensor/%s/%s\n", in_index_str, sensor_attr[i].attr.name);
            retval = sysfs_create_file(in_index_kobj[in_index], &sensor_attr[i].attr);
            if(retval)
            {
                printk(KERN_ERR "Failed to create file '%s'\n", sensor_attr[i].attr.name);
                err = -retval;
                goto sysfs_create_s3ip_sensor_in_attr_failed;
            }
        }
    }

    for(curr_index=0; curr_index<CURR_TOTAL_NUM; curr_index++)
    {
        if(sprintf_s(curr_index_str, 7, "curr%d", curr_index+SENSORS_INDEX_START) < 0)
        {
            err = -ENOMEM;
            goto sysfs_create_kobject_sensor_curr_index_failed;
        }
        curr_index_kobj[curr_index] = kobject_create_and_add(curr_index_str, sensor_kobj[NUM_CURR_SENSORS]);
        if(!curr_index_kobj[curr_index])
        {
            SENSOR_DEBUG( "Failed to create '%s'\n", curr_index_str);
            err = -ENOMEM;
            goto sysfs_create_kobject_sensor_curr_index_failed;
        }

        for(i=CURR_ALIAS; i<=CURR_INPUT; i++)
        {
            SENSOR_DEBUG( "sysfs_create_file /sensor/%s/%s\n", curr_index_str, sensor_attr[i].attr.name);
            retval = sysfs_create_file(curr_index_kobj[curr_index], &sensor_attr[i].attr);
            if(retval)
            {
                printk(KERN_ERR "Failed to create file '%s'\n", sensor_attr[i].attr.name);
                err = -retval;
                goto sysfs_create_s3ip_sensor_curr_attr_failed;
            }
        }
    }

    kfree(temp_index_str);
    kfree(in_index_str);
    kfree(curr_index_str);
    
    return 0;
    
sysfs_create_s3ip_sensor_curr_attr_failed:
sysfs_create_kobject_sensor_curr_index_failed:
    for(curr_index=0; curr_index<CURR_TOTAL_NUM; curr_index++)
    {
        if(curr_index_kobj[curr_index])
        {
            for(i=CURR_ALIAS; i<=CURR_INPUT; i++)
                sysfs_remove_file(curr_index_kobj[curr_index], &sensor_attr[i].attr);

            kobject_put(curr_index_kobj[curr_index]);
            curr_index_kobj[curr_index] = NULL;
        }
    }

sysfs_create_s3ip_sensor_in_attr_failed:
sysfs_create_kobject_sensor_in_index_failed: 
    for(in_index=0; in_index<IN_TOTAL_NUM; in_index++)
    {
        if(in_index_kobj[in_index])
        {
            for(i=IN_ALIAS; i<=IN_ALARM; i++)
                sysfs_remove_file(in_index_kobj[in_index], &sensor_attr[i].attr);

            kobject_put(in_index_kobj[in_index]);
            in_index_kobj[in_index] = NULL;
        }
    }
    
sysfs_create_s3ip_sensor_temp_attr_failed:
sysfs_create_kobject_sensor_temp_index_failed:

    for(temp_index=0; temp_index<TEMP_TOTAL_NUM; temp_index++)
    {
        if(temp_index_kobj[temp_index])
        {
            for(i=TEMP_ALIAS; i<=TEMP_INPUT; i++)
                sysfs_remove_file(temp_index_kobj[temp_index], &sensor_attr[i].attr);

            kobject_put(temp_index_kobj[temp_index]);
            temp_index_kobj[temp_index] = NULL;
        }
    }

    for(i=0; i<=NUM_CURR_SENSORS; i++)
        sysfs_remove_file(sensor_kobj[i], &sensor_attr[i].attr);

sysfs_create_s3ip_attr_num_temp_sensors_failed:
    for(i = 0;i < TOTAL_SENSORS;i++)
    {
        if(sensor_kobj[i])
        {
            kobject_put(sensor_kobj[i]);
            sensor_kobj[i] = NULL;
        }
    }

sysfs_create_kobject_sensor_failed:
    kfree(curr_index_str);

drv_allocate_curr_index_str_failed:
    kfree(in_index_str);

drv_allocate_in_index_str_failed:
    kfree(temp_index_str);

    return err;
}

static void __exit switch_sensor_exit(void)
{
    int i;
    int temp_index, in_index, curr_index;

    /* For S3IP */
    for(i=0; i<=NUM_CURR_SENSORS; i++)
        sysfs_remove_file(sensor_kobj[i], &sensor_attr[i].attr);

    for(temp_index=0; temp_index<TEMP_TOTAL_NUM; temp_index++)
    {
        if(temp_index_kobj[temp_index])
        {
            for(i=TEMP_ALIAS; i<=TEMP_INPUT; i++)
                sysfs_remove_file(temp_index_kobj[temp_index], &sensor_attr[i].attr);

            kobject_put(temp_index_kobj[temp_index]);
            temp_index_kobj[temp_index] = NULL;
        }
    }

    for(in_index=0; in_index<IN_TOTAL_NUM; in_index++)
    {
        if(in_index_kobj[in_index])
        {
            for(i=IN_ALIAS; i<=IN_ALARM; i++)
                sysfs_remove_file(in_index_kobj[in_index], &sensor_attr[i].attr);

            kobject_put(in_index_kobj[in_index]);
            in_index_kobj[in_index] = NULL;
        }
    }

    for(curr_index=0; curr_index<CURR_TOTAL_NUM; curr_index++)
    {
        if(curr_index_kobj[curr_index])
        {
            for(i=CURR_ALIAS; i<=CURR_INPUT; i++)
                sysfs_remove_file(curr_index_kobj[curr_index], &sensor_attr[i].attr);

            kobject_put(curr_index_kobj[curr_index]);
            curr_index_kobj[curr_index] = NULL;
        }
    }

    for(i=0; i<=NUM_CURR_SENSORS; i++)
    {
        if(sensor_kobj[i])
        {
            kobject_put(sensor_kobj[i]);
            sensor_kobj[i] = NULL;
        }
    }

    cb_func = NULL;

    return;
}

MODULE_AUTHOR("Weichen Chen <weichen_chen@accton.com>");
MODULE_DESCRIPTION("Huarong Switch S3IP Sensor Driver");
MODULE_VERSION(SWITCH_S3IP_SENSOR_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_sensor_init);
module_exit(switch_sensor_exit);