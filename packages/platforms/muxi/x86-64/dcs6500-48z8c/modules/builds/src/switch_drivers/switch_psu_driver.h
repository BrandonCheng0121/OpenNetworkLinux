#ifndef SWITCH_PSU_DRIVER_H
#define SWITCH_PSU_DRIVER_H

#include "switch.h"
#include "pmbus.h"

#define PSU_ERR(fmt, args...)      LOG_ERR("psu: ", fmt, ##args)
#define PSU_WARNING(fmt, args...)  LOG_WARNING("psu: ", fmt, ##args)
#define PSU_INFO(fmt, args...)     LOG_INFO("psu: ", fmt, ##args)
#define PSU_DEBUG(fmt, args...)    LOG_DBG("psu: ", fmt, ##args)
#define CHECK_BIT(a, b)  (((a) >> (b)) & 1U)

#define CPLD_PSU_PRSNT_REG              0x1D
#define CPLD_PSU_PRSNT_REG_OFFSET       6
#define CPLD_PSU_PRSNT_REG_MASK         0x3

#define PSU_INDEX_START             1
#define PSU_TEMP_INDEX_START        1
#define PSU_TOTAL_NUM               2
#define PSU1_INDEX                  1
#define PSU2_INDEX                  2
#define PSU_TOTAL_TEMP_NUM          2
#define PSU_NAME_STRING             "psu"
#define TEMP_NAME_STRING            "temp"
#define PSU_MAX_STR_BUFF_LENGTH     30
#define PSU_MAX_ALARM_THRESHOLD_CURR 9000  //8.500 AC   8.000 DC
#define PSU_MAX_ALARM_THRESHOLD_VOL 240000  //240.000

#define SYS_CPLD_PSU_BASE_ADDR      0x1200
#define PSU_ONLINE_OFFSET           0x01

#define PSU_MAX_FAN_SPEED   23000
// the following definition should be the same as pmbus.h
// #define PMBUS_VOUT_MODE             0x20
// #define PMBUS_POUT_MAX              0xA7
// #define PMBUS_STATUS_WORD           0x79
// #define PMBUS_STATUS_INPUT          0x7C
// #define PMBUS_STATUS_VOUT           0x7A
// #define PMBUS_READ_VIN              0x88
// #define PMBUS_READ_IIN              0x89
// #define PMBUS_READ_VOUT             0x8B
// #define PMBUS_READ_IOUT             0x8C
// #define PMBUS_READ_TEMPERATURE_1    0x8D
// #define PMBUS_READ_TEMPERATURE_2    0x8E
// #define PMBUS_READ_TEMPERATURE_3    0x8F
// #define PMBUS_READ_FAN_SPEED_1      0x90
// #define PMBUS_MFR_ID                0x99
// #define PMBUS_MFR_MODEL             0x9A
// #define PMBUS_MFR_REVISION          0x9B
// #define PMBUS_MFR_DATE              0x9D
// #define PMBUS_MFR_SERIAL            0x9E
// #define PMBUS_READ_POUT             0x96
// #define PMBUS_READ_PIN              0x97
#define PMBUS_MFR_PSU_VIN_TYPE      0x85  //just for C1A-B0650
#define PMBUS_READ_LED_STATUS       0xDA  //just for C1A-B0650

#define READ_VIN_TYPE_NONE 0
#define READ_VIN_TYPE_AC   1
#define READ_VIN_TYPE_DC   2

#define S3IP_PSU_INPUT_AC 1
#define S3IP_PSU_INPUT_DC 0

// for fmea
#define FMEA_PSU_NO_INPUT        BIT(0)
#define FMEA_PSU_OT_WARING       BIT(1)
#define FMEA_PSU_FAN_WARING      BIT(2)
#define FMEA_PSU_VOL_CUR_WARING  BIT(3)
#define FMEA_PSU_I2C_WARING      BIT(4)
#define FMEA_PSU_NO_OUTPUT       BIT(5)

#define PSU_IS_PRESENT 1
#define PSU_IS_ABSENT  0

#define PSU_OUTPUT_PG_OK   1
#define PSU_OUTPUT_PG_LOS  0

#define PSU_INPUT_AC_OK   1
#define PSU_INPUT_AC_LOS  0

