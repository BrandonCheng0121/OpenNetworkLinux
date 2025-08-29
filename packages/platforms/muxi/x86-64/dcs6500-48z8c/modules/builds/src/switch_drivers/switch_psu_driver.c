#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include "fmea_format.h"
#include "switch_psu_driver.h"
#include "psu_g1156.h"
#include "cpld_lpc_driver.h"
#include "sysfs_ipmi.h"
#define DRVNAME "drv_psu_driver"
#define SWITCH_PSU_DRIVER_VERSION "0.0.1"
#define MULTIPLIER 1000

unsigned int loglevel = 0;
static struct platform_device *drv_psu_device;

struct psu_temp_attr
{
    const char *temp_alias;
    const char *temp_type;
    int temp_crit;
    int temp_max;
    int temp_min;
};

struct psu_temp_attr psu_temp_attr_tbl[PSU_TOTAL_NUM][PSU_TOTAL_TEMP_NUM] =
{
    {
        {.temp_alias = "temp1_AMB", .temp_type = "temp1", .temp_crit = 70000,  .temp_max = 60000,  .temp_min = -30000},
        {.temp_alias = "temp2_SEC", .temp_type = "temp2", .temp_crit = 120000, .temp_max = 100000, .temp_min = -30000},
    },
    {
        {.temp_alias = "temp1_AMB", .temp_type = "temp1", .temp_crit = 70000,  .temp_max = 60000,  .temp_min = -30000},
        {.temp_alias = "temp2_SEC", .temp_type = "temp2", .temp_crit = 120000, .temp_max = 100000, .temp_min = -30000},
    }
};

static int psu_cpld_read(u8 reg)
{
    return lpc_read_cpld( LCP_DEVICE_ADDRESS1, reg);
}

static int psu_cpld_write(u8 reg, u8 value)
{
    return lpc_write_cpld( LCP_DEVICE_ADDRESS1, reg, value);
}

int drv_get_psu_loglevel(int *attr_val)
{
    *attr_val = (int)loglevel;
    return 0;
}

int drv_set_psu_loglevel(int attr_val)
{
    loglevel = attr_val;
    return 0;
}

int drv_get_psu_number(int *attr_val)
{
    *attr_val = PSU_TOTAL_NUM;
    return 0;
}

int drv_get_psu_date(int index, char *buf)
{
    return psu_pmbus_read_char(index, PMBUS_MFR_DATE, buf);
}

int drv_get_psu_date_from_bmc(int index, char *buf)
{
    return drv_get_mfr_info_from_bmc(index, PMBUS_MFR_DATE, buf);
}

int drv_get_psu_vendor(int index, char *buf)
{
    return psu_pmbus_read_char(index, PMBUS_MFR_ID, buf);
}

int drv_get_psu_vendor_from_bmc(int index, char *buf)
{
    return drv_get_mfr_info_from_bmc(index, PMBUS_MFR_ID, buf);
}

int drv_get_psu_model_name(int index, char *buf)
{
    return psu_pmbus_read_char(index, PMBUS_MFR_MODEL, buf);
}

int drv_get_psu_model_name_from_bmc(int index, char *buf)
{
    return drv_get_mfr_info_from_bmc(index, PMBUS_MFR_MODEL, buf);
}

int drv_get_psu_hardware_version(int index, char *buf)
{
    return psu_pmbus_read_char(index, PMBUS_MFR_REVISION, buf);
}

int drv_get_psu_hardware_version_from_bmc(int index, char *buf)
{
    return drv_get_mfr_info_from_bmc(index, PMBUS_MFR_REVISION, buf);
}

int drv_get_psu_serial_number(int index, char *buf)
{
    return psu_pmbus_read_char(index, PMBUS_MFR_SERIAL, buf);
}

int drv_get_psu_serial_number_from_bmc(int index, char *buf)
{
    return drv_get_mfr_info_from_bmc(index, PMBUS_MFR_SERIAL, buf);
}

int drv_get_psu_part_number(int index, char *buf)
{
    return psu_pmbus_read_char(index, PMBUS_MFR_ID, buf);
}

int drv_get_psu_part_number_from_bmc(int index, char *buf)
{
    return drv_get_mfr_info_from_bmc(index, PMBUS_MFR_ID, buf);
}

int drv_get_psu_type(int index, int *attr_val)
{
    long read_value = 0;
    int ret = 0;

    ret = psu_pmbus_read_data(index, PMBUS_MFR_PSU_VIN_TYPE, &read_value);
    if (ret < 0)
        return ret;

    switch (read_value)
    {
    case READ_VIN_TYPE_AC:
        *attr_val = S3IP_PSU_INPUT_AC;
        break;
    case READ_VIN_TYPE_DC:
        *attr_val = S3IP_PSU_INPUT_DC;
        break;
    case READ_VIN_TYPE_NONE:
    default:
        return -EINVAL;
        break;
    }
    return 0;
}

