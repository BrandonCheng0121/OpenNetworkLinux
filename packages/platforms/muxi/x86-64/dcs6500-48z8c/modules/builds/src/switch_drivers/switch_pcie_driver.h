#ifndef SWITCH_PCIE_DRIVER_H
#define SWITCH_PCIE_DRIVER_H

#include "switch.h"

#define PCIE_ERR(fmt, args...)      LOG_ERR("pcie: ", fmt, ##args)
#define PCIE_WARNING(fmt, args...)  LOG_WARNING("pcie: ", fmt, ##args)
#define PCIE_INFO(fmt, args...)     LOG_INFO("pcie: ", fmt, ##args)
#define PCIE_DEBUG(fmt, args...)    LOG_DBG("pcie: ", fmt, ##args)

#define CHECK_BIT(a, b)  (((a) >> (b)) & 1U)

struct pci_dev* fpga_ich_dev = NULL;
struct pci_dev* lsw_ich_dev = NULL;
struct pci_dev* lan_ich_dev = NULL;

#define FPGA_ICH_VENDOR_ID			0x8086
#define FPGA_ICH_DEVICE_ID			0x8C10

#define LSW_ICH_VENDOR_ID 					0x8086
#define LSW_ICH_DEVICE_ID 					0x6F08

#define LAN_ICH_VENDOR_ID 					0x8086
#define LAN_ICH_DEVICE_ID 					0x8C1E

#define FPGA_ICH_LINK_STATUS_REG_OFFSET     0x52
#define LSW_ICH_LINK_STATUS_REG_OFFSET		0xA2
#define LAN_ICH_LINK_STATUS_REG_OFFSET		0x52

#define BUFFER_SIZE				64
#define FALSE_VALUE				1

#define LINK_STATUS_MASK			0x2000
#define LANE_STATUS_MASK			0x3f0
#define SPEED_STATUS_MASK			0xf
#define WIDTH_1			0x10    /*Widthx1*/
#define WIDTH_2			0x20    /*Widthx2*/
#define WIDTH_4			0x40    /*Widthx4*/
#define WIDTH_8			0x80    /*Widthx8*/
#define WIDTH_16			0x100   /*Widthx16*/
#define SPEED_VALUE_1			1   /*2.5Gbps*/
#define SPEED_VALUE_2			2   /*5Gbps*/
#define SPEED_VALUE_3			3   /*8Gbps*/
#define LINK_STATUS_NAME     "link_status"
#define SPEED_STATUS_NAME     "speed_status"

#define WIDTH_1_STR			"Widthx1"    /*Widthx1*/
#define WIDTH_2_STR			"Widthx2"    /*Widthx2*/
#define WIDTH_4_STR			"Widthx4"    /*Widthx4*/
#define WIDTH_8_STR			"Widthx8"    /*Widthx8*/
#define WIDTH_16_STR		"Widthx16"   /*Widthx16*/
#define SPEED_2_5GBPS		"2.5Gbps"   /*2.5Gbps*/
#define SPEED_5GBPS		"5Gbps"   /*5Gbps*/
#define SPEED_8GBPS		"8Gbps"   /*8Gbps*/

struct pcie_drivers_t{
    ssize_t (*fpga_link_status_read)(u32 reg,char* buf);
    ssize_t (*fpga_speed_status_read)(u32 reg,char* buf);
    ssize_t (*lsw_link_status_read)(u32 reg,char* buf);
    ssize_t (*lsw_speed_status_read)(u32 reg,char* buf);
    ssize_t (*lan_link_status_read)(u32 reg,char* buf);
    ssize_t (*lan_speed_status_read)(u32 reg,char* buf);
    void (*get_loglevel) (long *lev);
    void (*set_loglevel) (long lev);
};

ssize_t drv_fpga_link_status_read(u32 reg, char* buf);
ssize_t drv_fpga_speed_status_read(u32 reg, char* buf);
ssize_t drv_lsw_link_status_read(u32 reg, char* buf);
ssize_t drv_lsw_speed_status_read(u32 reg, char* buf);
ssize_t drv_lan_link_status_read(u32 reg, char* buf);
ssize_t drv_lan_speed_status_read(u32 reg, char* buf);
void drv_get_loglevel(long *lev);
void drv_set_loglevel(long lev);

void mxsonic_pcie_drivers_register(struct pcie_drivers_t *p_func);
void mxsonic_pcie_drivers_unregister(void);

#endif /* SWITCH_PCIE_DRIVER_H */