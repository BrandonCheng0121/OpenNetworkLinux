#ifndef SWITCH_SENSOR_DRIVER_H
#define SWITCH_SENSOR_DRIVER_H

#include "switch.h"

#define SENSOR_ERR(fmt, args...)      LOG_ERR("sensor: ", fmt, ##args)
#define SENSOR_WARNING(fmt, args...)  LOG_WARNING("sensor: ", fmt, ##args)
#define SENSOR_INFO(fmt, args...)     LOG_INFO("sensor: ", fmt, ##args)
#define SENSOR_DEBUG(fmt, args...)    LOG_DBG("sensor: ", fmt, ##args)

#define SENSORS_INDEX_START 1
#define TOTAL_SENSORS       3
#define MAX_X86_CORE_ATTRS  4
#define TOTAL_X86_TEMP_ATTRS    (MAX_X86_CORE_ATTRS + 1)
#define TOTAL_LSW_TEMP      1
#define TOTAL_AVS_TEMP      2
#define TOTAL_LM75_TEMP     3
#define TEMP_TOTAL_NUM      (TOTAL_LM75_TEMP + TOTAL_LSW_TEMP)
#define TOTAL_AVS_NUM       (8+15)  //lm25066:Vin 1 Vout 2, mp2976:Vin 1 Vout 2, mp2973:Vin 1 Vout 2, max34461 0-15ch Vin
#define IN_TOTAL_NUM        (8+15)  //
#define CURR_TOTAL_NUM      (7)     //lm25066:Iin 1 Iout 0, mp2976:Iin 1 Iout 2, mp2973:Iin 1 Iout 2, 

struct sensor_drivers_t{
    ssize_t (*get_temp_alias) (unsigned int index, char *buf);
    ssize_t (*get_temp_type) (unsigned int index, char *buf);
    bool (*get_temp_max) (unsigned int index, long *val);
    bool (*get_temp_max_hyst) (unsigned int index, long *val);
    bool (*get_temp_min) (unsigned int index, long *val);
    bool (*set_temp_max) (unsigned int index, long val);
    bool (*set_temp_max_hyst) (unsigned int index, long val);
    bool (*set_temp_min) (unsigned int index, long val);
    bool (*get_temp_input) (unsigned int index, long *temp_input);
    ssize_t (*get_temp_status) (unsigned int index, char *buf);
    ssize_t (*get_in_alias) (unsigned int index, char *buf);
    ssize_t (*get_in_type) (unsigned int index, char *buf);
    bool (*get_in_max) (unsigned int index, long *val);
    bool (*get_in_min) (unsigned int index, long *val);
    bool (*get_in_input) (unsigned int index, long *val);
    ssize_t (*get_in_status) (unsigned int index, char *buf);
    bool (*get_in_alarm) (unsigned int index, long *val);
    ssize_t (*get_curr_alias) (unsigned int index, char *buf);
    ssize_t (*get_curr_type) (unsigned int index, char *buf);
    bool (*get_curr_max) (unsigned int index, long *val);
    bool (*get_curr_min) (unsigned int index, long *val);
    bool (*get_curr_input) (unsigned int index, long *val);
    void (*get_loglevel) (long *lev);
    void (*set_loglevel) (long lev);
    ssize_t (*debug_help) (char *buf);
    ssize_t (*debug) (const char *buf, int count);
};

ssize_t drv_get_temp_alias(unsigned int index, char *buf);
ssize_t drv_get_temp_type(unsigned int index, char *buf);
bool drv_get_temp_max(unsigned int index, long *val);
bool drv_get_temp_max_hyst(unsigned int index, long *val);
bool drv_get_temp_min(unsigned int index, long *val);
bool drv_set_temp_max(unsigned int index, long val);
bool drv_set_temp_max_hyst(unsigned int index, long val);
bool drv_set_temp_min(unsigned int index, long val);
bool drv_get_temp_input(unsigned int index, long *temp_input);
ssize_t drv_get_temp_status(unsigned int index, char *buf);
ssize_t drv_get_in_alias(unsigned int index, char *buf);
ssize_t drv_get_in_type(unsigned int index, char *buf);
bool drv_get_in_max(unsigned int index, long *val);
bool drv_get_in_min(unsigned int index, long *val);
ssize_t drv_get_in_status(unsigned int index, char *buf);
bool drv_get_in_input(unsigned int index, long *in_input);
bool drv_get_in_alarm(unsigned int index, long *val);
ssize_t drv_get_curr_alias(unsigned int index, char *buf);
ssize_t drv_get_curr_type(unsigned int index, char *buf);
bool drv_get_curr_max(unsigned int index, long *val);
bool drv_get_curr_min(unsigned int index, long *val);
bool drv_get_curr_input(unsigned int index, long *curr_input);
void drv_get_loglevel(long *lev);
void drv_set_loglevel(long lev);
ssize_t drv_debug_help(char *buf);
ssize_t drv_debug(const char *buf, int count);

void hisonic_sensor_drivers_register(struct sensor_drivers_t *p_func);
void hisonic_sensor_drivers_unregister(void);
void kssonic_sensor_drivers_register(struct sensor_drivers_t *p_func);
void kssonic_sensor_drivers_unregister(void);
void mxsonic_sensor_drivers_register(struct sensor_drivers_t *p_func);
void mxsonic_sensor_drivers_unregister(void);
void s3ip_sensor_drivers_register(struct sensor_drivers_t *p_func);
void s3ip_sensor_drivers_unregister(void);

#endif /* SWITCH_SENSOR_DRIVER_H */