int drv_get_psu_type_from_bmc(int index, int *attr_val)
{
    char buf[PSU_MAX_STR_BUFF_LENGTH] = {0};
    int read_value = 0;
    int ret = 0;

    ret = drv_get_mfr_info_from_bmc(index, PMBUS_MFR_PSU_VIN_TYPE, buf);
    if (ret < 0)
        return ret;
    ret = kstrtoint(buf, 10, &read_value);
    if (ret < 0)
        return ret;
    switch (read_value)
    {
    case READ_VIN_TYPE_AC:
        *attr_val = S3IP_PSU_INPUT_AC;
        break;
    case READ_VIN_TYPE_DC:
        *attr_val = S3IP_PSU_INPUT_DC;
        break;
    case READ_VIN_TYPE_NONE:
    default:
        return -EINVAL;
        break;
    }
    return 0;
}

int drv_get_psu_in_curr(int index, int *attr_val)
{
    long get_val = 0;
    int ret = 0;
    ret = psu_pmbus_read_data(index, PMBUS_READ_IIN, &get_val);
    *attr_val = (int)get_val;
    return ret;
}

int drv_get_psu_in_curr_from_bmc(int index, int *attr_val)
{
    char buf[PSU_MAX_STR_BUFF_LENGTH] = {0};
    int read_value = 0;
    int ret = 0;

    ret = drv_get_mfr_info_from_bmc(index, PMBUS_READ_IIN, buf);
    if (ret < 0)
        return ret;
    ret = kstrtoint(buf, 10, &read_value);
    if (ret < 0)
        return ret;
    *attr_val = (int)read_value;
    return 0;
}

int drv_get_psu_in_vol(int index, int *attr_val)
{
    long get_val = 0;
    int ret = 0;
    ret = psu_pmbus_read_data(index, PMBUS_READ_VIN, &get_val);
    *attr_val = (int)get_val;
    return ret;
}

int drv_get_psu_in_vol_from_bmc(int index, int *attr_val)
{
    char buf[PSU_MAX_STR_BUFF_LENGTH] = {0};
    int read_value = 0;
    int ret = 0;

    ret = drv_get_mfr_info_from_bmc(index, PMBUS_READ_VIN, buf);
    if (ret < 0)
        return ret;
    ret = kstrtoint(buf, 10, &read_value);
    if (ret < 0)
        return ret;
    *attr_val = (int)read_value;
    return 0;
}

int drv_get_psu_in_power(int index, long *attr_val)
{
    long get_val = 0;
    int ret = 0;
    ret = psu_pmbus_read_data(index, PMBUS_READ_PIN, &get_val);
    *attr_val = get_val;
    return ret;
}

int drv_get_psu_in_power_from_bmc(int index, long *attr_val)
{
    char buf[PSU_MAX_STR_BUFF_LENGTH] = {0};
    int read_value = 0;
    int ret = 0;

    ret = drv_get_mfr_info_from_bmc(index, PMBUS_READ_PIN, buf);
    if (ret < 0)
        return ret;
    ret = kstrtoint(buf, 10, &read_value);
    if (ret < 0)
        return ret;
    *attr_val = (long)read_value;
    return 0;
}

int drv_get_psu_out_max_power(int index, long *attr_val)
{
    *attr_val = PSU_OUT_MAX_POWER_VAL;
    return 0;
}

int drv_get_psu_out_curr(int index, int *attr_val)
{
    long get_val = 0;
    int ret = 0;
    ret = psu_pmbus_read_data(index, PMBUS_READ_IOUT, &get_val);
    *attr_val = (int)get_val;
    return ret;
}

int drv_get_psu_out_curr_from_bmc(int index, int *attr_val)
{
    char buf[PSU_MAX_STR_BUFF_LENGTH] = {0};
    int read_value = 0;
    int ret = 0;

    ret = drv_get_mfr_info_from_bmc(index, PMBUS_READ_IOUT, buf);
    if (ret < 0)
        return ret;
    ret = kstrtoint(buf, 10, &read_value);
    if (ret < 0)
        return ret;
    *attr_val = (int)read_value;
    return 0;
}

int drv_get_psu_out_vol(int index, int *attr_val)
{
    long get_val = 0;
    int ret = 0;
    ret = psu_pmbus_read_data(index, PMBUS_READ_VOUT, &get_val);
    *attr_val = (int)get_val;
    return ret;
}

int drv_get_psu_out_vol_from_bmc(int index, int *attr_val)
{
    char buf[PSU_MAX_STR_BUFF_LENGTH] = {0};
    int read_value = 0;
    int ret = 0;

    ret = drv_get_mfr_info_from_bmc(index, PMBUS_READ_VOUT, buf);
    if (ret < 0)
        return ret;
    ret = kstrtoint(buf, 10, &read_value);
    if (ret < 0)
        return ret;
    *attr_val = (int)read_value;
    return 0;
}

