#ifndef SYSFS_IPMI_H
#define SYSFS_IPMI_H

//unsigned int bmc_get_temp(unsigned int index, long *temp_input);
int drv_get_sensor_temp_input_from_bmc(unsigned int index, long *value);
int drv_get_sensor_in_input_from_bmc(unsigned int index, int pmbus_command, long *value);
int drv_get_sensor_curr_input_from_bmc(unsigned int index, int pmbus_command, long *value);
// bool drv_get_wind_from_bmc(unsigned int fan_index, unsigned int *wind);
ssize_t drv_get_model_name_from_bmc(unsigned int fan_index, char *buf);
ssize_t drv_get_sn_from_bmc(unsigned int fan_index, char *buf);
ssize_t drv_get_vendor_from_bmc(unsigned int fan_index, char *buf);
ssize_t drv_get_part_number_from_bmc(unsigned int fan_index, char *buf);
ssize_t drv_get_hw_version_from_bmc(unsigned int fan_index, char *buf);
// ssize_t drv_get_speed_from_bmc(unsigned int slot_id, unsigned int fan_id, unsigned int *speed);
// bool drv_get_pwm_from_bmc(unsigned int fan_id, int *pwm);
// bool drv_set_pwm_from_bmc(unsigned int fan_id, int pwm);
int drv_get_mfr_info_from_bmc(unsigned int psu_index, u8 pmbus_command, char *buf);
bool ipmi_bmc_is_ok(void);
ssize_t drv_fmea_get_work_status_from_bmc(unsigned int index, char *buf, char *plt);
ssize_t drv_fmea_get_current_status_from_bmc(unsigned int index, char *buf, char *plt);
ssize_t drv_fmea_get_pmbus_status_from_bmc(unsigned int index, char *buf, char *plt);
// ssize_t drv_fmea_get_mfr_id_from_bmc(unsigned int index, int *value);
#endif /*SYSFS_IPMI_H*/