#ifndef SWITCH_FPGA_DRIVER_H
#define SWITCH_FPGA_DRIVER_H

#include "switch.h"

#if 0
#define FPGA_ERR(...)
#define FPGA_WARNING(...)
#define FPGA_INFO(...)
#define FPGA_DEBUG(...)
#else
#define FPGA_ERR(fmt, args...)      LOG_ERR("fpga: ", fmt, ##args)
#define FPGA_WARNING(fmt, args...)  LOG_WARNING("fpga: ", fmt, ##args)
#define FPGA_INFO(fmt, args...)     LOG_INFO("fpga: ", fmt, ##args)
#define FPGA_DEBUG(fmt, args...)    LOG_DBG("fpga: ", fmt, ##args)

#endif

#define FPGA_TOTAL_NUM          1
#define FPGA_INDEX_START        1
#define FPGA_SF_OFFSET          0x04
#define FPGA_VER_OFFSET         0x00
#define FPGA_TEST_REG_OFFSET    0x04
#define FPGA_CLK_MONITOR_LATCH_OFFSET   0x14
#define FPGA_CLK0_ALARM_CLR_OFFSET      0x18
#define FPGA_CLK1_ALARM_CLR_OFFSET      0x1C

#define FPGA_REG_TCORROSION_DET         0x44
#define FPGA_MASK_TCORROSION_DET    0x7
#define FPGA_MASK_VULCANIZATION     0x4
#define FPGA_OFFSET_VULCANIZATION     2
#define FPGA_MASK_CORROSION         0x2
#define FPGA_OFFSET_CORROSION         1
#define FPGA_MASK_WET_DUST          0x1
#define FPGA_OFFSET_WET_DUST          0

#define VULCANIZATION_NORMAL    1
#define VULCANIZATION_ABNORMAL  0
#define CORROSION_NORMAL        0
#define CORROSION_ABNORMAL      1
#define WET_DUST_NORMAL         1
#define WET_DUST_ABNORMAL       0

#define S3IP_FMEA_VULCANIZATION     0x4
#define S3IP_FMEA_CORROSION         0x2
#define S3IP_FMEA_WET_DUST          0x1
struct fpga_drivers_t{
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

void mxsonic_fpga_drivers_register(struct fpga_drivers_t *p_func);
void mxsonic_fpga_drivers_unregister(void);
void s3ip_fpga_drivers_register(struct fpga_drivers_t *p_func);
void s3ip_fpga_drivers_unregister(void);

#endif /* SWITCH_FPGA_DRIVER_H */