int drv_get_psu_out_power(int index, long *attr_val)
{
    long get_val = 0;
    int ret = 0;
    ret = psu_pmbus_read_data(index, PMBUS_READ_POUT, &get_val);
    *attr_val = get_val;
    return ret;
}

int drv_get_psu_out_power_from_bmc(int index, long *attr_val)
{
    char buf[PSU_MAX_STR_BUFF_LENGTH] = {0};
    int read_value = 0;
    int ret = 0;

    ret = drv_get_mfr_info_from_bmc(index, PMBUS_READ_POUT, buf);
    if (ret < 0)
        return ret;
    ret = kstrtoint(buf, 10, &read_value);
    if (ret < 0)
        return ret;
    *attr_val = (long)read_value;
    return 0;
}

int drv_get_psu_present(int index, int *attr_val)
{
    u8 cpld_reg = 0;
    int ret = 0;
    int present = 0;
    ret = psu_cpld_read(SYS_CPLD_PSU_INT);
    if (ret < 0)
        return ret;
    cpld_reg = (u8)ret;
    switch (index)
    {
    case 0: // PSU1
        present = (cpld_reg >> PSU1_PRESENT_OFFSET) & 0x1; // reg 0:present 1:absent
        break;
    case 1: // PSU2
        present = (cpld_reg >> PSU2_PRESENT_OFFSET) & 0x1; // reg 0:present 1:absent
        break;
    default:
        return -EINVAL;
        break;
    }
    if (present == 0)
        *attr_val = PSU_IS_PRESENT; // s3ip 1:present 0:absent
    else
        *attr_val = PSU_IS_ABSENT;
    return 0;
}

int drv_get_psu_num_temp_sensors(int index, int *attr_val)
{
    *attr_val = PSU_TOTAL_TEMP_NUM;
    return 0;
}

int drv_get_psu_out_status(int index, int *attr_val)
{
    u8 cpld_reg = 0;
    int ret = 0;
    int output_ok = 0;
    ret = psu_cpld_read(SYS_CPLD_PSU_INT);
    if (ret < 0)
        return ret;
    cpld_reg = (u8)ret;
    switch (index)
    {
    case 0: // PSU1
        output_ok = (cpld_reg >> PSU1_OUTPUT_OK_OFFSET) & 0x1; // reg 1:ok 0:fault
        break;
    case 1: // PSU2
        output_ok = (cpld_reg >> PSU2_OUTPUT_OK_OFFSET) & 0x1; // reg 1:ok 0:fault
        break;
    default:
        return -EINVAL;
        break;
    }
    *attr_val = output_ok; // s3ip 1:ok 0:fault
    return 0;
}

int drv_get_psu_in_status(int index, int *attr_val)
{
    u8 cpld_reg = 0;
    int ret = 0;
    int vin_good = 0;
    ret = psu_cpld_read(SYS_CPLD_PSU_INT);
    if (ret < 0)
        return ret;
    cpld_reg = (u8)ret;
    switch (index)
    {
    case 0: // PSU1
        vin_good = (cpld_reg >> PSU1_VIN_GOOD_OFFSET) & 0x1; // reg 1:ok 0:fault
        break;
    case 1: // PSU2
        vin_good = (cpld_reg >> PSU2_VIN_GOOD_OFFSET) & 0x1; // reg 1:ok 0:fault
        break;
    default:
        return -EINVAL;
        break;
    }
    *attr_val = vin_good; // s3ip 1:ok 0:fault
    return 0;
}

int drv_get_psu_fan_speed(int index, int *attr_val)
{
    long get_val = 0;
    int ret = 0;
    ret = psu_pmbus_read_data(index, PMBUS_READ_FAN_SPEED_1, &get_val);
    *attr_val = (int)get_val;
    return ret;
}

int drv_get_psu_fan_speed_from_bmc(int index, int *attr_val)
{
    char buf[PSU_MAX_STR_BUFF_LENGTH] = {0};
    int read_value = 0;
    int ret = 0;

    ret = drv_get_mfr_info_from_bmc(index, PMBUS_READ_FAN_SPEED_1, buf);
    if (ret < 0)
        return ret;
    ret = kstrtoint(buf, 10, &read_value);
    if (ret < 0)
        return ret;
    *attr_val = (int)read_value;
    return 0;
}

int drv_get_psu_fan_ratio(int index, int *attr_val)
{
    int fan_speed = 0;
    int ret = 0;
    ret = drv_get_psu_fan_speed(index, &fan_speed);
    if(ret)
        return ret;
    *attr_val = (fan_speed*100)/PSU_MAX_FAN_SPEED;
    if(*attr_val > 100)
        *attr_val = 100;
    return 0;
}

int drv_get_psu_fan_ratio_from_bmc(int index, int *attr_val)
{
    int fan_speed = 0;
    int ret = 0;
    ret = drv_get_psu_fan_speed_from_bmc(index, &fan_speed);
    if(ret)
        return ret;
    *attr_val = (fan_speed*100)/PSU_MAX_FAN_SPEED;
    if(*attr_val > 100)
        *attr_val = 100;
    return 0;
}

