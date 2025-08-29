#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "switch_sensor_driver.h"

#define SWITCH_MXSONIC_SENSOR_VERSION "0.0.0.1"

unsigned int loglevel = 0;
struct sensor_drivers_t *cb_func = NULL;
static int multiplier = 1000;

struct sensor_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct sensor_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct sensor_attribute *attr, const char *buf, size_t count);
};

/* For MXSONiC */
extern struct kobject *mx_switch_kobj;
static struct kobject *sensor_kobj;
static struct kobject *temp_index_kobj[TEMP_TOTAL_NUM];
static struct kobject *in_index_kobj[IN_TOTAL_NUM];
static struct kobject *curr_index_kobj[CURR_TOTAL_NUM];

enum sensor_attrs {
    DEBUG,
    LOGLEVEL,
    NUM_TEMP_SENSORS,
    NUM_IN_SENSORS,
    NUM_CURR_SENSORS,
    TEMP_ALIAS,
    TEMP_TYPE,
    TEMP_STATUS,
    TEMP_MAX,
    TEMP_MID,
    TEMP_MAX_HYST,
    TEMP_MIN,
    TEMP_INPUT,
    IN_ALIAS,
    IN_TYPE,
    IN_MAX,
    IN_MIN,
    IN_STATUS,
    IN_INPUT,
    IN_ALARM,
    CURR_ALIAS,
    CURR_TYPE,
    CURR_MAX,
    CURR_MIN,
    CURR_INPUT,
    CURR_AVERAGE,
    NUM_SENSOR_ATTR,
};