#define PSU_ABSENT_RET              0
#define PSU_OK_RET                  1
#define PSU_NOT_OK_RET              2

#define PSU_OUT_MAX_POWER_VAL   (650*1000*1000) // uW  650W
#define SYS_CPLD_PSU_INT        0x1d
#define PSU1_PRESENT_OFFSET     7
#define PSU2_PRESENT_OFFSET     6
#define PSU1_OUTPUT_OK_OFFSET   3
#define PSU2_OUTPUT_OK_OFFSET   2
#define PSU1_VIN_GOOD_OFFSET    1
#define PSU2_VIN_GOOD_OFFSET    0

enum psu_led_status
{
    PSU_GREEN_ON,
    PSU_ALL_OFF,
    PSU_GREEN_FLASH,
    PSU_RED_ON,
    PSU_RED_ON1,
    PSU_RED_FLASH,
};

struct psu_drivers_t{
    int (*get_psu_loglevel)(int *attr_val);
    int (*set_psu_loglevel)(int attr_val);
    int (*get_psu_number)(int *attr_val);
    int (*get_psu_date)(int index, char *buf);
    int (*get_psu_vendor)(int index, char *buf);
    int (*get_psu_model_name)(int index, char *buf);
    int (*get_psu_hardware_version)(int index, char *buf);
    int (*get_psu_serial_number)(int index, char *buf);
    int (*get_psu_part_number)(int index, char *buf);
    int (*get_psu_type)(int index, int *attr_val);
    int (*get_psu_in_curr)(int index, int *attr_val);
    int (*get_psu_in_vol)(int index, int *attr_val);
    int (*get_psu_in_power)(int index, long *attr_val);
    int (*get_psu_out_max_power)(int index, long *attr_val);
    int (*get_psu_out_curr)(int index, int *attr_val);
    int (*get_psu_out_vol)(int index, int *attr_val);
    int (*get_psu_out_power)(int index, long *attr_val);
    int (*get_psu_num_temp_sensors)(int index, int *attr_val);
    int (*get_psu_present)(int index, int *attr_val);
    int (*get_psu_out_status)(int index, int *attr_val);
    int (*get_psu_in_status)(int index, int *attr_val);
    int (*get_psu_fan_speed)(int index, int *attr_val);
    int (*get_psu_fan_ratio)(int index, int *attr_val);
    int (*set_psu_fan_ratio)(int index, int attr_val);
    int (*get_psu_led_status)(int index, int *attr_val);
    int (*get_alarm_threshold_curr)(int index, int *attr_val);
    int (*get_alarm_threshold_vol)(int index, int *attr_val);
    int (*get_psu_temp_alias)(int index, int temp_index, char *buf);
    int (*get_psu_temp_type)(int index, int temp_index, char *buf);
    int (*get_psu_temp_crit)(int index, int temp_index, int *attr_val);
    int (*set_psu_temp_crit)(int index, int temp_index, int attr_val);
    int (*get_psu_temp_max)(int index, int temp_index, int *attr_val);
    int (*set_psu_temp_max)(int index, int temp_index, int attr_val);
    int (*get_psu_temp_max_hyst)(int index, int temp_index, int *attr_val);
    int (*get_psu_temp_min)(int index, int temp_index, int *attr_val);
    int (*set_psu_temp_min)(int index, int temp_index, int attr_val);
    int (*get_psu_temp_value)(int index, int temp_index, int *attr_val);
    int (*get_fmea_psu_alarm)(int index, char *buf);
    int (*get_psu_status)(int index, int *attr_val);
    int (*debug_help) (char *buf);
    int (*debug) (const char *buf, int count);
};

void hisonic_psu_drivers_register(struct psu_drivers_t *p_func);
void hisonic_psu_drivers_unregister(void);
void kssonic_psu_drivers_register(struct psu_drivers_t *p_func);
void kssonic_psu_drivers_unregister(void);
void mxsonic_psu_drivers_register(struct psu_drivers_t *p_func);
void mxsonic_psu_drivers_unregister(void);
void s3ip_psu_drivers_register(struct psu_drivers_t *p_func);
void s3ip_psu_drivers_unregister(void);

#endif /* SWITCH_PSU_DRIVER_H */