int drv_set_psu_fan_ratio(int index, int attr_val)
{
    return -1;//todo not support?
}

int drv_get_psu_led_status(int index, int *attr_val)
{
    long read_value = 0;
    int ret = 0;

    ret = psu_pmbus_read_data(index, PMBUS_READ_LED_STATUS, &read_value);
    if (ret < 0)
        return ret;

    switch (read_value)
    {
    case PSU_GREEN_ON:
        *attr_val = S3IP_LED_GREEN_ON;
        break;
    case PSU_ALL_OFF:
        *attr_val = S3IP_LED_ALL_OFF;
        break;
    case PSU_GREEN_FLASH:
        *attr_val = S3IP_LED_GREEN_FLASH;
        break;
    case PSU_RED_ON:
        *attr_val = S3IP_LED_RED_ON;
        break;
    case PSU_RED_ON1:
        *attr_val = S3IP_LED_RED_ON;
        break;
    case PSU_RED_FLASH:
        *attr_val = S3IP_LED_RED_FLASH;
        break;
    default:
        return -EINVAL;
        break;
    }
    return 0;
}

int drv_get_psu_led_status_from_bmc(int index, int *attr_val)
{
    char buf[PSU_MAX_STR_BUFF_LENGTH] = {0};
    int read_value = 0;
    int ret = 0;

    ret = drv_get_mfr_info_from_bmc(index, PMBUS_READ_LED_STATUS, buf);
    if (ret < 0)
        return ret;
    ret = kstrtoint(buf, 10, &read_value);
    if (ret < 0)
        return ret;

    switch (read_value)
    {
    case PSU_GREEN_ON:
        *attr_val = S3IP_LED_GREEN_ON;
        break;
    case PSU_ALL_OFF:
        *attr_val = S3IP_LED_ALL_OFF;
        break;
    case PSU_GREEN_FLASH:
        *attr_val = S3IP_LED_GREEN_FLASH;
        break;
    case PSU_RED_ON:
        *attr_val = S3IP_LED_RED_ON;
        break;
    case PSU_RED_ON1:
        *attr_val = S3IP_LED_RED_ON;
        break;
    case PSU_RED_FLASH:
        *attr_val = S3IP_LED_RED_FLASH;
        break;
    default:
        return -EINVAL;
        break;
    }
    return 0;
}

int drv_get_alarm_threshold_curr(int index, int *attr_val)
{
    *attr_val = PSU_MAX_ALARM_THRESHOLD_CURR;
    return 0;
}

int drv_get_alarm_threshold_vol(int index, int *attr_val)
{
    *attr_val = PSU_MAX_ALARM_THRESHOLD_VOL;
    return 0;
}

int drv_get_psu_temp_alias(int index, int temp_index, char *buf)
{
    if ((index >= PSU_TOTAL_NUM) || (temp_index >= PSU_TOTAL_TEMP_NUM))
    {
        return -EINVAL;
    }
    return sprintf_s(buf, PAGE_SIZE, "%s\n", psu_temp_attr_tbl[index][temp_index].temp_alias);
}

int drv_get_psu_temp_type(int index, int temp_index, char *buf)
{
    if ((index >= PSU_TOTAL_NUM) || (temp_index >= PSU_TOTAL_TEMP_NUM))
    {
        return -EINVAL;
    }
    return sprintf_s(buf, PAGE_SIZE, "%s\n", psu_temp_attr_tbl[index][temp_index].temp_type);
}

int drv_get_psu_temp_crit(int index, int temp_index, int *attr_val)
{
    if ((index >= PSU_TOTAL_NUM) || (temp_index >= PSU_TOTAL_TEMP_NUM))
    {
        return -EINVAL;
    }
    *attr_val = psu_temp_attr_tbl[index][temp_index].temp_crit;
    return 0;
}

int drv_set_psu_temp_crit(int index, int temp_index, int attr_val)
{
    if ((index >= PSU_TOTAL_NUM) || (temp_index >= PSU_TOTAL_TEMP_NUM))
    {
        return -EINVAL;
    }
    psu_temp_attr_tbl[index][temp_index].temp_crit = attr_val;
    return 0;
}

int drv_get_psu_temp_max(int index, int temp_index, int *attr_val)
{
    if ((index >= PSU_TOTAL_NUM) || (temp_index >= PSU_TOTAL_TEMP_NUM))
    {
        return -EINVAL;
    }
    *attr_val = psu_temp_attr_tbl[index][temp_index].temp_max;
    return 0;
}

int drv_set_psu_temp_max(int index, int temp_index, int attr_val)
{
    if ((index >= PSU_TOTAL_NUM) || (temp_index >= PSU_TOTAL_TEMP_NUM))
    {
        return -EINVAL;
    }
    psu_temp_attr_tbl[index][temp_index].temp_max = attr_val;
    return 0;
}

