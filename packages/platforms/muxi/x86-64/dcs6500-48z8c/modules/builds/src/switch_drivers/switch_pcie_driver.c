#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/pci.h>

#include "switch_pcie_driver.h"

#define DRVNAME "drv_pcie_driver"
#define SWITCH_PCIE_DRIVER_VERSION "0.0.0.1"
unsigned int loglevel = 0;
static struct platform_device *drv_pcie_device;

int link_status_up(u16 reg_val)
{
    int value = 0;
    if(!(reg_val & LINK_STATUS_MASK))
    {
        value = FALSE_VALUE;
    }
    return value;
}

int link_status_and_lane_status_vlaue_get(u16 reg_val,int expect_width, char *lane_status)
{
    int value = 0;
    /*Link status: bit[13] = 1/0 (up/down)*/
    if(!(reg_val & LINK_STATUS_MASK))
    {
        value = FALSE_VALUE;
    }

    /*
       Lane status bit[9:4] = 000001 (x1)
                              000010 (x2)
                              000100 (x4)
                              001000 (x8)
                              010000 (x16)
    */

    if((reg_val & LANE_STATUS_MASK) != expect_width)
    {
        value = FALSE_VALUE;
    }

    switch(reg_val & LANE_STATUS_MASK)
    {
        case WIDTH_1 :
            sprintf_s(lane_status, PAGE_SIZE, "%s","Widthx1");
            break;
        case WIDTH_2 :
            sprintf_s(lane_status, PAGE_SIZE, "%s","Widthx2");
            break;
        case WIDTH_4 :
            sprintf_s(lane_status, PAGE_SIZE, "%s","Widthx4");
            break;
        case WIDTH_8 :
            sprintf_s(lane_status, PAGE_SIZE, "%s","Widthx8");
            break;
        case WIDTH_16 :
            sprintf_s(lane_status, PAGE_SIZE, "%s","Widthx16");
            break;
        default:
            PCIE_DEBUG("Mismatch Width reg:0x%x\n", reg_val);
            break;
    }

    return value;
}

int link_speed_status_vlaue_get(u16 reg_val,int expect_speed, char *speed_status)
{
    int value = 0;

    /*0001b 2.5Gbps, 0010b 5Gbps, 0011b 8Gbps*/
    if( (reg_val & SPEED_STATUS_MASK) != expect_speed )
    {
        value = FALSE_VALUE;
    }

    switch(reg_val & SPEED_STATUS_MASK)
    {
        case SPEED_VALUE_1 :
            sprintf_s(speed_status, PAGE_SIZE, "%s","2.5Gbps");
            break;
        case SPEED_VALUE_2 :
            sprintf_s(speed_status, PAGE_SIZE, "%s","5Gbps");
            break;
        case SPEED_VALUE_3 :
            sprintf_s(speed_status, PAGE_SIZE, "%s","8Gbps");
            break;
        default:
            PCIE_DEBUG("Mismatch Speed reg:0x%x\n", reg_val);
            break;
    }

    return value;
}

ssize_t drv_fpga_link_status_read(u32 reg, char *buf)
{
    int value = 0;
    int link_val = 0;
    char lane_status[BUFFER_SIZE] = "NA";
    u16 reg_val = 0;

    pci_read_config_word(fpga_ich_dev, reg, &reg_val);

    value = link_status_and_lane_status_vlaue_get(reg_val, WIDTH_1, lane_status);;
    link_val = link_status_up(reg_val);
    return sprintf_s(buf, PAGE_SIZE, "%d %s,expect:%s,lane_status:%s,reg:0x%x\n", value, link_val != FALSE_VALUE? "up":"down",WIDTH_1_STR,lane_status,reg_val);
}

ssize_t drv_fpga_speed_status_read(u32 reg, char *buf)
{
    int value = 0;
    u16 reg_val = 0;
    char speed_status[BUFFER_SIZE] = "NA";

    pci_read_config_word(fpga_ich_dev, reg, &reg_val);

    value = link_speed_status_vlaue_get(reg_val, SPEED_VALUE_2, speed_status);

    return sprintf_s(buf, PAGE_SIZE, "%d expect:%s,link_speed:%s,reg:0x%x\n", value, SPEED_5GBPS, speed_status,reg_val);
}

ssize_t drv_lsw_link_status_read(u32 reg, char *buf)
{
    int value = 0;
    int link_val = 0;
    u16 reg_val = 0;
    char lane_status[BUFFER_SIZE] = "NA";

    pci_read_config_word(lsw_ich_dev, reg, &reg_val);

    value = link_status_and_lane_status_vlaue_get(reg_val, WIDTH_4, lane_status);
    link_val = link_status_up(reg_val);
    return sprintf_s(buf, PAGE_SIZE, "%d %s,expect:%s,lane_status:%s,reg:0x%x\n", value, link_val != FALSE_VALUE? "up":"down",WIDTH_4_STR,lane_status,reg_val);
}

ssize_t drv_lsw_speed_status_read(u32 reg, char *buf)
{
    u32 value = 0;
    u16 reg_val = 0;
    char speed_status[BUFFER_SIZE] = "NA";

    pci_read_config_word(lsw_ich_dev, reg, &reg_val);

    value = link_speed_status_vlaue_get(reg_val, SPEED_VALUE_2, speed_status);

    return sprintf_s(buf, PAGE_SIZE, "%d expect:%s,link_speed:%s,reg:0x%x\n", value, SPEED_5GBPS, speed_status,reg_val);
}

