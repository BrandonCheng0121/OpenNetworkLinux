#ifndef FPGA_DRIVER_H
#define FPGA_DRIVER_H

// #if 1
// #define FPGA_DEBUG(...)
// #else
// #define FPGA_DEBUG(...) printk(KERN_ALERT __VA_ARGS__)
// #endif

int fpga_pcie_write( u32 reg, u32 value);
int fpga_pcie_read( u32 reg);

#endif /* FPGA_DRIVER_H */