int drv_get_psu_temp_max_hyst(int index, int temp_index, int *attr_val)
{
    if ((index >= PSU_TOTAL_NUM) || (temp_index >= PSU_TOTAL_TEMP_NUM))
    {
        return -EINVAL;
    }
    *attr_val = 5000;
    return 0;
}

int drv_get_psu_temp_min(int index, int temp_index, int *attr_val)
{
    if ((index >= PSU_TOTAL_NUM) || (temp_index >= PSU_TOTAL_TEMP_NUM))
    {
        return -EINVAL;
    }
    *attr_val = psu_temp_attr_tbl[index][temp_index].temp_min;
    return 0;
}

int drv_set_psu_temp_min(int index, int temp_index, int attr_val)
{
    if ((index >= PSU_TOTAL_NUM) || (temp_index >= PSU_TOTAL_TEMP_NUM))
    {
        return -EINVAL;
    }
    psu_temp_attr_tbl[index][temp_index].temp_min = attr_val;
    return 0;
}

int drv_get_psu_temp_value(int index, int temp_index, int *attr_val)
{
    long get_val = 0;
    int ret = 0;

    if ((index >= PSU_TOTAL_NUM) || (temp_index >= PSU_TOTAL_TEMP_NUM))
    {
        return -EINVAL;
    }
    switch (temp_index)
    {
    case 0:
        ret = psu_pmbus_read_data(index, PMBUS_READ_TEMPERATURE_1, &get_val);
        break;
    case 1:
        ret = psu_pmbus_read_data(index, PMBUS_READ_TEMPERATURE_2, &get_val);
        break;
    case 2:
        ret = psu_pmbus_read_data(index, PMBUS_READ_TEMPERATURE_3, &get_val);
        break;
    default:
        return -EINVAL;
        break;
    }
    *attr_val = (int)get_val;
    return ret;
}

int drv_get_psu_temp_value_from_bmc(int index, int temp_index, int *attr_val)
{
    char buf[PSU_MAX_STR_BUFF_LENGTH] = {0};
    int read_value = 0;
    int ret = 0;

    if ((index >= PSU_TOTAL_NUM) || (temp_index >= PSU_TOTAL_TEMP_NUM))
    {
        return -EINVAL;
    }
    switch (temp_index)
    {
    case 0:
        ret = drv_get_mfr_info_from_bmc(index, PMBUS_READ_TEMPERATURE_1, buf);
        break;
    case 1:
        ret = drv_get_mfr_info_from_bmc(index, PMBUS_READ_TEMPERATURE_2, buf);
        break;
    case 2:
        ret = drv_get_mfr_info_from_bmc(index, PMBUS_READ_TEMPERATURE_3, buf);
        break;
    default:
        return -EINVAL;
        break;
    }
    if (ret < 0)
        return ret;
    ret = kstrtoint(buf, 10, &read_value);
    if (ret < 0)
        return ret;
    *attr_val = (int)read_value;
    return ret;
}

int get_psu_output_pg_ok(int index, int *attr_val)
{
    u8 cpld_reg = 0;
    u8 val = 0;
    int ret = 0;
    ret = psu_cpld_read(SYS_CPLD_PSU_INT);
    if (ret < 0)
        return ret;
    cpld_reg = (u8)ret;
    switch (index)
    {
    case 0: // PSU1
        val = (cpld_reg >> PSU1_OUTPUT_OK_OFFSET) & 0x1; // reg 1:ok 0:los
        break;
    case 1: // PSU2
        val = (cpld_reg >> PSU2_OUTPUT_OK_OFFSET) & 0x1; // reg 1:ok 0:los
        break;
    default:
        return -EINVAL;
        break;
    }
    if (val == PSU_OUTPUT_PG_OK)
        *attr_val = PSU_OUTPUT_PG_OK;
    else
        *attr_val = PSU_OUTPUT_PG_LOS;
    return 0;
}

int get_psu_input_ac_ok(int index, int *attr_val)
{
    u8 cpld_reg = 0;
    u8 val = 0;
    int ret = 0;
    ret = psu_cpld_read(SYS_CPLD_PSU_INT);
    if (ret < 0)
        return ret;
    cpld_reg = (u8)ret;
    switch (index)
    {
    case 0: // PSU1
        val = (cpld_reg >> PSU1_VIN_GOOD_OFFSET) & 0x1; // reg 1:ok 0:los
        break;
    case 1: // PSU2
        val = (cpld_reg >> PSU2_VIN_GOOD_OFFSET) & 0x1; // reg 1:ok 0:los
        break;
    default:
        return -EINVAL;
        break;
    }
    if (val == PSU_INPUT_AC_OK)
        *attr_val = PSU_INPUT_AC_OK;
    else
        *attr_val = PSU_INPUT_AC_LOS;
    return 0;
}

