#ifndef SWITCH_LED_DRIVER_H
#define SWITCH_LED_DRIVER_H

#include "switch.h"

#if 0
#define LED_ERR(...)
#define LED_WARNING(...)
#define LED_INFO(...)
#define LED_DEBUG(...)
#else
#define LED_ERR(fmt, args...)      LOG_ERR("led: ", fmt, ##args)
#define LED_WARNING(fmt, args...)  LOG_WARNING("led: ", fmt, ##args)
#define LED_INFO(fmt, args...)     LOG_INFO("led: ", fmt, ##args)
#define LED_DEBUG(fmt, args...)    LOG_DBG("led: ", fmt, ##args)

#endif

#define PORT_NUM (48+8)
//**************
#define CPLD_SYSLED_REG             0x78  //SYS LED = CPU LED
#define CPLD_SYSLED_OFFSET          0
#define CPLD_SYSLED_REG_MASK        0x07

#define CPLD_MSTLED_REG             0x78
#define CPLD_MSTLED_OFFSET          3
#define CPLD_MSTLED_REG_MASK        0x07

#define CPLD_IDLED_REG              0x2A
#define CPLD_IDLED_OFFSET           4
#define CPLD_IDLED_REG_MASK         0x01

#define CPLD_ACTLED_REG             0x78 //ACT LED = USB LED
#define CPLD_ACTLED_OFFSET          6
#define CPLD_ACTLED_REG_MASK        0x03

#define CPLD_FANLED_REG             0x65 //0x65-0x68
#define CPLD_FANLED_OFFSET          2
#define CPLD_FANLED_REG_MASK        0x03

#define SYS_LED_GREEN_ON            0x00
#define SYS_LED_RED_ON              0x01
#define SYS_LED_GREEN_FLASH_FAST    0x02
#define SYS_LED_GREEN_FLASH_SLOW    0x04

#define MST_LED_OFF                 0x07
#define MST_LED_GREEN_ON            0x05
#define MST_LED_RED_ON              0x06
#define MST_LED_YELLOW_ON           0x00
#define MST_LED_GREEN_FLASH         0x04

#define ID_LED_OFF                  0x00
#define ID_LED_BLUE_ON              0x01

#define ACT_LED_OFF                 0x03
#define ACT_LED_GREEN_ON            0x01
#define ACT_LED_RED_ON              0x02
#define ACT_LED_GREEN_FLASH_FAST    0x00

#define FAN_LED_OFF                 0x03
#define FAN_LED_GREEN_ON            0x01
#define FAN_LED_RED_ON              0x02
//**************

#define PORT_LED_OFF                 0x0
#define PORT_LED_GREEN_ON            0xa1
#define PORT_LED_GREEN_FLASH_ON      0xa3
#define PORT_LED_RED_ON              0xd1
#define PORT_LED_RED_FLASH_ON        0xd3
#define PORT_LED_BLUE_ON             0x61
#define PORT_LED_BLUE_FLASH_ON       0x63

struct led_drivers_t{
    bool (*get_location_led) (unsigned char *loc);
    bool (*set_location_led) (unsigned char loc);
    int  (*get_port_led_num) (void);
    bool (*get_sys_led) (unsigned int *led);
    bool (*set_sys_led) (unsigned int led);
    bool (*get_mst_led) (unsigned int *led);
    bool (*set_mst_led) (unsigned int led);
    bool (*get_bmc_led) (unsigned int *led);
    bool (*get_fan_led) (unsigned int *led);
    bool (*set_fan_led) (unsigned int led);
    bool (*get_psu_led) (unsigned int *led);
    bool (*set_psu_led) (unsigned int led);
    bool (*get_id_led) (unsigned int *led, unsigned int cs);
    bool (*set_id_led) (unsigned int led, unsigned int cs);
    bool (*get_act_led) (unsigned int *led);
    bool (*set_act_led) (unsigned int led);
    bool (*set_port_led) (unsigned int led, unsigned int index);
    bool (*get_port_led) (unsigned int *led, unsigned int index);
    bool (*set_hw_control) (void);
    void (*get_loglevel) (long *lev);
    void (*set_loglevel) (long lev);
    ssize_t (*debug_help) (char *buf);
    ssize_t (*debug) (const char *buf, int count);
};

bool drv_get_location_led(unsigned int *loc);
bool drv_set_location_led(unsigned int loc);
int drv_get_port_led_num(void);
bool drv_get_sys_led(unsigned int *led);
bool drv_set_sys_led(unsigned int led);
bool drv_get_mst_led(unsigned int *led);
bool drv_set_mst_led(unsigned int led);
bool drv_get_bmc_led(unsigned int *led);
bool drv_get_fan_led(unsigned int *led);
bool drv_set_fan_led(unsigned int led);
bool drv_get_psu_led(unsigned int *led);
bool drv_set_psu_led(unsigned int led);
bool drv_get_id_led(unsigned int *led, unsigned int cs);
bool drv_set_id_led(unsigned int led, unsigned int cs);
bool drv_get_act_led(unsigned int *led);
bool drv_set_act_led(unsigned int led);
bool drv_set_port_led (unsigned int led, unsigned int index);
bool drv_get_port_led (unsigned int *led, unsigned int index);

bool drv_set_hw_control(void);
void drv_get_loglevel(long *lev);
void drv_set_loglevel(long lev);
ssize_t drv_debug_help(char *buf);
ssize_t drv_debug(const char *buf, int count);

void hisonic_led_drivers_register(struct led_drivers_t *p_func);
void hisonic_led_drivers_unregister(void);
void kssonic_led_drivers_register(struct led_drivers_t *p_func);
void kssonic_led_drivers_unregister(void);
void mxsonic_led_drivers_register(struct led_drivers_t *p_func);
void mxsonic_led_drivers_unregister(void);
void s3ip_led_drivers_register(struct led_drivers_t *p_func);
void s3ip_led_drivers_unregister(void);

#endif /* SWITCH_LED_DRIVER_H */
