#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "switch_psu_driver.h"
#include "switch_led_driver.h"

#define SWITCH_S3IP_PSU_VERSION "0.0.0.1"

unsigned int loglevel = 0;
struct psu_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct psu_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct psu_attribute *attr, const char *buf, size_t count);
};

struct psu_drivers_t *cb_func = NULL;

/* For s3ip */
extern struct kobject *switch_kobj;
static struct kobject *psu_kobj;
static struct kobject *psu_index_kobj[PSU_TOTAL_NUM];
static struct kobject *temp_index_kobj[PSU_TOTAL_NUM][PSU_TOTAL_TEMP_NUM];

enum psu_attrs {
    PSU_LOGLEVEL,
    PSU_NUMBER,
    PSU_MODEL_NAME,
    PSU_HARDWARE_VERSION,
    PSU_SERIAL_NUMBER,
    PSU_PART_NUMBER,
    PSU_TYPE,
    PSU_IN_CURR,
    PSU_IN_VOL,
    PSU_IN_POWER,
    PSU_OUT_MAX_POWER,
    PSU_OUT_CURR,
    PSU_OUT_VOL,
    PSU_OUT_POWER,
    PSU_NUM_TEMP_SENSORS,
    PSU_PRESENT,
    PSU_OUT_STATUS,
    PSU_IN_STATUS,
    PSU_FAN_SPEED,
    PSU_FAN_RATIO,
    PSU_LED_STATUS,
    NUM_PSU_ATTR,
};

enum temp_attrs {
    PSU_TEMP_ALIAS,
    PSU_TEMP_TYPE,
    PSU_TEMP_CRIT,
    PSU_TEMP_MAX,
    PSU_TEMP_MIN,
    PSU_TEMP_VALUE,
    NUM_PSU_TEMP_ATTR,
};

static int get_psu_index(struct kobject *kobj)
{
    int retval;
    int psu_index;
    char psu_index_str[MAX_INDEX_STR_LEN] = {0};

    if(memcpy_s(psu_index_str, MAX_INDEX_STR_LEN, (kobject_name(kobj)+sizeof(PSU_NAME_STRING)-1), MAX_INDEX_STR_LEN) != 0)
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

    if(!((psu_index - INDEX_START) < PSU_TOTAL_NUM))
        return -EINVAL;

    return psu_index - INDEX_START;
}

static int get_psu_temp_index(struct kobject *kobj)
{
    int retval;
    int psu_temp_index;
    char psu_temp_index_str[MAX_INDEX_STR_LEN] = {0};

    if(memcpy_s(psu_temp_index_str, MAX_INDEX_STR_LEN, (kobject_name(kobj)+sizeof(TEMP_NAME_STRING)-1), MAX_INDEX_STR_LEN) != 0)
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

    if(!((psu_temp_index - INDEX_START) < PSU_TOTAL_TEMP_NUM))
        return -EINVAL;

    return psu_temp_index - INDEX_START;
}