int drv_fmea_get_psu_alarm(int index, char *buf)
{
    int ret = 0;
    long reg_status = 0;
    int psu_status = 0;
    int psu_present = 0;
    int out_pg_ok = 0;
    int in_ac_ok = 0;
    ret = drv_get_psu_present(index, &psu_present);
    if(ret)
    {
        return -EINVAL;
    }

    ret = get_psu_input_ac_ok(index, &in_ac_ok);
    if(ret == 0)
    {
        if(in_ac_ok == PSU_INPUT_AC_LOS)
        psu_status |= FMEA_PSU_NO_INPUT;
    }

    ret = get_psu_output_pg_ok(index, &out_pg_ok);
    if(ret == 0)
    {
        if(out_pg_ok == PSU_OUTPUT_PG_LOS)
        psu_status |= FMEA_PSU_NO_OUTPUT;
    }

    ret = psu_pmbus_read_data(index, PMBUS_STATUS_WORD, &reg_status);
    if(ret)
    {
        psu_status |= FMEA_PSU_I2C_WARING;
    }
    else
    {
        if(reg_status & PB_STATUS_INPUT)
        {
            psu_status |= FMEA_PSU_NO_INPUT;
        }
        if(reg_status & PB_STATUS_TEMPERATURE)
        {
            psu_status |= FMEA_PSU_OT_WARING;
        }
        if(reg_status & PB_STATUS_FANS)
        {
            psu_status |= FMEA_PSU_FAN_WARING;
        }
        if(reg_status & (PB_STATUS_IOUT_POUT | PB_STATUS_VIN_UV | PB_STATUS_IOUT_OC | PB_STATUS_VOUT_OV))
        {
            psu_status |= FMEA_PSU_VOL_CUR_WARING;
        }
        if(reg_status & PB_STATUS_OFF)
        {
            psu_status |= FMEA_PSU_NO_OUTPUT;
        }
    }
    ret = fmea_format_status_bit(psu_status, psu_status_tbl, sizeof(psu_status_tbl)/sizeof(psu_status_tbl[0]), PSU_STATUS_DESCRIPTION, buf);
    return ret;
}

int drv_fmea_get_psu_alarm_from_bmc(int index, char *buf)
{
    int ret = 0;
    int reg_status = 0;
    int psu_status = 0;
    int psu_present = 0;
    int out_pg_ok = 0;
    int in_ac_ok = 0;
    ret = drv_get_psu_present(index, &psu_present);
    if(ret)
    {
        return -EINVAL;
    }

    ret = get_psu_input_ac_ok(index, &in_ac_ok);
    if(ret == 0)
    {
        if(in_ac_ok == PSU_INPUT_AC_LOS)
        psu_status |= FMEA_PSU_NO_INPUT;
    }

    ret = get_psu_output_pg_ok(index, &out_pg_ok);
    if(ret == 0)
    {
        if(out_pg_ok == PSU_OUTPUT_PG_LOS)
        psu_status |= FMEA_PSU_NO_OUTPUT;
    }

    ret = drv_get_mfr_info_from_bmc(index, PMBUS_STATUS_WORD, buf);
    if(ret < 0)
    {
        PSU_DEBUG("Get psu%d pmbus str status word failed.\n", index);
        psu_status |= FMEA_PSU_I2C_WARING;
    }
    else
    {
        ret = kstrtoint(buf, 10, &reg_status);
        if(ret < 0)
        {
            PSU_DEBUG("Get psu%d pmbus str status word failed.\n", index);
            psu_status |= FMEA_PSU_I2C_WARING;
        }
        else
        {
            if(reg_status & PB_STATUS_INPUT)
            {
                psu_status |= FMEA_PSU_NO_INPUT;
            }
            if(reg_status & PB_STATUS_TEMPERATURE)
            {
                psu_status |= FMEA_PSU_OT_WARING;
            }
            if(reg_status & PB_STATUS_FANS)
            {
                psu_status |= FMEA_PSU_FAN_WARING;
            }
            if(reg_status & (PB_STATUS_IOUT_POUT | PB_STATUS_VIN_UV | PB_STATUS_IOUT_OC | PB_STATUS_VOUT_OV))
            {
                psu_status |= FMEA_PSU_VOL_CUR_WARING;
            }
            if(reg_status & PB_STATUS_OFF)
            {
                psu_status |= FMEA_PSU_NO_OUTPUT;
            }
        }
    }

    ret = fmea_format_status_bit(psu_status, psu_status_tbl, sizeof(psu_status_tbl)/sizeof(psu_status_tbl[0]), PSU_STATUS_DESCRIPTION, buf);
    return ret;
}

