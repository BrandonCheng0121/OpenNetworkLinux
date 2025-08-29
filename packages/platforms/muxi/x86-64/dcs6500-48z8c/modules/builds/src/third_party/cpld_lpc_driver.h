#ifndef CPLD_LPC_DRIVER_H
#define CPLD_LPC_DRIVER_H

#define LCP_DEVICE_ADDRESS1 0xfc801000
#define LCP_DEVICE_ADDRESS2 0xfc802000

int lpc_read_cpld(u32 lpc_device, u8 reg);

int lpc_write_cpld(u32 lpc_device, u8 reg, u8 value);

#endif /* CPLD_LPC_DRIVER_H */