int get_temp_index(struct kobject *kobj)
{
    int retval;
    unsigned int temp_index;
    char temp_index_str[2] = {0};

    if(memcpy_s(temp_index_str, 2, (kobject_name(kobj)+4), 1) != 0)
    {
        return -ENOMEM;
    }
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

    if(memcpy_s(in_index_str, 3, (kobject_name(kobj)+2), strlen(kobject_name(kobj))-2) !=0)
    {
        return -ENOMEM;
    }
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

    if(memcpy_s(curr_index_str, 2, (kobject_name(kobj)+4), 1) != 0)
    {
        return -ENOMEM;
    }
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

static ssize_t mxsonic_debug_help(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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

static ssize_t mxsonic_debug(struct kobject *kobj, struct sensor_attribute *attr, const char *buf, size_t count)
{
    return count;
}

static ssize_t mxsonic_get_loglevel(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", loglevel);
}

static ssize_t mxsonic_set_loglevel(struct kobject *kobj, struct sensor_attribute *attr, const char *buf, size_t count)
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

static ssize_t mxsonic_get_temp_num(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", TEMP_TOTAL_NUM);
}

static ssize_t mxsonic_get_in_num(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", IN_TOTAL_NUM);
}

static ssize_t mxsonic_get_curr_num(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", CURR_TOTAL_NUM);
}

static ssize_t mxsonic_get_temp_alias(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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

static ssize_t mxsonic_get_temp_type(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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

static ssize_t mxsonic_get_temp_status(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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

    retval = cb_func->get_temp_status(temp_index, buf);

    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_get_temp_max(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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

    return sprintf_s(buf, PAGE_SIZE, "%s%ld.%03ld\n", ((temp_max < 0) ? "-":""), abs(temp_max/multiplier), abs(temp_max%multiplier));
}

static ssize_t mxsonic_set_temp_max(struct kobject *kobj, struct sensor_attribute *attr, const char *buf, size_t count)
{
    int retval;
    int temp_index;
    long temp_max;

    if(cb_func == NULL)
    {
        return -EIO;
    }

    temp_index = get_temp_index(kobj);
    if(temp_index < 0)
    {
        SENSOR_DEBUG("Get temp index failed.\n");
        return -EINVAL;
    }

    retval = kstrtol(buf, 10, &temp_max);
    if(retval == 0)
    {
        SENSOR_DEBUG("temp_max:%ld \n", temp_max);
    }
    else
    {
        SENSOR_DEBUG("Error:%d, temp_max:%ld \n", retval, temp_max);
        return -EIO;
    }

    retval = cb_func->set_temp_max(temp_index, temp_max);
    if(retval == false)
    {
        SENSOR_DEBUG("Set temp %d temp_max failed.\n", temp_index);
        return -EIO;
    }

    return count;
}

static ssize_t mxsonic_get_temp_mid(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    int retval;
    int temp_index;
    long temp_major;

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

    retval = cb_func->get_temp_max_hyst(temp_index, &temp_major);
    if(retval == false)
    {
        SENSOR_DEBUG("Get temp max hyst failed.\n");
        ERROR_RETURN_NA(-1);
    }
    return sprintf_s(buf, PAGE_SIZE, "%s%ld.%03ld\n", ((temp_major < 0) ? "-":""), abs(temp_major/multiplier), abs(temp_major%multiplier));
}

static ssize_t mxsonic_get_temp_max_hyst(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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
    return sprintf_s(buf, PAGE_SIZE, "%s%ld.%03ld\n", ((temp_max_hyst < 0) ? "-":""), abs(temp_max_hyst/multiplier), abs(temp_max_hyst%multiplier));
}

static ssize_t mxsonic_set_temp_max_hyst(struct kobject *kobj, struct sensor_attribute *attr, const char *buf, size_t count)
{
    int retval;
    int temp_index;
    long temp_max_hyst;

    if(cb_func == NULL)
    {
        return -EIO;
    }

    temp_index = get_temp_index(kobj);
    if(temp_index < 0)
    {
        SENSOR_DEBUG("Get temp index failed.\n");
        return -EINVAL;
    }

    retval = kstrtol(buf, 10, &temp_max_hyst);
    if(retval == 0)
    {
        SENSOR_DEBUG("temp_max_hyst:%ld \n", temp_max_hyst);
    }
    else
    {
        SENSOR_DEBUG("Error:%d, temp_max_hyst:%ld \n", retval, temp_max_hyst);
        return -EIO;
    }

    retval = cb_func->set_temp_max_hyst(temp_index, temp_max_hyst);
    if(retval == false)
    {
        SENSOR_DEBUG("Set temp %d temp_max_hyst failed.\n", temp_index);
        return -EIO;
    }

    return count;
}

static ssize_t mxsonic_get_temp_min(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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
    return sprintf_s(buf, PAGE_SIZE, "%s%ld.%03ld\n", ((temp_min < 0) ? "-":""), abs(temp_min/multiplier), abs(temp_min%multiplier));
}

static ssize_t mxsonic_set_temp_min(struct kobject *kobj, struct sensor_attribute *attr, const char *buf, size_t count)
{
    int retval;
    int temp_index;
    long temp_min;

    if(cb_func == NULL)
    {
        return -EIO;
    }

    temp_index = get_temp_index(kobj);
    if(temp_index < 0)
    {
        SENSOR_DEBUG("Get temp index failed.\n");
        return -EINVAL;
    }

    retval = kstrtol(buf, 10, &temp_min);
    if(retval == 0)
    {
        SENSOR_DEBUG("temp_min:%ld \n", temp_min);
    }
    else
    {
        SENSOR_DEBUG("Error:%d, temp_min:%ld \n", retval, temp_min);
        return -EIO;
    }

    retval = cb_func->set_temp_min(temp_index, temp_min);
    if(retval == false)
    {
        SENSOR_DEBUG("Set temp %d temp_min failed.\n", temp_index);
        return -EIO;
    }

    return count;
}

static ssize_t mxsonic_get_temp_input(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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
    return sprintf_s(buf, PAGE_SIZE, "%s%ld.%03ld\n", ((temp_input < 0) ? "-":""), abs(temp_input/multiplier), abs(temp_input%multiplier));
}

static ssize_t mxsonic_get_in_alias(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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

static ssize_t mxsonic_get_in_type(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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

static ssize_t mxsonic_get_in_max(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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
    return sprintf_s(buf, PAGE_SIZE, "%s%ld.%03ld\n", ((in_max < 0) ? "-":""), abs(in_max/multiplier), abs(in_max%multiplier));
}

static ssize_t mxsonic_get_in_min(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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
    return sprintf_s(buf, PAGE_SIZE, "%s%ld.%03ld\n", ((in_min < 0) ? "-":""), abs(in_min/multiplier), abs(in_min%multiplier));
}

static ssize_t mxsonic_get_in_status(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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
        SENSOR_DEBUG("Get temp index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_in_status(in_index, buf);

    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_get_in_input(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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
    return sprintf_s(buf, PAGE_SIZE, "%s%ld.%03ld\n", ((in_input < 0) ? "-":""), abs(in_input/multiplier), abs(in_input%multiplier));
}

static ssize_t mxsonic_get_in_alarm(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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
    return sprintf_s(buf, PAGE_SIZE, "%s%ld.%03ld\n", ((in_alarm < 0) ? "-":""), abs(in_alarm/multiplier), abs(in_alarm%multiplier));
}

static ssize_t mxsonic_get_curr_alias(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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

static ssize_t mxsonic_get_curr_type(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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

static ssize_t mxsonic_get_curr_max(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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
    return sprintf_s(buf, PAGE_SIZE, "%s%ld.%03ld\n", ((curr_max < 0) ? "-":""), abs(curr_max/multiplier), abs(curr_max%multiplier));
}

static ssize_t mxsonic_get_curr_min(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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
    return sprintf_s(buf, PAGE_SIZE, "%s%ld.%03ld\n", ((curr_min < 0) ? "-":""), abs(curr_min/multiplier), abs(curr_min%multiplier));
}

static ssize_t mxsonic_get_curr_input(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
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
    return sprintf_s(buf, PAGE_SIZE, "%s%ld.%03ld\n", ((curr_input < 0) ? "-":""), abs(curr_input/multiplier), abs(curr_input%multiplier));
}

static ssize_t mxsonic_get_curr_average(struct kobject *kobj, struct sensor_attribute *attr, char *buf)
{
    ERROR_RETURN_NA(-ENOENT);
}

static struct sensor_attribute sensor_attr[NUM_SENSOR_ATTR] = {
    [DEBUG]                 = {{.name = "debug",                    .mode = S_IRUGO | S_IWUSR},     mxsonic_debug_help,             mxsonic_debug},
    [LOGLEVEL]              = {{.name = "loglevel",                 .mode = S_IRUGO | S_IWUSR},     mxsonic_get_loglevel,           mxsonic_set_loglevel},
    [NUM_TEMP_SENSORS]      = {{.name = "num_temp_sensors",         .mode = S_IRUGO},               mxsonic_get_temp_num,           NULL},
    [NUM_IN_SENSORS]        = {{.name = "num_in_sensors",           .mode = S_IRUGO},               mxsonic_get_in_num,             NULL},
    [NUM_CURR_SENSORS]      = {{.name = "num_curr_sensors",         .mode = S_IRUGO},               mxsonic_get_curr_num,           NULL},
    [TEMP_ALIAS]            = {{.name = "temp_alias",               .mode = S_IRUGO},               mxsonic_get_temp_alias,         NULL},
    [TEMP_TYPE]             = {{.name = "temp_type",                .mode = S_IRUGO},               mxsonic_get_temp_type,          NULL},
    [TEMP_STATUS]           = {{.name = "status",                   .mode = S_IRUGO},               mxsonic_get_temp_status,        NULL},
    [TEMP_MAX]              = {{.name = "temp_max",                 .mode = S_IRUGO | S_IWUSR},     mxsonic_get_temp_max,           mxsonic_set_temp_max},
    [TEMP_MID]              = {{.name = "temp_mid",                 .mode = S_IRUGO},               mxsonic_get_temp_mid,           NULL},
    [TEMP_MAX_HYST]         = {{.name = "temp_max_hyst",            .mode = S_IRUGO | S_IWUSR},     mxsonic_get_temp_max_hyst,      mxsonic_set_temp_max_hyst},
    [TEMP_MIN]              = {{.name = "temp_min",                 .mode = S_IRUGO | S_IWUSR},     mxsonic_get_temp_min,           mxsonic_set_temp_min},
    [TEMP_INPUT]            = {{.name = "temp_input",               .mode = S_IRUGO},               mxsonic_get_temp_input,         NULL},
    [IN_ALIAS]              = {{.name = "in_alias",                 .mode = S_IRUGO},               mxsonic_get_in_alias,           NULL},
    [IN_TYPE]               = {{.name = "in_type",                  .mode = S_IRUGO},               mxsonic_get_in_type,            NULL},
    [IN_MAX]                = {{.name = "in_max",                   .mode = S_IRUGO},               mxsonic_get_in_max,             NULL},
    [IN_MIN]                = {{.name = "in_min",                   .mode = S_IRUGO},               mxsonic_get_in_min,             NULL},
    [IN_STATUS]             = {{.name = "status",                   .mode = S_IRUGO},               mxsonic_get_in_status,          NULL},
    [IN_INPUT]              = {{.name = "in_input",                 .mode = S_IRUGO},               mxsonic_get_in_input,           NULL},
    [IN_ALARM]              = {{.name = "in_alarm",                 .mode = S_IRUGO},               mxsonic_get_in_alarm,           NULL},
    [CURR_ALIAS]            = {{.name = "curr_alias",               .mode = S_IRUGO},               mxsonic_get_curr_alias,         NULL},
    [CURR_TYPE]             = {{.name = "curr_type",                .mode = S_IRUGO},               mxsonic_get_curr_type,          NULL},
    [CURR_MAX]              = {{.name = "curr_max",                 .mode = S_IRUGO},               mxsonic_get_curr_max,           NULL},
    [CURR_MIN]              = {{.name = "curr_min",                 .mode = S_IRUGO},               mxsonic_get_curr_min,           NULL},
    [CURR_INPUT]            = {{.name = "curr_input",               .mode = S_IRUGO},               mxsonic_get_curr_input,         NULL},
    [CURR_AVERAGE]          = {{.name = "curr_average",             .mode = S_IRUGO},               mxsonic_get_curr_average,       NULL},
};

void mxsonic_sensor_drivers_register(struct sensor_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_sensor_drivers_register);

void mxsonic_sensor_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_sensor_drivers_unregister);

static int __init switch_sensor_init(void)
{
    int err;
    int retval;
    int i;
    int temp_index, in_index, curr_index;
    char *temp_index_str, *in_index_str, *curr_index_str;

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

    /* For MXSONiC */
    sensor_kobj = kobject_create_and_add("sensor", mx_switch_kobj);
    if(!sensor_kobj)
    {
        SENSOR_DEBUG( "Failed to create 'sensor'\n");
        err = -ENOMEM;
        goto sysfs_create_kobject_sensor_failed;
    }

    for(i=0; i<=NUM_CURR_SENSORS; i++)
    {
        SENSOR_DEBUG( "sysfs_create_file /sensor/%s\n", sensor_attr[i].attr.name);
        retval = sysfs_create_file(sensor_kobj, &sensor_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", sensor_attr[i].attr.name);
            err = -retval;
            goto sysfs_create_mxsonic_attr_num_temp_sensors_failed;
        }
    }

    for(temp_index=0; temp_index<TEMP_TOTAL_NUM; temp_index++)
    {
        if(sprintf_s(temp_index_str, 7, "temp%d", temp_index+SENSORS_INDEX_START) < 0)
        {
            err = -ENOMEM;
            goto sysfs_create_kobject_sensor_temp_index_failed;
        }
        temp_index_kobj[temp_index] = kobject_create_and_add(temp_index_str, sensor_kobj);
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
                goto sysfs_create_mxsonic_sensor_temp_attr_failed;
            }
        }
    }

    for(in_index=0; in_index<IN_TOTAL_NUM; in_index++)
    {
        if(sprintf_s(in_index_str, 7, "in%d", in_index+SENSORS_INDEX_START) < 0)
        {
            err = -ENOMEM;
            goto sysfs_create_kobject_sensor_in_index_failed;
        }
        in_index_kobj[in_index] = kobject_create_and_add(in_index_str, sensor_kobj);
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
                goto sysfs_create_mxsonic_sensor_in_attr_failed;
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
        curr_index_kobj[curr_index] = kobject_create_and_add(curr_index_str, sensor_kobj);
        if(!curr_index_kobj[curr_index])
        {
            SENSOR_DEBUG( "Failed to create '%s'\n", curr_index_str);
            err = -ENOMEM;
            goto sysfs_create_kobject_sensor_curr_index_failed;
        }

        for(i=CURR_ALIAS; i<=CURR_AVERAGE; i++)
        {
            SENSOR_DEBUG( "sysfs_create_file /sensor/%s/%s\n", curr_index_str, sensor_attr[i].attr.name);
            retval = sysfs_create_file(curr_index_kobj[curr_index], &sensor_attr[i].attr);
            if(retval)
            {
                printk(KERN_ERR "Failed to create file '%s'\n", sensor_attr[i].attr.name);
                err = -retval;
                goto sysfs_create_mxsonic_sensor_curr_attr_failed;
            }
        }
    }

    kfree(temp_index_str);
    kfree(in_index_str);
    kfree(curr_index_str);

    return 0;

sysfs_create_mxsonic_sensor_curr_attr_failed:
sysfs_create_kobject_sensor_curr_index_failed:
    for(curr_index=0; curr_index<CURR_TOTAL_NUM; curr_index++)
    {
        if(curr_index_kobj[curr_index])
        {
            for(i=CURR_ALIAS; i<=CURR_AVERAGE; i++)
                sysfs_remove_file(curr_index_kobj[curr_index], &sensor_attr[i].attr);

            kobject_put(curr_index_kobj[curr_index]);
            curr_index_kobj[curr_index] = NULL;
        }
    }

sysfs_create_mxsonic_sensor_in_attr_failed:
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

sysfs_create_mxsonic_sensor_temp_attr_failed:
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
        sysfs_remove_file(sensor_kobj, &sensor_attr[i].attr);

sysfs_create_mxsonic_attr_num_temp_sensors_failed:
    if(sensor_kobj)
    {
        kobject_put(sensor_kobj);
        sensor_kobj = NULL;
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

    /* For MXSONiC */
    for(i=0; i<=NUM_CURR_SENSORS; i++)
        sysfs_remove_file(sensor_kobj, &sensor_attr[i].attr);

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
            for(i=CURR_ALIAS; i<=CURR_AVERAGE; i++)
                sysfs_remove_file(curr_index_kobj[curr_index], &sensor_attr[i].attr);

            kobject_put(curr_index_kobj[curr_index]);
            curr_index_kobj[curr_index] = NULL;
        }
    }

    if(sensor_kobj)
    {
        kobject_put(sensor_kobj);
        sensor_kobj = NULL;
    }

    cb_func = NULL;

    return;
}

MODULE_DESCRIPTION("Huarong Switch MXSONiC Sensor Driver");
MODULE_VERSION(SWITCH_MXSONIC_SENSOR_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_sensor_init);
module_exit(switch_sensor_exit);