int drv_get_psu_status(int index, int *attr_val)
{
    int retval = 0;
    int psu_present = 0;
    long status_word = 0;

    retval = drv_get_psu_present(index, &psu_present);
    if((retval < 0) || (psu_present == PSU_ABSENT_RET))
    {
        *attr_val = PSU_ABSENT_RET;
    }
    else
    {
        retval = psu_pmbus_read_data(index, PMBUS_STATUS_WORD, &status_word);
        if(retval < 0)
        {
            PSU_DEBUG("Get psu%d pmbus status word failed.\n", index);
            *attr_val = PSU_NOT_OK_RET;
        }
        else
        {
            if(status_word != 0)
            {
                PSU_DEBUG("Get psu%d pmbus status_word:0x%x.\n", index, status_word);
                *attr_val = PSU_NOT_OK_RET;
            }
            else
                *attr_val = PSU_OK_RET;
        }
    }
    return 0;
}

int drv_get_psu_status_from_bmc(int index, int *attr_val)
{
    int retval = 0;
    int psu_present = 0;
    char buf[PSU_MAX_STR_BUFF_LENGTH] = {0};
    int status_word = 0;

    retval = drv_get_psu_present(index, &psu_present);
    if((retval < 0) || (psu_present == PSU_ABSENT_RET))
    {
        *attr_val = PSU_ABSENT_RET;
    }
    else
    {
        retval = drv_get_mfr_info_from_bmc(index, PMBUS_STATUS_WORD, buf);
        if(retval < 0)
        {
            PSU_DEBUG("Get psu%d pmbus status word failed.\n", index);
            *attr_val = PSU_NOT_OK_RET;
        }
        else
        {
            retval = kstrtoint(buf, 10, &status_word);
            if(retval < 0)
            {
                PSU_DEBUG("Get psu%d pmbus str status word failed.\n", index);
                *attr_val = PSU_NOT_OK_RET;
            }
            else
            {
                if(status_word != 0)
                {
                    PSU_DEBUG("Get psu%d pmbus status_word:0x%x.\n", index, status_word);
                    *attr_val = PSU_NOT_OK_RET;
                }
                else
                    *attr_val = PSU_OK_RET;
            }
        }
    }
    return 0;
}

int drv_debug_help(char *buf)
{
    return sprintf_s(buf, PAGE_SIZE,
        "For psu1, use i2cget/i2cset -f -y 1 0x5a to debug.\n"
        "For psu2, use i2cget/i2cset -f -y 2 0x59 to debug.\n");
}

int drv_debug(const char *buf, int count)
{
    return 0;
}

static struct psu_drivers_t pfunc = {
    .get_psu_loglevel = drv_get_psu_loglevel,
    .set_psu_loglevel = drv_set_psu_loglevel,
    .get_psu_number = drv_get_psu_number,
    .get_psu_date = drv_get_psu_date,
    .get_psu_vendor = drv_get_psu_vendor,
    .get_psu_model_name = drv_get_psu_model_name,
    .get_psu_hardware_version = drv_get_psu_hardware_version,
    .get_psu_serial_number = drv_get_psu_serial_number,
    .get_psu_part_number = drv_get_psu_part_number,
    .get_psu_type = drv_get_psu_type,
    .get_psu_in_curr = drv_get_psu_in_curr,
    .get_psu_in_vol = drv_get_psu_in_vol,
    .get_psu_in_power = drv_get_psu_in_power,
    .get_psu_out_max_power = drv_get_psu_out_max_power,
    .get_psu_out_curr = drv_get_psu_out_curr,
    .get_psu_out_vol = drv_get_psu_out_vol,
    .get_psu_out_power = drv_get_psu_out_power,
    .get_psu_num_temp_sensors = drv_get_psu_num_temp_sensors,
    .get_psu_present = drv_get_psu_present,
    .get_psu_out_status = drv_get_psu_out_status,
    .get_psu_in_status = drv_get_psu_in_status,
    .get_psu_fan_speed = drv_get_psu_fan_speed,
    .get_psu_fan_ratio = drv_get_psu_fan_ratio,
    .set_psu_fan_ratio = drv_set_psu_fan_ratio,
    .get_psu_led_status = drv_get_psu_led_status,
    .get_alarm_threshold_curr = drv_get_alarm_threshold_curr,
    .get_alarm_threshold_vol = drv_get_alarm_threshold_vol,
    .get_psu_temp_alias = drv_get_psu_temp_alias,
    .get_psu_temp_type = drv_get_psu_temp_type,
    .get_psu_temp_crit = drv_get_psu_temp_crit,
    .set_psu_temp_crit = drv_set_psu_temp_crit,
    .get_psu_temp_max = drv_get_psu_temp_max,
    .set_psu_temp_max = drv_set_psu_temp_max,
    .get_psu_temp_max_hyst = drv_get_psu_temp_max_hyst,
    .get_psu_temp_min = drv_get_psu_temp_min,
    .set_psu_temp_min = drv_set_psu_temp_min,
    .get_psu_temp_value = drv_get_psu_temp_value,
    .get_fmea_psu_alarm = drv_fmea_get_psu_alarm,
    .get_psu_status = drv_get_psu_status,
    .debug_help = drv_debug_help,
};

