#ifndef SWITCH_CPLD_DRIVER_H
#define SWITCH_CPLD_DRIVER_H

#include "switch.h"

#if 0
#define CPLD_ERR(...)
#define CPLD_WARNING(...)
#define CPLD_INFO(...)
#define CPLD_DEBUG(...)
#else
#define CPLD_ERR(fmt, args...)      LOG_ERR("cpld: ", fmt, ##args)
#define CPLD_WARNING(fmt, args...)  LOG_WARNING("cpld: ", fmt, ##args)
#define CPLD_INFO(fmt, args...)     LOG_INFO("cpld: ", fmt, ##args)
#define CPLD_DEBUG(fmt, args...)    LOG_DBG("cpld: ", fmt, ##args)
#endif

#define CPLD_INDEX_START            1

#define CPLD_TOTAL_NUM              2
#define CPLD_VER_OFFSET                     0x03
#define CPLD_HW_VERSION_OFFSET              0x00
#define CPLD_BOARD_VERSION_OFFSET           0x00
#define CPLD_RST_SRC_RECORD_REG_1_OFFSET    0x11
#define SYS_CPLD_TEST_REG_OFFSET            0x05
#define LED1_CPLD_TEST_REG_OFFSET           0x05
#define CPLD_HIS_STATE_PWR1_OFFSET          0x53
#define CPLD_HIS_STATE_PWR2_OFFSET          0x54
#define CPLD_HIS_STATE_CLR_START_OFFSET     0x55
#define CPLD_HIS_STATE_CLR_END_OFFSET       0x5E
#define CPLD_HIS_STATE_CLR_PG10_OFFSET      0x79

// just cpld2 suport  cpld1 reset cpld2
#define CPLD1_RESET_REG_OFFSET 0x23
#define CPLD1_RESET_CPLD2_BIT BIT(3)
#define CPLD2_STATUS_RESET 1
#define CPLD2_STATUS_NO_RESET 0

#define EC_BIOS_SELECT_STATUS_REG_OFFSET    0x20

struct cpld_drivers_t{
    ssize_t (*get_sw_version) (char* buf);
    ssize_t (*get_reboot_cause) (char *buf);
    ssize_t (*get_alias) (unsigned int index, char *buf);
    ssize_t (*get_type) (unsigned int index, char *buf);
    ssize_t (*get_hw_version) (unsigned int index, char *buf);
    ssize_t (*get_board_version) (unsigned int index, char *buf);
    ssize_t (*get_fmea_selftest_status) (unsigned int index, char *buf);
    ssize_t (*get_fmea_corrosion_status) (unsigned int index, char *buf);
    ssize_t (*get_fmea_voltage_status) (unsigned int index, char *buf);
    ssize_t (*get_fmea_clock_status) (unsigned int index, char *buf);
    ssize_t (*get_fmea_battery_status) (char *buf);
    ssize_t (*get_fmea_reset) (char *buf);
    bool (*set_fmea_reset) (int reset);
    void (*get_loglevel) (long *lev);
    void (*set_loglevel) (long lev);
    ssize_t (*debug_help) (char *buf);
    ssize_t (*debug) (const char *buf, int count);
};

ssize_t drv_get_sw_version(char *buf);
ssize_t drv_get_reboot_cause(char *buf);
ssize_t drv_get_alias(unsigned int index, char *buf);
ssize_t drv_get_type(unsigned int index, char *buf);
ssize_t drv_get_hw_version(unsigned int index, char *buf);
ssize_t drv_get_board_version(unsigned int index, char *buf);
ssize_t drv_get_fmea_selftest_status(unsigned int index, char *buf);
ssize_t drv_get_fmea_corrosion_status(unsigned int index, char *buf);
ssize_t drv_get_fmea_voltage_status(unsigned int index, char *buf);
ssize_t drv_get_fmea_clock_status(unsigned int index, char *buf);
ssize_t drv_get_fmea_battery_status(char *buf);
ssize_t drv_get_fmea_reset(char *buf);
bool drv_set_fmea_reset(int reset);
void drv_get_loglevel(long *lev);
void drv_set_loglevel(long lev);
ssize_t drv_debug_help(char *buf);
ssize_t drv_debug(const char *buf, int count);

void hisonic_cpld_drivers_register(struct cpld_drivers_t *p_func);
void hisonic_cpld_drivers_unregister(void);
void kssonic_cpld_drivers_register(struct cpld_drivers_t *p_func);
void kssonic_cpld_drivers_unregister(void);
void mxsonic_cpld_drivers_register(struct cpld_drivers_t *p_func);
void mxsonic_cpld_drivers_unregister(void);
void s3ip_cpld_drivers_register(struct cpld_drivers_t *p_func);
void s3ip_cpld_drivers_unregister(void);

#endif /* SWITCH_CPLD_DRIVER_H */
