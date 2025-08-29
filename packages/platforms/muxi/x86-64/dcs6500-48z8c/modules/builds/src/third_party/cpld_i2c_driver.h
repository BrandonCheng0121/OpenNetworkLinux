#ifndef CPLD_I2C_DRIVER_H
#define CPLD_I2C_DRIVER_H

#if 1
#define CPLD_I2C_DEBUG(...)
#else
#define CPLD_I2C_DEBUG(...) printk(KERN_ALERT __VA_ARGS__)
#endif

#define SYS_CPLD_I2C_ADDRS  (0x10)

enum cpld_type {
    sys_cpld = 0,
    led_cpld1,
    max_type
};

int _i2c_cpld_read(u8 cpld_addr,u8 device_id ,u8 reg);

int _i2c_cpld_write(u8 cpld_addr,u8 device_id , u8 reg, u8 value);

#endif /* CPLD_I2C_DRIVER_H */