static ssize_t s3ip_get_psu_loglevel(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t s3ip_set_psu_loglevel(struct kobject *kobj, struct psu_attribute *attr, const char *buf, size_t count)
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

static ssize_t s3ip_get_psu_number(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t s3ip_get_psu_model_name(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t s3ip_get_psu_hardware_version(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t s3ip_get_psu_serial_number(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t s3ip_get_psu_part_number(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t s3ip_get_psu_type(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t s3ip_get_psu_in_curr(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t s3ip_get_psu_in_vol(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t s3ip_get_psu_in_power(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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
        return sprintf_s(buf, PAGE_SIZE, "%ld\n", attr_val);
    }
}

static ssize_t s3ip_get_psu_out_max_power(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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
        return sprintf_s(buf, PAGE_SIZE, "%ld\n", attr_val);
    }
}

static ssize_t s3ip_get_psu_out_curr(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t s3ip_get_psu_out_vol(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t s3ip_get_psu_out_power(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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
        return sprintf_s(buf, PAGE_SIZE, "%ld\n", attr_val);
    }
}

static ssize_t s3ip_get_psu_num_temp_sensors(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t s3ip_get_psu_present(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t s3ip_get_psu_out_status(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

    retval = cb_func->get_psu_out_status(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d out_status failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t s3ip_get_psu_in_status(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

    retval = cb_func->get_psu_in_status(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d in_status failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t s3ip_get_psu_fan_speed(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t s3ip_get_psu_fan_ratio(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

    retval = cb_func->get_psu_fan_ratio(psu_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d fan_ratio failed.\n", psu_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t s3ip_set_psu_fan_ratio(struct kobject *kobj, struct psu_attribute *attr, const char *buf, size_t count)
{
    int retval = -1;
    int attr_val = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        return -EINVAL;
    }

    psu_index = get_psu_index(kobj);
    if(psu_index < 0)
    {
        PSU_ERR("Get psu index failed.\n");
        return -ENXIO;
    }

    retval = kstrtoint(buf, 0, &attr_val);
    if(retval == 0)
    {
        PSU_DEBUG("fan_ratio:%d \n", attr_val);
    }
    else
    {
        PSU_ERR("Error:%d, fan_ratio:%s \n", retval, buf);
        return -ENXIO;
    }

    retval = cb_func->set_psu_fan_ratio(psu_index, attr_val);
    if(retval < 0)
    {
        PSU_ERR("Set PSU%d fan_ratio failed.\n", psu_index);
        return -ENXIO;
    }

    return count;
}

static ssize_t s3ip_get_psu_led_status(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t s3ip_get_psu_temp_alias(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

static ssize_t s3ip_get_psu_temp_type(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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


static ssize_t s3ip_get_psu_temp_crit(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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

    retval = cb_func->get_psu_temp_crit(psu_index, psu_temp_index, &attr_val);
    if(retval < 0)
    {
        PSU_ERR("Get PSU%d temp%d max failed.\n", psu_index, psu_temp_index);
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t s3ip_set_psu_temp_crit(struct kobject *kobj, struct psu_attribute *attr, const char *buf, size_t count)
{
    int retval = -1;
    int attr_val = -1;
    int psu_temp_index = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        return -EINVAL;
    }

    psu_index = get_psu_index(kobj->parent);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        return -EINVAL;
    }

    psu_temp_index = get_psu_temp_index(kobj);
    if(psu_temp_index < 0)
    {
        PSU_ERR("Get PSU temp index failed.\n");
        return -EINVAL;
    }

    retval = kstrtoint(buf, 0, &attr_val);
    if(retval == 0)
    {
        PSU_DEBUG("max:%d \n", attr_val);
    }
    else
    {
        PSU_ERR("Error:%d, max:%s \n", retval, buf);
        return -ENXIO;
    }

    retval = cb_func->set_psu_temp_crit(psu_index, psu_temp_index, attr_val);
    if(retval < 0)
    {
        PSU_ERR("Set PSU%d temp%d max failed.\n", psu_index, psu_temp_index);
        return -ENXIO;
    }

    return count;
}

static ssize_t s3ip_get_psu_temp_max(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t s3ip_set_psu_temp_max(struct kobject *kobj, struct psu_attribute *attr, const char *buf, size_t count)
{
    int retval = -1;
    int attr_val = -1;
    int psu_temp_index = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        return -EINVAL;
    }

    psu_index = get_psu_index(kobj->parent);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        return -EINVAL;
    }

    psu_temp_index = get_psu_temp_index(kobj);
    if(psu_temp_index < 0)
    {
        PSU_ERR("Get PSU temp index failed.\n");
        return -EINVAL;
    }

    retval = kstrtoint(buf, 0, &attr_val);
    if(retval == 0)
    {
        PSU_DEBUG("max:%d \n", attr_val);
    }
    else
    {
        PSU_ERR("Error:%d, max:%s \n", retval, buf);
        return -ENXIO;
    }

    retval = cb_func->set_psu_temp_max(psu_index, psu_temp_index, attr_val);
    if(retval < 0)
    {
        PSU_ERR("Set PSU%d temp%d max failed.\n", psu_index, psu_temp_index);
        return -ENXIO;
    }

    return count;
}

static ssize_t s3ip_get_psu_temp_min(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static ssize_t s3ip_set_psu_temp_min(struct kobject *kobj, struct psu_attribute *attr, const char *buf, size_t count)
{
    int retval = -1;
    int attr_val = -1;
    int psu_temp_index = -1;
    int psu_index = -1;

    if(cb_func == NULL)
    {
        PSU_ERR("Get PSU cb_func failed.\n");
        return -EINVAL;
    }

    psu_index = get_psu_index(kobj->parent);
    if(psu_index < 0)
    {
        PSU_ERR("Get PSU index failed.\n");
        return -EINVAL;
    }

    psu_temp_index = get_psu_temp_index(kobj);
    if(psu_temp_index < 0)
    {
        PSU_ERR("Get PSU temp index failed.\n");
        return -EINVAL;
    }

    retval = kstrtoint(buf, 0, &attr_val);
    if(retval == 0)
    {
        PSU_DEBUG("min:%d \n", attr_val);
    }
    else
    {
        PSU_ERR("Error:%d, min:%s \n", retval, buf);
        return -ENXIO;
    }

    retval = cb_func->set_psu_temp_min(psu_index, psu_temp_index, attr_val);
    if(retval < 0)
    {
        PSU_ERR("Set PSU%d temp%d min failed.\n", psu_index, psu_temp_index);
        return -ENXIO;
    }

    return count;
}

static ssize_t s3ip_get_psu_temp_value(struct kobject *kobj, struct psu_attribute *attr, char *buf)
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
        return sprintf_s(buf, PAGE_SIZE, "%d\n", attr_val);
    }
}

static struct psu_attribute psu_attr[NUM_PSU_ATTR] = {
    [PSU_LOGLEVEL]           = { {.name = "loglevel",            .mode = S_IRUGO | S_IWUSR },   s3ip_get_psu_loglevel,         s3ip_set_psu_loglevel},
    [PSU_NUMBER]             = { {.name = "number",              .mode = S_IRUGO },             s3ip_get_psu_number,           NULL},
    [PSU_MODEL_NAME]         = { {.name = "model_name",          .mode = S_IRUGO },             s3ip_get_psu_model_name,       NULL},
    [PSU_HARDWARE_VERSION]   = { {.name = "hardware_version",    .mode = S_IRUGO },             s3ip_get_psu_hardware_version, NULL},
    [PSU_SERIAL_NUMBER]      = { {.name = "serial_number",       .mode = S_IRUGO },             s3ip_get_psu_serial_number,    NULL},
    [PSU_PART_NUMBER]        = { {.name = "part_number",         .mode = S_IRUGO },             s3ip_get_psu_part_number,      NULL},
    [PSU_TYPE]               = { {.name = "type",                .mode = S_IRUGO },             s3ip_get_psu_type,             NULL},
    [PSU_IN_CURR]            = { {.name = "in_curr",             .mode = S_IRUGO },             s3ip_get_psu_in_curr,          NULL},
    [PSU_IN_VOL]             = { {.name = "in_vol",              .mode = S_IRUGO },             s3ip_get_psu_in_vol,           NULL},
    [PSU_IN_POWER]           = { {.name = "in_power",            .mode = S_IRUGO },             s3ip_get_psu_in_power,         NULL},
    [PSU_OUT_MAX_POWER]      = { {.name = "out_max_power",       .mode = S_IRUGO },             s3ip_get_psu_out_max_power,    NULL},
    [PSU_OUT_CURR]           = { {.name = "out_curr",            .mode = S_IRUGO },             s3ip_get_psu_out_curr,         NULL},
    [PSU_OUT_VOL]            = { {.name = "out_vol",             .mode = S_IRUGO },             s3ip_get_psu_out_vol,          NULL},
    [PSU_OUT_POWER]          = { {.name = "out_power",           .mode = S_IRUGO },             s3ip_get_psu_out_power,        NULL},
    [PSU_NUM_TEMP_SENSORS]   = { {.name = "num_temp_sensors",    .mode = S_IRUGO },             s3ip_get_psu_num_temp_sensors, NULL},
    [PSU_PRESENT]            = { {.name = "present",             .mode = S_IRUGO },             s3ip_get_psu_present,          NULL},
    [PSU_OUT_STATUS]         = { {.name = "out_status",          .mode = S_IRUGO },             s3ip_get_psu_out_status,       NULL},
    [PSU_IN_STATUS]          = { {.name = "in_status",           .mode = S_IRUGO },             s3ip_get_psu_in_status,        NULL},
    [PSU_FAN_SPEED]          = { {.name = "fan_speed",           .mode = S_IRUGO },             s3ip_get_psu_fan_speed,        NULL},
    [PSU_FAN_RATIO]          = { {.name = "fan_ratio",           .mode = S_IRUGO | S_IWUSR },   s3ip_get_psu_fan_ratio,        s3ip_set_psu_fan_ratio},
    [PSU_LED_STATUS]         = { {.name = "led_status",          .mode = S_IRUGO },             s3ip_get_psu_led_status,       NULL},

};

static struct psu_attribute temp_attr[NUM_PSU_TEMP_ATTR] = {
    [PSU_TEMP_ALIAS]         = { {.name = "alias",               .mode = S_IRUGO },             s3ip_get_psu_temp_alias,       NULL},
    [PSU_TEMP_TYPE]          = { {.name = "type",                .mode = S_IRUGO },             s3ip_get_psu_temp_type,        NULL},
    [PSU_TEMP_CRIT]          = { {.name = "crit",                .mode = S_IRUGO | S_IWUSR },   s3ip_get_psu_temp_crit,        s3ip_set_psu_temp_crit},
    [PSU_TEMP_MAX]           = { {.name = "max",                 .mode = S_IRUGO | S_IWUSR },   s3ip_get_psu_temp_max,         s3ip_set_psu_temp_max},
    [PSU_TEMP_MIN]           = { {.name = "min",                 .mode = S_IRUGO | S_IWUSR },   s3ip_get_psu_temp_min,         s3ip_set_psu_temp_min},
    [PSU_TEMP_VALUE]         = { {.name = "value",               .mode = S_IRUGO },             s3ip_get_psu_temp_value,       NULL},
};

void s3ip_psu_drivers_register(struct psu_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(s3ip_psu_drivers_register);

void s3ip_psu_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(s3ip_psu_drivers_unregister);

static int __init switch_psu_init(void)
{
    int err = 0;
    int retval = -1;
    int i = 0;
    int psu_index = -1;
    int temp_index = -1;
    char psu_index_str[MAX_INDEX_NAME_LEN] = {0};
    char temp_index_str[MAX_INDEX_NAME_LEN] = {0};

    /* For s3ip */
    psu_kobj = kobject_create_and_add(PSU_NAME_STRING, switch_kobj);/* create /sys/switch/psu/ */
    if(!psu_kobj)
    {
        PSU_ERR("[%s]Failed to create 'psu/'\n", __func__);
        err = -ENOMEM;
        goto sysfs_create_kobject_psu_failed;
    }

    for (i = PSU_LOGLEVEL; i <= PSU_NUMBER; i++)
    {
        PSU_DEBUG("[%s]sysfs_create_file 'psu/%s'\n", __func__, psu_attr[i].attr.name);
        retval = sysfs_create_file(psu_kobj, &psu_attr[i].attr);/* create /sys/switch/psu/xxx */
        if (retval)
        {
            PSU_ERR("[%s]Failed to create file 'psu/%s'\n", __func__, psu_attr[i].attr.name);
            err = -retval;
            goto sysfs_create_s3ip_attr_failed;
        }
    }

    for (psu_index = 0; psu_index < PSU_TOTAL_NUM; psu_index++)
    {
        if (sprintf_s(psu_index_str, sizeof(PSU_NAME_STRING) + MAX_INDEX_STR_LEN, "%s%d", PSU_NAME_STRING, psu_index + INDEX_START) < 0)
        {
            PSU_ERR("[%s]Failed to sprintf_s 'psu%d'\n", __func__, psu_index + INDEX_START);
            err = -ENOMEM;
            goto sysfs_create_kobject_switch_psu_index_failed;
        }
        psu_index_kobj[psu_index] = kobject_create_and_add(psu_index_str, psu_kobj);/* create /sys/switch/psu/psu[n]/ */
        if (!psu_index_kobj[psu_index])
        {
            PSU_ERR("[%s]Failed to create 'psu/psu%d'\n", __func__, psu_index + INDEX_START);
            err = -ENOMEM;
            goto sysfs_create_kobject_switch_psu_index_failed;
        }

        for (i = PSU_MODEL_NAME; i < NUM_PSU_ATTR; i++)
        {
            PSU_DEBUG("[%s]sysfs_create_file 'psu/psu%d/%s'\n", __func__, psu_index + INDEX_START, psu_attr[i].attr.name);
            retval = sysfs_create_file(psu_index_kobj[psu_index], &psu_attr[i].attr);/* create /sys/switch/psu/psu[n]/xxx */
            if (retval)
            {
                PSU_ERR("[%s]Failed to create file 'psu/psu%d/%s'\n", __func__, psu_index + INDEX_START, psu_attr[i].attr.name);
                err = -retval;
                goto sysfs_create_s3ip_psu_attr_failed;
            }
        }

        for (temp_index = 0; temp_index < PSU_TOTAL_TEMP_NUM; temp_index++)
        {
            if (sprintf_s(temp_index_str, sizeof(TEMP_NAME_STRING) + MAX_INDEX_STR_LEN, "%s%d", TEMP_NAME_STRING, temp_index + INDEX_START) < 0)
            {
                PSU_ERR("[%s]Failed to sprintf_s 'temp%d'\n", __func__, temp_index + INDEX_START);
                err = -ENOMEM;
                goto sysfs_create_kobject_switch_temp_index_failed;
            }
            temp_index_kobj[psu_index][temp_index] = kobject_create_and_add(temp_index_str, psu_index_kobj[psu_index]);/* create /sys/switch/psu/psu[n]/temp[n]/ */
            if (!temp_index_kobj[psu_index][temp_index])
            {
                PSU_ERR("[%s]Failed to create 'psu/psu%d/temp%d/'\n", __func__, psu_index + INDEX_START, temp_index + INDEX_START);
                err = -ENOMEM;
                goto sysfs_create_kobject_switch_temp_index_failed;
            }

            for (i = PSU_TEMP_ALIAS; i < NUM_PSU_TEMP_ATTR; i++)
            {
                PSU_DEBUG("[%s]sysfs_create_file 'psu/psu%d/%s/%s'\n", __func__, psu_index + INDEX_START, temp_index_str, temp_attr[i].attr.name);
                retval = sysfs_create_file(temp_index_kobj[psu_index][temp_index], &temp_attr[i].attr);/* create /sys/switch/psu/psu[n]/temp[n]/xxx */
                if (retval)
                {
                    PSU_ERR("[%s]Failed to create file 'psu/psu%d/%s/%s'\n", __func__, psu_index + INDEX_START, temp_index_str, temp_attr[i].attr.name);
                    err = -retval;
                    goto sysfs_create_s3ip_temp_attrs_failed;
                }
            }
        }
    }

    return 0;

sysfs_create_s3ip_temp_attrs_failed:
sysfs_create_kobject_switch_temp_index_failed:
sysfs_create_s3ip_psu_attr_failed:
sysfs_create_kobject_switch_psu_index_failed:
    for (i = PSU_LOGLEVEL; i <= PSU_TOTAL_NUM; i++)
        sysfs_remove_file(psu_kobj, &psu_attr[i].attr);/* rm /sys/switch/psu/xxx */

    for (psu_index = 0; psu_index < PSU_TOTAL_NUM; psu_index++)
    {
        if (psu_index_kobj[psu_index])
        {
            for (i = PSU_MODEL_NAME; i < NUM_PSU_ATTR; i++)
                sysfs_remove_file(psu_index_kobj[psu_index], &psu_attr[i].attr);/* rm /sys/switch/psu/psu[n]/xxx */

            for (temp_index = 0; temp_index < PSU_TOTAL_TEMP_NUM; temp_index++)
            {
                if (temp_index_kobj[psu_index][temp_index])
                {
                    for (i = PSU_TEMP_ALIAS; i < NUM_PSU_TEMP_ATTR; i++)
                        sysfs_remove_file(temp_index_kobj[psu_index][temp_index], &temp_attr[i].attr);/* rm /sys/switch/psu/psu[n]/temp[n]/xxx */
                }
                kobject_put(temp_index_kobj[psu_index][temp_index]);/* rm -r /sys/switch/psu/psu[n]/temp[n]/ */
                temp_index_kobj[psu_index][temp_index] = NULL;
            }
            kobject_put(psu_index_kobj[psu_index]);/* rm -r /sys/switch/psu/psu[n]/ */
            psu_index_kobj[psu_index] = NULL;
        }
    }

sysfs_create_s3ip_attr_failed:
    if (psu_kobj)
    {
        kobject_put(psu_kobj);/* rm -r /sys/switch/psu/ */
        psu_kobj = NULL;
    }

sysfs_create_kobject_psu_failed:
    return err;
}

static void __exit switch_psu_exit(void)
{
    int i = 0;
    int psu_index = 0;
    int temp_index = 0;

    /* For s3ip */
    for (i = PSU_LOGLEVEL; i <= PSU_NUMBER; i++)
        sysfs_remove_file(psu_kobj, &psu_attr[i].attr);/* rm /sys/switch/psu/xxx */

    for (psu_index = 0; psu_index < PSU_TOTAL_NUM; psu_index++)
    {
        if (psu_index_kobj[psu_index])
        {
            for (i = PSU_MODEL_NAME; i < NUM_PSU_ATTR; i++)
                sysfs_remove_file(psu_index_kobj[psu_index], &psu_attr[i].attr);/* rm /sys/switch/psu/psu[n]/xxx */

            for (temp_index = 0; temp_index < PSU_TOTAL_TEMP_NUM; temp_index++)
            {
                if (temp_index_kobj[psu_index][temp_index])
                {
                    for (i = PSU_TEMP_ALIAS; i < NUM_PSU_TEMP_ATTR; i++)
                        sysfs_remove_file(temp_index_kobj[psu_index][temp_index], &temp_attr[i].attr);/* rm /sys/switch/psu/psu[n]/temp[n]/xxx */
                }
                kobject_put(temp_index_kobj[psu_index][temp_index]);/* rm -r /sys/switch/psu/psu[n]/temp[n]/ */
                temp_index_kobj[psu_index][temp_index] = NULL;
            }
            kobject_put(psu_index_kobj[psu_index]);/* rm -r /sys/switch/psu/psu[n]/ */
            psu_index_kobj[psu_index] = NULL;
        }
    }
    if (psu_kobj)
    {
        kobject_put(psu_kobj);/* rm -r /sys/switch/psu/ */
        psu_kobj = NULL;
    }

    cb_func = NULL;

    return;
}

MODULE_DESCRIPTION("Switch S3IP PSU Driver");
MODULE_VERSION(SWITCH_S3IP_PSU_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_psu_init);
module_exit(switch_psu_exit);
