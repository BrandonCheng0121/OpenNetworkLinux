#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "switch.h"
#include "switch_psu_driver.h"
#include "switch_led_driver.h"

#define SWITCH_MXSONIC_PSU_VERSION "0.0.0.1"

unsigned int loglevel = 0;

struct psu_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct psu_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct psu_attribute *attr, const char *buf, size_t count);
};

struct psu_drivers_t *cb_func = NULL;

/* For MXSONiC */
extern struct kobject *mx_switch_kobj;
static struct kobject *psu_kobj;
static struct kobject *psu_index_kobj[PSU_TOTAL_NUM];
static struct kobject *temp_index_kobj[PSU_TOTAL_NUM][PSU_TOTAL_TEMP_NUM];

enum psu_attrs {
    DEBUG,
    LOGLEVEL,
    PSU_NUM,
    PSU_MODEL_NAME,
    PSU_SERIAL_NUMBER,
    PSU_DATE,
    PSU_VENDOR,
    PSU_HARDWARE_VERSION,
    PSU_ALARM,
    PSU_ALARM_THRESHOLD_CURR,
    PSU_ALARM_THRESHOLD_VOL,
    PSU_PART_NUMBER,
    PSU_MAX_OUTPUT_POWER,
    PSU_IN_CURR,
    PSU_IN_VOL,
    PSU_IN_POWER,
    PSU_OUT_CURR,
    PSU_OUT_VOL,
    PSU_OUT_POWER,
    PSU_PRESENT,
    PSU_STATUS,
    PSU_LED_STATUS,
    PSU_TYPE,
    PSU_NUM_TEMP_SENSORS,
    PSU_FAN_SPEED,
    NUM_PSU_ATTR,
};

enum temp_attrs {
    TEMP_ALIAS,
    TEMP_TYPE,
    TEMP_MAX,
    TEMP_MAX_HYST,
    TEMP_MIN,
    TEMP_INPUT,
    NUM_TEMP_ATTR,
};

int get_psu_index(struct kobject *kobj)
{
    int retval;
    int psu_index;
    char psu_index_str[2] = {0};

    if(memcpy_s(psu_index_str, 2, (kobject_name(kobj)+sizeof(PSU_NAME_STRING)-1), 2) != 0)
    {
        return -ENOMEM;
    }
    retval = kstrtoint(psu_index_str, 10, &psu_index);
    if(retval == 0)
    {
        PSU_DEBUG("psu_index:%d \n", psu_index);
    }
    else
    {
        PSU_ERR("Error:%d, psu_index:%s \n", retval, psu_index_str);
        return -EINVAL;
    }

    if(!((psu_index-PSU_INDEX_START) < PSU_TOTAL_NUM))
        return -EINVAL;

    return (psu_index-PSU_INDEX_START);
}

int get_psu_temp_index(struct kobject *kobj)
{
    int retval;
    int psu_temp_index;
    char psu_temp_index_str[2] = {0};

    if(memcpy_s(psu_temp_index_str, 2, (kobject_name(kobj)+sizeof(TEMP_NAME_STRING)-1), 2) != 0)
    {
        return -ENOMEM;
    }
    retval = kstrtoint(psu_temp_index_str, 10, &psu_temp_index);
    if(retval == 0)
    {
        PSU_DEBUG("psu_temp_index:%d \n", psu_temp_index);
    }
    else
    {
        PSU_ERR("Error:%d, psu_temp_index:%s \n", retval, psu_temp_index_str);
        return -EINVAL;
    }

    if(!((psu_temp_index-PSU_TEMP_INDEX_START) < PSU_TOTAL_TEMP_NUM))
        return -EINVAL;

    return (psu_temp_index-PSU_TEMP_INDEX_START);
}