static struct psu_drivers_t pfunc_bmc = {
    .get_psu_loglevel = drv_get_psu_loglevel,
    .set_psu_loglevel = drv_set_psu_loglevel,
    .get_psu_number = drv_get_psu_number,
    .get_psu_date = drv_get_psu_date_from_bmc,
    .get_psu_vendor = drv_get_psu_vendor_from_bmc,
    .get_psu_model_name = drv_get_psu_model_name_from_bmc,
    .get_psu_hardware_version = drv_get_psu_hardware_version_from_bmc,
    .get_psu_serial_number = drv_get_psu_serial_number_from_bmc,
    .get_psu_part_number = drv_get_psu_part_number_from_bmc,
    .get_psu_type = drv_get_psu_type_from_bmc,
    .get_psu_in_curr = drv_get_psu_in_curr_from_bmc,
    .get_psu_in_vol = drv_get_psu_in_vol_from_bmc,
    .get_psu_in_power = drv_get_psu_in_power_from_bmc,
    .get_psu_out_max_power = drv_get_psu_out_max_power,
    .get_psu_out_curr = drv_get_psu_out_curr_from_bmc,
    .get_psu_out_vol = drv_get_psu_out_vol_from_bmc,
    .get_psu_out_power = drv_get_psu_out_power_from_bmc,
    .get_psu_num_temp_sensors = drv_get_psu_num_temp_sensors,
    .get_psu_present = drv_get_psu_present,
    .get_psu_out_status = drv_get_psu_out_status,
    .get_psu_in_status = drv_get_psu_in_status,
    .get_psu_fan_speed = drv_get_psu_fan_speed_from_bmc,
    .get_psu_fan_ratio = drv_get_psu_fan_ratio_from_bmc,
    .set_psu_fan_ratio = drv_set_psu_fan_ratio,
    .get_psu_led_status = drv_get_psu_led_status_from_bmc,
    .get_psu_temp_alias = drv_get_psu_temp_alias,
    .get_psu_temp_type = drv_get_psu_temp_type,
    .get_psu_temp_crit = drv_get_psu_temp_crit,
    .set_psu_temp_crit = drv_set_psu_temp_crit,
    .get_psu_temp_max = drv_get_psu_temp_max,
    .set_psu_temp_max = drv_set_psu_temp_max,
    .get_psu_temp_max_hyst = drv_get_psu_temp_max_hyst,
    .get_psu_temp_min = drv_get_psu_temp_min,
    .set_psu_temp_min = drv_set_psu_temp_min,
    .get_psu_temp_value = drv_get_psu_temp_value_from_bmc,
    .get_fmea_psu_alarm = drv_fmea_get_psu_alarm_from_bmc,
    .get_psu_status = drv_get_psu_status_from_bmc,
    .debug_help = drv_debug_help,
};

static int drv_psu_probe(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    if(ipmi_bmc_is_ok())
    {
        s3ip_psu_drivers_register(&pfunc_bmc);
        mxsonic_psu_drivers_register(&pfunc_bmc);
    }
    else
    {
        s3ip_psu_drivers_register(&pfunc);
        mxsonic_psu_drivers_register(&pfunc);
    }
#else
    hisonic_psu_drivers_register(&pfunc);
    kssonic_psu_drivers_register(&pfunc);
#endif

    return 0;
}

static int drv_psu_remove(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    s3ip_psu_drivers_unregister();
    mxsonic_psu_drivers_unregister();
#else
    hisonic_psu_drivers_unregister();
    kssonic_psu_drivers_unregister();
#endif

    return 0;
}

static struct platform_driver drv_psu_driver = {
    .driver = {
        .name   = DRVNAME,
        .owner = THIS_MODULE,
    },
    .probe      = drv_psu_probe,
    .remove     = drv_psu_remove,
};

static int __init drv_psu_init(void)
{
    int err;
    int retval;

    drv_psu_device = platform_device_alloc(DRVNAME, 0);
    if(!drv_psu_device)
    {
        PSU_ERR("(#%d): platform_device_alloc fail\n", __LINE__);
        return -ENOMEM;
    }

    retval = platform_device_add(drv_psu_device);
    if(retval)
    {
        PSU_ERR("(#%d): platform_device_add failed\n", __LINE__);
        err = -retval;
        goto dev_add_failed;
    }

    retval = platform_driver_register(&drv_psu_driver);
    if(retval)
    {
        PSU_ERR("(#%d): platform_driver_register failed(%d)\n", __LINE__, err);
        err = -retval;
        goto dev_reg_failed;
    }

    return 0;

dev_reg_failed:
    platform_device_unregister(drv_psu_device);
    return err;
dev_add_failed:
    platform_device_put(drv_psu_device);
    return err;
}

static void __exit drv_psu_exit(void)
{
    platform_driver_unregister(&drv_psu_driver);
    platform_device_unregister(drv_psu_device);

    return;
}

MODULE_DESCRIPTION("Huarong PSU Driver");
MODULE_VERSION(SWITCH_PSU_DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(drv_psu_init);
module_exit(drv_psu_exit);