ssize_t drv_lan_link_status_read(u32 reg, char *buf)
{
    u32 value = 0;
    int link_val = 0;
    u16 reg_val = 0;
    char lane_status[BUFFER_SIZE] = "NA";

    pci_read_config_word(lan_ich_dev, reg, &reg_val);

    value = link_status_and_lane_status_vlaue_get(reg_val, WIDTH_1, lane_status);
    link_val = link_status_up(reg_val);
    return sprintf_s(buf, PAGE_SIZE, "%d %s,expect:%s,lane_status:%s,reg:0x%x\n", value, link_val != FALSE_VALUE? "up":"down",WIDTH_1_STR,lane_status,reg_val);
}

ssize_t drv_lan_speed_status_read(u32 reg, char *buf)
{
    u32 value = 0;
    u16 reg_val = 0;
    char speed_status[BUFFER_SIZE] = "NA";

    pci_read_config_word(lan_ich_dev, reg, &reg_val);

    value = link_speed_status_vlaue_get(reg_val, SPEED_VALUE_1, speed_status);

    return sprintf_s(buf, PAGE_SIZE, "%d expect:%s,link_speed:%s,reg:0x%x\n", value, SPEED_2_5GBPS, speed_status,reg_val);
}

void drv_get_loglevel(long *lev)
{
    *lev = (long)loglevel;

    return;
}

void drv_set_loglevel(long lev)
{
    loglevel = (unsigned int)lev;

    return;
}

// For S3IP SONiC
static struct pcie_drivers_t pfunc = {
    .fpga_link_status_read  = drv_fpga_link_status_read,
    .fpga_speed_status_read = drv_fpga_speed_status_read,
    .lsw_link_status_read  = drv_lsw_link_status_read,
    .lsw_speed_status_read = drv_lsw_speed_status_read,
    .lan_link_status_read  = drv_lan_link_status_read,
    .lan_speed_status_read = drv_lan_speed_status_read,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
};

static int drv_pcie_probe(struct platform_device *pdev)
{
    mxsonic_pcie_drivers_register(&pfunc);

    return 0;
}

static int drv_pcie_remove(struct platform_device *pdev)
{
    mxsonic_pcie_drivers_unregister();

    return 0;
}

static struct platform_driver drv_pcie_driver = {
    .driver = {
        .name   = DRVNAME,
        .owner = THIS_MODULE,
    },
    .probe      = drv_pcie_probe,
    .remove     = drv_pcie_remove,
};


static int __init drv_pcie_init(void)
{
    
    int err;
    int retval;

    drv_pcie_device = platform_device_alloc(DRVNAME, 0);
    if(!drv_pcie_device)
    {
        PCIE_DEBUG("%s(#%d): platform_device_alloc fail\n", __func__, __LINE__);
        return -ENOMEM;
    }

    retval = platform_device_add(drv_pcie_device);
    if(retval)
    {
        PCIE_DEBUG("%s(#%d): platform_device_add failed\n", __func__, __LINE__);
        err = -retval;
        goto dev_add_failed;
    }

    retval = platform_driver_register(&drv_pcie_driver);
    if(retval)
    {
        PCIE_DEBUG("%s(#%d): platform_driver_register failed(%d)\n", __func__, __LINE__, err);
        err = -retval;
        goto dev_reg_failed;
    }

    fpga_ich_dev = pci_get_device(FPGA_ICH_VENDOR_ID, FPGA_ICH_DEVICE_ID, NULL);
    PCIE_DEBUG("%s: fpga_ich_dev = 0x%p\n", __func__, (void*)fpga_ich_dev);
    if(NULL == fpga_ich_dev) {
        printk(KERN_ERR "%s: failed to access FPGA controller.\n", __func__);
    }

    lsw_ich_dev = pci_get_device(LSW_ICH_VENDOR_ID, LSW_ICH_DEVICE_ID, NULL);
    PCIE_DEBUG("%s: lsw_ich_dev = 0x%p\n", __func__, (void*)lsw_ich_dev);
    if(NULL == lsw_ich_dev) {
        printk(KERN_ERR "%s: failed to access LSW controller.\n", __func__);
    }

    lan_ich_dev = pci_get_device(LAN_ICH_VENDOR_ID, LAN_ICH_DEVICE_ID, NULL);
    PCIE_DEBUG("%s: lan_ich_dev = 0x%p\n", __func__, (void*)lan_ich_dev);
    if(NULL == lan_ich_dev) {
        printk(KERN_ERR "%s: failed to access LAN controller.\n", __func__);
    }

    return 0;
dev_reg_failed:
    platform_device_unregister(drv_pcie_device);

dev_add_failed:
    platform_device_put(drv_pcie_device);
    return err;
}

static void __exit drv_pcie_exit(void)
{
    platform_driver_unregister(&drv_pcie_driver);
    platform_device_unregister(drv_pcie_device);

    return;
}


MODULE_DESCRIPTION("S3IP PCIE Driver");
MODULE_VERSION(SWITCH_PCIE_DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(drv_pcie_init);
module_exit(drv_pcie_exit);