static ssize_t mxsonic_debug_help(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t mxsonic_debug(struct kobject *kobj, struct psu_attribute *attr, const char *buf, size_t count)
{
    return count;
}

static ssize_t mxsonic_get_loglevel(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = 0;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    retval = cb_func->get_psu_loglevel(&attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU loglevel failed.\n");
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t mxsonic_set_loglevel(struct kobject *kobj, struct psu_attribute *attr, const char *buf, size_t count)
{
    int retval = -1;
    int attr_val = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        return -EINVAL;
    }

    retval = kstrtoint(buf, 0, &attr_val);
    if(retval == 0)
    {
        PSU_DEBUG("loglevel:%d \n", attr_val);
    }
    else
    {
        PSU_ERR("Error:%d, loglevel:%s \n", retval, buf);
        return -ENXIO;
    }

    retval = cb_func->set_psu_loglevel(attr_val);
    if(retval < 0)
    {
        PSU_ERR("Set psu loglevel failed.\n");
        return -ENXIO;
    }

    return count;
}

static ssize_t mxsonic_psu_get_num(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = 0;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    retval = cb_func->get_psu_number(&attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU number failed.\n");
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t mxsonic_psu_get_psu_model_name(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_model_name(psu_index, buf);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d model_name failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    return retval;
}

static ssize_t mxsonic_psu_get_serial_number(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_serial_number(psu_index, buf);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d serial_number failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    return retval;
}

static ssize_t mxsonic_psu_get_date(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_date(psu_index, buf);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d date failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    return retval;
}

static ssize_t mxsonic_psu_get_vendor(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_vendor(psu_index, buf);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d date failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    return retval;
}

static ssize_t mxsonic_psu_get_hardware_version(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_hardware_version(psu_index, buf);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d hardware_version failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    return retval;
}

static ssize_t mxsonic_psu_get_alarm(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_fmea_psu_alarm(psu_index, buf);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d hardware_version failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    return retval;
}

static ssize_t mxsonic_psu_get_alarm_threshold_curr(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_alarm_threshold_curr(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d alarm_threshold_curr failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static ssize_t mxsonic_psu_get_alarm_threshold_vol(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_alarm_threshold_vol(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d alarm_threshold_vol failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static ssize_t mxsonic_psu_get_part_number(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_part_number(psu_index, buf);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d part_number failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    return retval;
}

static ssize_t mxsonic_psu_get_max_output_power(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    long attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_out_max_power(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d out_max_power failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        attr_val = attr_val/1000;
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static ssize_t mxsonic_psu_get_in_curr(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_in_curr(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d in_curr failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static ssize_t mxsonic_psu_get_in_vol(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_in_vol(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d in_vol failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static ssize_t mxsonic_psu_get_in_power(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    long attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_in_power(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d in_power failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        attr_val = attr_val/1000;
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static ssize_t mxsonic_psu_get_out_curr(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_out_curr(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d out_curr failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static ssize_t mxsonic_psu_get_out_vol(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_out_vol(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d out_vol failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static ssize_t mxsonic_psu_get_out_power(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    long attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_out_power(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d out_power failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        attr_val = attr_val/1000;
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static ssize_t mxsonic_psu_get_present(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_present(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d present failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t mxsonic_psu_get_status(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_status(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d status failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t mxsonic_psu_get_led_status(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_led_status(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d led_status failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t mxsonic_psu_get_type(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_type(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d type failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t mxsonic_psu_get_num_temp_sensors(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_num_temp_sensors(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d num_temp_sensors failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t mxsonic_psu_get_fan_speed(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_fan_speed(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d fan_speed failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t mxsonic_get_psu_temp_alias(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;
    int psu_temp_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj->parent);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    psu_temp_index = get_psu_temp_index(kobj);
    if(psu_temp_index < 0)
    {
        PSU_ERR("Get PSU temp index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_temp_alias(psu_index, psu_temp_index, buf);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d temp%d alias failed.\n", psu_index, psu_temp_index);
        ERROR_RETURN_NA(retval);
    }
    return retval;
}

static ssize_t mxsonic_get_psu_temp_type(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;
    int psu_temp_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj->parent);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    psu_temp_index = get_psu_temp_index(kobj);
    if(psu_temp_index < 0)
    {
        PSU_ERR("Get PSU temp index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_temp_alias(psu_index, psu_temp_index, buf);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d temp%d type failed.\n", psu_index, psu_temp_index);
        ERROR_RETURN_NA(retval);
    }
    return retval;
}

static ssize_t mxsonic_get_psu_temp_max(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;
    int psu_temp_index = -1;
    int attr_val = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj->parent);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    psu_temp_index = get_psu_temp_index(kobj);
    if(psu_temp_index < 0)
    {
        PSU_ERR("Get PSU temp index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_temp_max(psu_index, psu_temp_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d temp%d max failed.\n", psu_index, psu_temp_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static ssize_t mxsonic_get_psu_temp_max_hyst(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;
    int psu_temp_index = -1;
    int attr_val = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj->parent);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    psu_temp_index = get_psu_temp_index(kobj);
    if(psu_temp_index < 0)
    {
        PSU_ERR("Get PSU temp index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_temp_max_hyst(psu_index, psu_temp_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d temp%d max failed.\n", psu_index, psu_temp_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static ssize_t mxsonic_get_psu_temp_min(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;
    int psu_temp_index = -1;
    int attr_val = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj->parent);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    psu_temp_index = get_psu_temp_index(kobj);
    if(psu_temp_index < 0)
    {
        PSU_ERR("Get PSU temp index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_temp_min(psu_index, psu_temp_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d temp%d min failed.\n", psu_index, psu_temp_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static ssize_t mxsonic_get_psu_temp_input(struct kobject *kobj, struct psu_attribute *attr, char *buf)
{
    int retval;
    int psu_index = -1;
    int psu_temp_index = -1;
    int attr_val = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        ERROR_RETURN_NA(-EFAULT);
    }

    psu_index = get_psu_index(kobj->parent);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    psu_temp_index = get_psu_temp_index(kobj);
    if(psu_temp_index < 0)
    {
        PSU_ERR("Get PSU temp index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_psu_temp_value(psu_index, psu_temp_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d temp%d value failed.\n", psu_index, psu_temp_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%s%d.%03d\n", attr_val < 0 ? "-" : "", abs(attr_val) / 1000, abs(attr_val) % 1000);
    }
}

static struct psu_attribute psu_attr[NUM_PSU_ATTR] = {
    [DEBUG]                     = {{.name = "debug",                .mode = S_IRUGO | S_IWUSR},     mxsonic_debug_help,                     mxsonic_debug},
    [LOGLEVEL]                  = {{.name = "loglevel",             .mode = S_IRUGO | S_IWUSR},     mxsonic_get_loglevel,                   mxsonic_set_loglevel},
    [PSU_NUM]                   = {{.name = "num",                  .mode = S_IRUGO},               mxsonic_psu_get_num,                    NULL},
    [PSU_MODEL_NAME]            = {{.name = "model_name",           .mode = S_IRUGO},               mxsonic_psu_get_psu_model_name,         NULL},
    [PSU_SERIAL_NUMBER]         = {{.name = "serial_number",        .mode = S_IRUGO},               mxsonic_psu_get_serial_number,          NULL},
    [PSU_DATE]                  = {{.name = "date",                 .mode = S_IRUGO},               mxsonic_psu_get_date,                   NULL},
    [PSU_VENDOR]                = {{.name = "vendor",               .mode = S_IRUGO},               mxsonic_psu_get_vendor,                 NULL},
    [PSU_HARDWARE_VERSION]      = {{.name = "hardware_version",     .mode = S_IRUGO},               mxsonic_psu_get_hardware_version,       NULL},
    [PSU_ALARM]                 = {{.name = "alarm",                .mode = S_IRUGO},               mxsonic_psu_get_alarm,                  NULL},
    [PSU_ALARM_THRESHOLD_CURR]  = {{.name = "alarm_threshold_curr", .mode = S_IRUGO},               mxsonic_psu_get_alarm_threshold_curr,   NULL},
    [PSU_ALARM_THRESHOLD_VOL]   = {{.name = "alarm_threshold_vol",  .mode = S_IRUGO},               mxsonic_psu_get_alarm_threshold_vol,    NULL},
    [PSU_PART_NUMBER]           = {{.name = "part_number",          .mode = S_IRUGO},               mxsonic_psu_get_part_number,            NULL},
    [PSU_MAX_OUTPUT_POWER]      = {{.name = "max_output_power",     .mode = S_IRUGO},               mxsonic_psu_get_max_output_power,       NULL},
    [PSU_IN_CURR]               = {{.name = "in_curr",              .mode = S_IRUGO},               mxsonic_psu_get_in_curr,                NULL},
    [PSU_IN_VOL]                = {{.name = "in_vol",               .mode = S_IRUGO},               mxsonic_psu_get_in_vol,                 NULL},
    [PSU_IN_POWER]              = {{.name = "in_power",             .mode = S_IRUGO},               mxsonic_psu_get_in_power,               NULL},
    [PSU_OUT_CURR]              = {{.name = "out_curr",             .mode = S_IRUGO},               mxsonic_psu_get_out_curr,               NULL},
    [PSU_OUT_VOL]               = {{.name = "out_vol",              .mode = S_IRUGO},               mxsonic_psu_get_out_vol,                NULL},
    [PSU_OUT_POWER]             = {{.name = "out_power",            .mode = S_IRUGO},               mxsonic_psu_get_out_power,              NULL},
    [PSU_PRESENT]               = {{.name = "present",              .mode = S_IRUGO},               mxsonic_psu_get_present,                NULL},
    [PSU_STATUS]                = {{.name = "status",               .mode = S_IRUGO},               mxsonic_psu_get_status,                 NULL},
    [PSU_LED_STATUS]            = {{.name = "led_status",           .mode = S_IRUGO},               mxsonic_psu_get_led_status,             NULL},
    [PSU_TYPE]                  = {{.name = "type",                 .mode = S_IRUGO},               mxsonic_psu_get_type,                   NULL},
    [PSU_NUM_TEMP_SENSORS]      = {{.name = "num_temp_sensors",     .mode = S_IRUGO},               mxsonic_psu_get_num_temp_sensors,       NULL},
    [PSU_FAN_SPEED]             = {{.name = "fan_speed",            .mode = S_IRUGO},               mxsonic_psu_get_fan_speed,              NULL},
};

static struct psu_attribute temp_attr[NUM_TEMP_ATTR] = {
    [TEMP_ALIAS]            = {{.name = "temp_alias",               .mode = S_IRUGO},           mxsonic_get_psu_temp_alias,         NULL},
    [TEMP_TYPE]             = {{.name = "temp_type",                .mode = S_IRUGO},           mxsonic_get_psu_temp_type,          NULL},
    [TEMP_MAX]              = {{.name = "temp_max",                 .mode = S_IRUGO},           mxsonic_get_psu_temp_max,           NULL},
    [TEMP_MAX_HYST]         = {{.name = "temp_max_hyst",            .mode = S_IRUGO},           mxsonic_get_psu_temp_max_hyst,      NULL},
    [TEMP_MIN]              = {{.name = "temp_min",                 .mode = S_IRUGO},           mxsonic_get_psu_temp_min,           NULL},
    [TEMP_INPUT]            = {{.name = "temp_input",               .mode = S_IRUGO},           mxsonic_get_psu_temp_input,         NULL},
};

void mxsonic_psu_drivers_register(struct psu_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_psu_drivers_register);

void mxsonic_psu_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_psu_drivers_unregister);

static int __init switch_psu_init(void)
{
    int err;
    int retval;
    int i;
    int psu_index, temp_index;
    char *psu_index_str, *temp_index_str;

    psu_index_str = (char *)kzalloc(sizeof(PSU_NAME_STRING)+3, GFP_KERNEL);
    if (!psu_index_str)
    {
        PSU_ERR("Fail to alloc psu_index_str memory\n");
        return -ENOMEM;
    }

    temp_index_str = (char *)kzalloc(7*sizeof(char), GFP_KERNEL);
    if (!temp_index_str)
    {
        PSU_ERR("Fail to alloc temp_index_str memory\n");
        err =  -ENOMEM;
        goto drv_allocate_temp_index_str_failed;
    }

    /* For MXSONiC */
    psu_kobj = kobject_create_and_add(PSU_NAME_STRING, mx_switch_kobj);
    if(!psu_kobj)
    {
        PSU_ERR("Failed to create 'psu'\n");
        err = -ENOMEM;
        goto sysfs_create_kobject_psu_failed;
    }

    for(i=0; i <= PSU_NUM; i++)
    {
        PSU_DEBUG("sysfs_create_file /psu/%s\n", psu_attr[i].attr.name);
        retval = sysfs_create_file(psu_kobj, &psu_attr[i].attr);
        if(retval)
        {
            PSU_ERR("Failed to create file '%s'\n", psu_attr[i].attr.name);
            err = -retval;
            goto sysfs_create_mxsonic_attr_failed;
        }
    }

    for(psu_index=0; psu_index<PSU_TOTAL_NUM; psu_index++)
    {
        if(sprintf_s(psu_index_str, sizeof(PSU_NAME_STRING)+3, "%s%d", PSU_NAME_STRING, psu_index+PSU_INDEX_START) < 0)
        {
            err = -ENOMEM;
            goto sysfs_create_kobject_switch_psu_index_failed;
        }
        psu_index_kobj[psu_index] = kobject_create_and_add(psu_index_str, psu_kobj);
        if(!psu_index_kobj[psu_index])
        {
            PSU_ERR("Failed to create 'psu%d'\n", psu_index+PSU_INDEX_START);
            err = -ENOMEM;
            goto sysfs_create_kobject_switch_psu_index_failed;
        }

        for(i=PSU_MODEL_NAME; i < NUM_PSU_ATTR; i++)
        {
            PSU_DEBUG("sysfs_create_file /psu/psu%d/%s\n", psu_index, psu_attr[i].attr.name);
            retval = sysfs_create_file(psu_index_kobj[psu_index], &psu_attr[i].attr);
            if(retval)
            {
                PSU_ERR("Failed to create file '%s'\n", psu_attr[i].attr.name);
                err = -retval;
                goto sysfs_create_mxsonic_psu_attr_failed;
            }
        }

        for(temp_index=0; temp_index<PSU_TOTAL_TEMP_NUM; temp_index++)
        {
            if(sprintf_s(temp_index_str, 7, "%s%d", TEMP_NAME_STRING, temp_index+PSU_TEMP_INDEX_START) < 0)
            {
                err = -ENOMEM;
                goto sysfs_create_kobject_switch_temp_index_failed;
            }
            temp_index_kobj[psu_index][temp_index] = kobject_create_and_add(temp_index_str, psu_index_kobj[psu_index]);
            if(!temp_index_kobj[psu_index][temp_index])
            {
                PSU_DEBUG("Failed to create 'temp%d'\n", temp_index+PSU_TEMP_INDEX_START);
                err = -ENOMEM;
                goto sysfs_create_kobject_switch_temp_index_failed;
            }

            for(i=TEMP_ALIAS; i<NUM_TEMP_ATTR; i++)
            {
                PSU_DEBUG("sysfs_create_file /switch/psu/psu%d/%s/%s\n", psu_index+PSU_INDEX_START, temp_index_str, temp_attr[i].attr.name);

                retval = sysfs_create_file(temp_index_kobj[psu_index][temp_index], &temp_attr[i].attr);
                if(retval)
                {
                    PSU_ERR("Failed to create file '%s'\n", temp_attr[i].attr.name);
                    err = -retval;
                    goto sysfs_create_mxsonic_temp_attrs_failed;
                }
            }
        }
    }

    kfree(psu_index_str);
    kfree(temp_index_str);

    return 0;

sysfs_create_mxsonic_temp_attrs_failed:
sysfs_create_kobject_switch_temp_index_failed:
sysfs_create_mxsonic_psu_attr_failed:
sysfs_create_kobject_switch_psu_index_failed:
    for(i=0; i <= PSU_NUM; i++)
        sysfs_remove_file(psu_kobj, &psu_attr[i].attr);

    for(psu_index=0; psu_index<PSU_TOTAL_NUM; psu_index++)
    {
        if(psu_index_kobj[psu_index])
        {
            for(i=PSU_MODEL_NAME; i < NUM_PSU_ATTR; i++)
                sysfs_remove_file(psu_index_kobj[psu_index], &psu_attr[i].attr);

            for(temp_index=0; temp_index<PSU_TOTAL_TEMP_NUM; temp_index++)
            {
                if(temp_index_kobj[psu_index][temp_index])
                {
                    for(i=TEMP_ALIAS; i<NUM_TEMP_ATTR; i++)
                        sysfs_remove_file(temp_index_kobj[psu_index][temp_index], &temp_attr[i].attr);
                }

                kobject_put(temp_index_kobj[psu_index][temp_index]);
                temp_index_kobj[psu_index][temp_index] = NULL;
            }

            kobject_put(psu_index_kobj[psu_index]);
            psu_index_kobj[psu_index] = NULL;
        }
    }

sysfs_create_mxsonic_attr_failed:
    if(psu_kobj)
    {
        kobject_put(psu_kobj);
        psu_kobj = NULL;
    }

sysfs_create_kobject_psu_failed:
    kfree(temp_index_str);

drv_allocate_temp_index_str_failed:
    kfree(psu_index_str);

    return err;
}

static void __exit switch_psu_exit(void)
{
    int i;
    int psu_index, temp_index;

    /* For MXSONiC */
    for(i=0; i <= PSU_NUM; i++)
        sysfs_remove_file(psu_kobj, &psu_attr[i].attr);

    for(psu_index=0; psu_index<PSU_TOTAL_NUM; psu_index++)
    {
        if(psu_index_kobj[psu_index])
        {
            for(i=PSU_MODEL_NAME; i < NUM_PSU_ATTR; i++)
                sysfs_remove_file(psu_index_kobj[psu_index], &psu_attr[i].attr);

            for(temp_index=0; temp_index<PSU_TOTAL_TEMP_NUM; temp_index++)
            {
                if(temp_index_kobj[psu_index][temp_index])
                {
                    for(i=TEMP_ALIAS; i<NUM_TEMP_ATTR; i++)
                        sysfs_remove_file(temp_index_kobj[psu_index][temp_index], &temp_attr[i].attr);
                }

                kobject_put(temp_index_kobj[psu_index][temp_index]);
                temp_index_kobj[psu_index][temp_index] = NULL;
            }

            kobject_put(psu_index_kobj[psu_index]);
            psu_index_kobj[psu_index] = NULL;
        }
    }

    if(psu_kobj)
    {
        kobject_put(psu_kobj);
        psu_kobj = NULL;
    }

    cb_func = NULL;

    return;
}

MODULE_DESCRIPTION("Huarong Switch MXSONiC PSU Driver");
MODULE_VERSION(SWITCH_MXSONIC_PSU_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_psu_init);
module_exit(switch_psu_exit);
