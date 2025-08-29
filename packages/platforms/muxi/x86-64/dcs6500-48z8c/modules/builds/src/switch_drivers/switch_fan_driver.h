#ifndef SWITCH_FAN_DRIVER_H
#define SWITCH_FAN_DRIVER_H

#include "switch.h"

#define FAN_ERR(fmt, args...)      LOG_ERR("fan: ", fmt, ##args)
#define FAN_WARNING(fmt, args...)  LOG_WARNING("fan: ", fmt, ##args)
#define FAN_INFO(fmt, args...)     LOG_INFO("fan: ", fmt, ##args)
#define FAN_DEBUG(fmt, args...)    LOG_DBG("fan: ", fmt, ##args)

#define MAX_FAN_NUM                     4
#define MAX_MOTOR_NUM                   2
#define FAN_INDEX_START                 1
#define FAN_MOTOR_INDEX_START           1

#define FAN_EEPROM_SIZE 512

#define MAX_FAN_PWM                     100
#define MAX_FAN_RPM_FRONT               30000
#define MAX_FAN_RPM_REAR                27500

#define FAN_CPLD_I2C_ADDR               0x62
#define FAN_CPLD_I2C_PRESENT_REG        0x52
#define FAN_CPLD_I2C_PRESENT_OFFSET     6
#define FAN_CPLD_I2C_PRESENT_MASK       0xc0

#define SYS_CPLD_FAN_BASE_ADDR          0x1200
#define FAN_ONLINE_OFFSET               0x00
#define FAN_DIRECTION_OFFSET            0x52
#define FAN_PWM_CTL1_OFFSET             0x08
#define FAN_PWM_CTL2_OFFSET             0x09
#define FAN_PWM_CTL3_OFFSET             0x0a
#define FAN_PWM_CTL4_OFFSET             0x0b
#define FAN_LED_CTL1_OFFSET             0x65
//#define FAN_LED_CTL2_OFFSET             0x21
#define FAN_MODULE_ENABLE               0x0E
#define FAN_TECK_SPEED_CNT_OFFSET       0x15//0x13
#define FAN_TECK_SPEED_CNT              150 // 1/167.67*1000/2*60 = 178.92

// #define FAN_LED_ALL_ON                      0x00
// #define FAN_LED_GREEN_ON                    0x0c
// #define FAN_LED_GREEN_FLASH_SLOW            0x0d
// #define FAN_LED_GREEN_FLASH_FAST            0x0e
// #define FAN_LED_RED_ON                      0x03
// #define FAN_LED_RED_FLASH_SLOW              0x07
// #define FAN_LED_RED_FLASH_FAST              0x0b
// #define FAN_LED_ALL_OFF                     0x0f

#define FAN_F2B 0
#define FAN_B2F 1
#define HI_FAN_F2B 1
#define HI_FAN_B2F 0
#define S3IP_FAN_F2B 0
#define S3IP_FAN_B2F 1

struct fan_drivers_t{
    ssize_t (*get_model_name) (unsigned int index, char* buf);
    ssize_t (*get_sn) (unsigned int index, char* buf);
    ssize_t (*get_vendor) (unsigned int index, char* buf);
    ssize_t (*get_part_number) (unsigned int index, char* buf);
    ssize_t (*get_hw_version) (unsigned int index, char* buf);
    unsigned int (*get_number) (void);
    bool (*get_present) (unsigned short *bitmap);
    bool (*get_alarm) (unsigned int index, int* alarm);
    ssize_t (*get_speed) (unsigned int fan_index, unsigned int motor_index, unsigned int *speed);
    ssize_t (*get_speed_target) (unsigned int fan_index, unsigned int motor_index, unsigned int *speed_target);
    ssize_t (*get_speed_tolerance) (unsigned int fan_index, unsigned int motor_index, unsigned int *speed_tolerance);
    ssize_t (*get_speed_max) (unsigned int fan_index, unsigned int motor_index, unsigned int *speed_max);
    ssize_t (*get_speed_min) (unsigned int fan_index, unsigned int motor_index, unsigned int *speed_min);
    ssize_t (*get_status) (unsigned int fan_index, char *buf);
    bool (*get_pwm) (unsigned int index, int *pwm);
    bool (*set_pwm) (unsigned int index, int pwm);
    bool (*get_wind) (unsigned int fan_index, unsigned int *wind);
    bool (*get_led_status) (unsigned int fan_index, unsigned int *led);
    void (*get_loglevel) (long *lev);
    void (*set_loglevel) (long lev);
    ssize_t (*get_alarm_threshold) (char *buf);
    ssize_t (*get_scan_period) (char *buf);
    void (*set_scan_period) (unsigned int period);
    ssize_t (*debug_help) (char *buf);
    ssize_t (*debug) (const char *buf, int count);
};

ssize_t drv_get_model_name(unsigned int fan_index, char *buf);
ssize_t drv_get_sn(unsigned int fan_index, char *buf);
ssize_t drv_get_vendor(unsigned int fan_index, char *buf);
ssize_t drv_get_part_number(unsigned int fan_index, char *buf);
ssize_t drv_get_hw_version(unsigned int fan_index, char *buf);
unsigned int drv_get_number(void);
bool drv_get_present(unsigned short *bitmap);
bool drv_get_alarm(unsigned int fan_index, int *alarm);
ssize_t drv_get_speed(unsigned int fan_index, unsigned int motor_index, unsigned int *speed);
ssize_t drv_get_speed_target(unsigned int fan_index, unsigned int motor_index, unsigned int *speed_target);
ssize_t drv_get_speed_tolerance(unsigned int fan_index, unsigned int motor_index, unsigned int *speed_tolerance);
ssize_t drv_get_speed_max(unsigned int fan_index, unsigned int motor_index, unsigned int *speed_max);
ssize_t drv_get_speed_min(unsigned int fan_index, unsigned int motor_index, unsigned int *speed_min);
ssize_t drv_get_status(unsigned int fan_index, char *buf);
bool drv_get_pwm(unsigned int fan_index, int *pwm);
bool drv_set_pwm(unsigned int fan_index, int pwm);
ssize_t drv_get_alarm_threshold(char *buf);
bool drv_get_wind(unsigned int fan_index, unsigned int *wind);
bool drv_get_led_status(unsigned int fan_index, unsigned int *led);
ssize_t drv_get_scan_period(char *buf);
void drv_set_scan_period(unsigned int period);
void drv_get_loglevel(long *lev);
void drv_set_loglevel(long lev);
ssize_t drv_debug_help(char *buf);
ssize_t drv_debug(const char *buf, int count);

void hisonic_fan_drivers_register(struct fan_drivers_t *p_func);
void hisonic_fan_drivers_unregister(void);
void kssonic_fan_drivers_register(struct fan_drivers_t *p_func);
void kssonic_fan_drivers_unregister(void);
void mxsonic_fan_drivers_register(struct fan_drivers_t *p_func);
void mxsonic_fan_drivers_unregister(void);
void s3ip_fan_drivers_register(struct fan_drivers_t *p_func);
void s3ip_fan_drivers_unregister(void);

#endif /* SWITCH_FAN_DRIVER_H */
