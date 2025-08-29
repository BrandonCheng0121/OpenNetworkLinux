#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#include "cpld_lpc_driver.h"
#include "cpld_i2c_driver.h"
#include "switch_wdt_driver.h"
#include "switch_cpld_driver.h"

#define DRVNAME "drv_cpld_driver"
#define SWITCH_CPLD_DRIVER_VERSION "0.0.1"

#define PREVIOUS_REBOOT_CAUSE_FILE   "/host/reboot-cause/previous-reboot-cause.json"

#define SYS_CPLD_POWER_LOSS_REG  (0x81)
#define SYS_CPLD_POWER_LOSS_VAL  (0x79)
int power_loss_val = 0;
unsigned int loglevel;
module_param(loglevel,int,S_IRUGO);

static struct platform_device *drv_cpld_device;
bool is_wdt_trigger = false;

// static const char *cpld_str[] = {
//     "cpu board cpld",
//     "switch board led_cpld",
// };

static const char *cpld_alias_name[CPLD_TOTAL_NUM] = {
    "sys_cpld",
    "led_cpld1",
};

static const char *cpld_type_name[CPLD_TOTAL_NUM] = {
    "Lattice",// Lattice LCMXO3LF-9400C-5BG400C",
    "Lattice",// Lattice LCMXO3LF-2100C-5BG324C",
};

static const char CPLD_I2C_ADDR[CPLD_TOTAL_NUM] = {0x62,0x64};

enum cpld_interface_type {
    CPLD_INTERFACE_DEFAULT = 0, //Default syscpld:lpc  portcpld:i2c
    CPLD_INTERFACE_LPC = 1,
    CPLD_INTERFACE_I2C = 2
};

enum reboot_cause {
    REBOOT_CAUSE_NON_HARDWARE,
    REBOOT_CAUSE_POWER_LOSS,
    REBOOT_CAUSE_THERMAL_OVERLOAD_CPU,
    REBOOT_CAUSE_THERMAL_OVERLOAD_ASIC,
    REBOOT_CAUSE_THERMAL_OVERLOAD_OTHER,
    REBOOT_CAUSE_INSUFFICIENT_FAN_SPEED,
    REBOOT_CAUSE_WATCHDOG,
    REBOOT_CAUSE_HARDWARE_OTHER,
    REBOOT_CAUSE_CPU_COLD_RESET,
    REBOOT_CAUSE_CPU_WARM_RESET,
    REBOOT_CAUSE_BIOS_RESET,
    REBOOT_CAUSE_PSU_SHUTDOWN,
    REBOOT_CAUSE_BMC_SHUTDOWN,
    REBOOT_CAUSE_RESET_BUTTON_SHUTDOWN,
    REBOOT_CAUSE_RESET_BUTTON_COLD_SHUTDOWN,
    REBOOT_CAUSE_CPU_EC_NO_HB,
    REBOOT_CAUSE_CPU_CPU_ERROR
};

static int cpld_read(enum cpld_interface_type type, enum cpld_type device_id, u8 reg)
{
    u8 cpld_addr = 0;
    if(device_id == sys_cpld) //suport lpc and i2c
    {
        switch (type)
        {
        case CPLD_INTERFACE_LPC:
            return lpc_read_cpld( LCP_DEVICE_ADDRESS1, reg);
            break;
        case CPLD_INTERFACE_I2C:
            cpld_addr = CPLD_I2C_ADDR[device_id];
            return _i2c_cpld_read(cpld_addr, device_id, reg);
            break;
        default:
            return lpc_read_cpld( LCP_DEVICE_ADDRESS1, reg);
            break;
        }
    }
    else if(device_id == led_cpld1) //suport i2c
    {
        cpld_addr = CPLD_I2C_ADDR[device_id];
        return _i2c_cpld_read(cpld_addr, device_id, reg);
    }
    return -1;
}

static int cpld_write(enum cpld_interface_type type, enum cpld_type device_id, u8 reg, u8 value)
{
    u8 cpld_addr = 0;
    if(device_id == sys_cpld) //suport lpc and i2c
    {
        switch (type)
        {
        case CPLD_INTERFACE_LPC:
            return lpc_write_cpld( LCP_DEVICE_ADDRESS1, reg, value);
            break;
        case CPLD_INTERFACE_I2C:
            cpld_addr = CPLD_I2C_ADDR[device_id];
            return _i2c_cpld_write(cpld_addr, device_id, reg, value);
            break;
        default:
            return lpc_write_cpld( LCP_DEVICE_ADDRESS1, reg, value);
            break;
        }
    }
    else if(device_id == led_cpld1) //suport i2c
    {
        cpld_addr = CPLD_I2C_ADDR[device_id];
        return _i2c_cpld_write(cpld_addr, device_id, reg, value);
    }
    return -1;
}

static int sys_cpld_write(u8 reg,u8 value)
{
    return cpld_write(CPLD_INTERFACE_DEFAULT, sys_cpld, reg, value);
}

static int sys_cpld_read(u8 reg)
{
    return cpld_read(CPLD_INTERFACE_DEFAULT, sys_cpld, reg);
}

int read_whole_file(struct file *filp, char *content)
{
    loff_t pos = 0;
    char tmp_buf[1];
    int i = 0;

    while(kernel_read(filp, tmp_buf, 1, &pos) == 1)
    {
        if(tmp_buf[0] != '\n')
        {
            content[i] = tmp_buf[0];
            i++;
        }
    }

    return i;
}

bool check_if_reboot_cause_power_loss(void)
{
    if(power_loss_val == SYS_CPLD_POWER_LOSS_VAL)
        return false;
    else if(power_loss_val == 0)
        return true;
    return false;
}

void check_if_reboot_cause_watchdog(void)
{
    int cpu_type = 0;
    cpu_type = get_cpu_name();
    if(cpu_type == CPU_TYPE_D1627)
    {
        if(((inb(EC_LPC_IO_BASE_ADDR + EC_WDT_CFG_REG_OFFSET) >> 7) & 0x1) == 1)
        {
            outb(WDT_CFG_WDT_CLEAR, (EC_LPC_IO_BASE_ADDR + EC_WDT_CFG_REG_OFFSET)); //Clear previous watchdog trigger report
            is_wdt_trigger = true;
        }
    }else // C3508
    {
        if(sys_cpld_read(C3508_CPLD_WDT_TRG_REG_OFFSET) & BIT7)
        {
            sys_cpld_write(C3508_CPLD_WDT_TRG_REG_OFFSET, sys_cpld_read(C3508_CPLD_WDT_TRG_REG_OFFSET) | (BIT7));//Clear previous watchdog trigger report
            is_wdt_trigger = true;
        }
    }

    return;
}

int check_if_reboot_cause_warm_reset(void)
{
    struct file *fp = NULL;
    int err = 0;
    char content[1024];

    fp = filp_open(PREVIOUS_REBOOT_CAUSE_FILE, O_RDONLY, 0);
    if(IS_ERR(fp))
    {
        CPLD_ERR("Open %s failed\n", PREVIOUS_REBOOT_CAUSE_FILE);

        err = PTR_ERR(fp);
        return -EBADF;
    }

    if(!(read_whole_file(fp, content)))
    {
        CPLD_ERR("Read %s failed\n", PREVIOUS_REBOOT_CAUSE_FILE);
        return -EBADF;
    }
    filp_close(fp, NULL);

    if(strstr(content, "Unknown"))
        return REBOOT_CAUSE_NON_HARDWARE;
    else if(strstr(content, "reboot") || strstr(content, "warm-reboot") || strstr(content, "fast-reboot"))
        return REBOOT_CAUSE_CPU_WARM_RESET;
    else
        return -EINVAL;
}

#if 0
bool check_if_reboot_cause_bios(void)
{
    unsigned char val;

    val = inb(EC_LPC_IO_BASE_ADDR + EC_BIOS_SELECT_STATUS_REG_OFFSET);
    if((val == 0x03) || (val == 0x30))
        return true;

    return false;
}


int check_if_reboot_cause_cold_reset(void)
{
    unsigned int val;
    unsigned char buf[2];

    lpc_read_cpld(0, CPLD_REG_CPU_COOL_RST_RECORD_OFFSET, buf);
    val = (buf[1] << 8) | buf[0];
    
    if(((val >> 1) & 0x1) == 1)
        return REBOOT_CAUSE_CPU_EC_NO_HB;
    else if(((val >> 2) & 0x1) == 1)
        return REBOOT_CAUSE_CPU_CPU_ERROR;
    else if(((val >> 3) & 0x1) == 1)
        return REBOOT_CAUSE_BMC_SHUTDOWN;
    else
        return -EINVAL;
}

ssize_t drv_get_sw_version(char *buf)
{
    unsigned int ver;
    unsigned int cs;
    unsigned char val[2];
    int len, count=0;
    char *temp;

    temp = (char *) kzalloc(128 * CPLD_TOTAL_NUM, GFP_KERNEL);
    if(!temp)
    {
        CPLD_DEBUG("[%s]: Memory allocate failed.\n", __func__);
        return -ENOMEM;
    }

    for(cs=0; cs<CPLD_TOTAL_NUM; cs++)
    {
        lpc_read_cpld(CPLD_VER_OFFSET, val);
        ver = (val[1] << 8) | val[0];
        len = sprintf_s(temp + count, 128 * CPLD_TOTAL_NUM, "%s version: 0x%04x", cpld_str[cs] , ver);
        count += len;
    }
    len = sprintf_s(buf, PAGE_SIZE, "%s\n", temp);

    kfree(temp);

    return len;
}
#endif

ssize_t drv_get_reboot_cause(char *buf)
{
    int ret;

    // REBOOT_CAUSE_WATCHDOG
    check_if_reboot_cause_watchdog();
    if(is_wdt_trigger == true)
        return sprintf_s(buf, PAGE_SIZE, "%d\n", REBOOT_CAUSE_WATCHDOG);
#if 0
    ret = check_if_reboot_cause_cold_reset();
    if(ret == REBOOT_CAUSE_CPU_EC_NO_HB)
        return sprintf_s(buf, PAGE_SIZE, "%d\n", REBOOT_CAUSE_CPU_EC_NO_HB);
    else if(ret == REBOOT_CAUSE_CPU_CPU_ERROR)
        return sprintf_s(buf, PAGE_SIZE, "%d\n", REBOOT_CAUSE_CPU_CPU_ERROR);
    else if(ret == REBOOT_CAUSE_BMC_SHUTDOWN)
        return sprintf_s(buf, PAGE_SIZE, "%d\n", REBOOT_CAUSE_BMC_SHUTDOWN);
#endif
    // REBOOT_CAUSE_POWER_LOSS
    if(check_if_reboot_cause_power_loss() == true)
        return sprintf_s(buf, PAGE_SIZE, "%d\n", REBOOT_CAUSE_POWER_LOSS);
#if 0
    // REBOOT_CAUSE_CPU_WARM_RESET
    ret = check_if_reboot_cause_warm_reset();
    if(ret == REBOOT_CAUSE_NON_HARDWARE)
        return sprintf_s(buf, PAGE_SIZE, "%d\n", REBOOT_CAUSE_NON_HARDWARE);
    else if(ret == REBOOT_CAUSE_CPU_WARM_RESET)
        return sprintf_s(buf, PAGE_SIZE, "%d\n", REBOOT_CAUSE_CPU_WARM_RESET);

    // REBOOT_CAUSE_BIOS_RESET
    if(check_if_reboot_cause_bios() == true)
        return sprintf_s(buf, PAGE_SIZE, "%d\n", REBOOT_CAUSE_BIOS_RESET);
#endif

    // For S3IP always return success with NA for HW fail.
    ERROR_RETURN_NA(-1);
}

ssize_t drv_get_alias(unsigned int index, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%s\n", cpld_alias_name[index]);
}

ssize_t drv_get_type(unsigned int index, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%s\n", cpld_type_name[index]);
}

ssize_t drv_get_hw_version(unsigned int index, char *buf)
{
    unsigned int ver_major_value = 0;
    unsigned int ver_minor_value = 0;
    unsigned int offset = 0;
    unsigned int device_id = index;

    offset = CPLD_HW_VERSION_OFFSET;

    ver_major_value = cpld_read(CPLD_INTERFACE_DEFAULT, device_id, offset);
    if(ver_major_value < 0)
    {
        return -1;
    }

    ver_minor_value = cpld_read(CPLD_INTERFACE_DEFAULT, device_id, offset+1);
    if(ver_minor_value < 0)
    {
        return -1;
    }

    return sprintf_s(buf, PAGE_SIZE, "%x.%x\n", ver_major_value, ver_minor_value);
}

ssize_t drv_get_board_version(unsigned int index, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%s\n", "NA");
}

ssize_t drv_get_fmea_selftest_status(unsigned int index, char *buf)
{
    unsigned short test;
    unsigned short val;
    unsigned int cpld_addr = 0, offset = 0, device_id = index;

    if(sys_cpld == device_id)
    {
        //lpc check
        offset = SYS_CPLD_TEST_REG_OFFSET;
        test = 0x55;
        sys_cpld_write(offset, test);
        val = sys_cpld_read(offset);
        if((sys_cpld == device_id) || (led_cpld1 == device_id))
            test = 0xaa;
        if(test != val)
            return sprintf_s(buf, PAGE_SIZE, "0x1 Selftest lpc Failed, Reg(0x%02x) = 0x%02x\n", offset, val);
        test = 0xaa;
        sys_cpld_write(offset, test);
        val = sys_cpld_read(offset);
        if((sys_cpld == device_id) || (led_cpld1 == device_id))
            test = 0x55;
        if(test != val)
            return sprintf_s(buf, PAGE_SIZE, "0x1 Selftest lpc Failed, Reg(0x%02x) = 0x%02x\n", offset, val);
    }
    else if(led_cpld1 == device_id)
        offset = LED1_CPLD_TEST_REG_OFFSET;
    else
        ERROR_RETURN_NA(-1);

    //i2c check
    cpld_addr = CPLD_I2C_ADDR[device_id];
    test = 0x55;
    cpld_write(CPLD_INTERFACE_I2C, device_id, offset, test);
    val = cpld_read(CPLD_INTERFACE_I2C, device_id, offset);
    if((sys_cpld == device_id) || (led_cpld1 == device_id))
        test = 0xaa;
    if(test != val)
        return sprintf_s(buf, PAGE_SIZE, "0x1 Selftest i2c Failed, Reg(0x%02x) = 0x%02x\n", offset, val);
    test = 0xaa;
    cpld_write(CPLD_INTERFACE_I2C, device_id, offset, test);
    val = cpld_read(CPLD_INTERFACE_I2C, device_id, offset);
    if((sys_cpld == device_id) || (led_cpld1 == device_id))
        test = 0x55;
    if(test != val)
        return sprintf_s(buf, PAGE_SIZE, "0x1 Selftest i2c Failed, Reg(0x%02x) = 0x%02x\n", offset, val);

    return sprintf_s(buf, PAGE_SIZE, "0x0\n");
}

ssize_t drv_get_fmea_corrosion_status(unsigned int index, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "NA\n");
}

ssize_t drv_get_fmea_voltage_status(unsigned int index, char *buf)
{
    unsigned int status1, status2;
    unsigned char reg;
    unsigned int result;
    unsigned int device_id = index;

    if(device_id == sys_cpld)
    {
        // get PG_ALARM_LATCH012 power good
        status1 = sys_cpld_read(CPLD_HIS_STATE_PWR1_OFFSET);
        status2 = sys_cpld_read(CPLD_HIS_STATE_PWR2_OFFSET);
        // clean PG_ALARM flag
        for (reg = CPLD_HIS_STATE_CLR_START_OFFSET; reg<=CPLD_HIS_STATE_CLR_END_OFFSET; reg++)
            sys_cpld_read(reg);
        sys_cpld_read(CPLD_HIS_STATE_CLR_PG10_OFFSET);

        if((status1 != 0) || (status2 != 0))
            result = 1;
        else
            result = 0;
        return sprintf_s(buf, PAGE_SIZE, "%d 0x%02x%02x\n", result, status1, status2);
    }
    else
        return -EINVAL;
}

ssize_t drv_get_fmea_clock_status(unsigned int index, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "NA\n");
#if 0
    unsigned int val;
    int result;
    unsigned int device_id = index;
    if(device_id == sys_fpga)
    {
        val = fpga_pcie_read(FPGA_CLK_MONITOR_LATCH_OFFSET);
        if(val != 0x0)
            result = 1;
        else
            result = 0;
        fpga_pcie_write(FPGA_CLK_MONITOR_LATCH_OFFSET, 0x3);
        return sprintf_s(buf, PAGE_SIZE, "%d 0x%04x\n", result, val);
    }
    else
        return -EINVAL;
#endif
}

ssize_t drv_get_fmea_battery_status(char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "NA\n");
}

ssize_t drv_get_fmea_reset(char *buf)
{
    // just cpld2 suport  cpld1 reset cpld2
    int read_val = 0;
    read_val = sys_cpld_read(CPLD1_RESET_REG_OFFSET);
    if(read_val < 0)
        return sprintf_s(buf, PAGE_SIZE, "NA\n");
    
    if(read_val & CPLD1_RESET_CPLD2_BIT)
        return sprintf_s(buf, PAGE_SIZE, "%d\n", CPLD2_STATUS_NO_RESET);
    else
        return sprintf_s(buf, PAGE_SIZE, "%d\n", CPLD2_STATUS_RESET);
}

bool drv_set_fmea_reset(int reset)
{
    // just cpld2 suport  cpld1 reset cpld2
    int read_val = 0;
    int write_val = 0;

    if(reset == CPLD2_STATUS_RESET)
    {
        read_val = sys_cpld_read(CPLD1_RESET_REG_OFFSET);
        if(read_val < 0)
            return -EINVAL;
        write_val =  (read_val & (~(CPLD1_RESET_CPLD2_BIT))) & 0xff;
        sys_cpld_write(CPLD1_RESET_REG_OFFSET, write_val);
    }
    else
    {
        read_val = sys_cpld_read(CPLD1_RESET_REG_OFFSET);
        if(read_val < 0)
            return -EINVAL;
        write_val = (read_val | CPLD1_RESET_CPLD2_BIT) & 0xff;
        sys_cpld_write(CPLD1_RESET_REG_OFFSET, write_val);
    }

    return true;
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

ssize_t drv_debug_help(char *buf)
{
    return sprintf_s(buf, PAGE_SIZE,
        "Use the following command to debug:\n"
        "i2cget -y -f <bus> <addr> <reg>; i2cset -y -f <bus> <addr> <reg> <val>\n"
        "sys_cpld: i2cget -y -f 157 0x62 <reg>; i2cset -y -f 157 0x62 <reg> <val>\n"
        "port_cpld: i2cget -y -f 158 0x64 <reg>; i2cset -y -f 158 0x64 <reg> <val>\n");
}

ssize_t drv_debug(const char *buf, int count)
{
    return 0;
}

static struct cpld_drivers_t pfunc = {
    .get_reboot_cause = drv_get_reboot_cause,
    .get_alias = drv_get_alias,
    .get_type = drv_get_type,
    .get_hw_version = drv_get_hw_version,
    .get_board_version = drv_get_board_version,
    .get_fmea_selftest_status = drv_get_fmea_selftest_status,
    .get_fmea_corrosion_status = drv_get_fmea_corrosion_status,
    .get_fmea_voltage_status = drv_get_fmea_voltage_status,
    .get_fmea_clock_status = drv_get_fmea_clock_status,
    .get_fmea_battery_status = drv_get_fmea_battery_status,
    .get_fmea_reset = drv_get_fmea_reset,
    .set_fmea_reset = drv_set_fmea_reset,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
    .debug_help = drv_debug_help,
};

static int drv_cpld_probe(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    s3ip_cpld_drivers_register(&pfunc);
    mxsonic_cpld_drivers_register(&pfunc);
#else
    hisonic_cpld_drivers_register(&pfunc);
    kssonic_cpld_drivers_register(&pfunc);
#endif
    return 0;
}

static int drv_cpld_remove(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    s3ip_cpld_drivers_unregister();
    mxsonic_cpld_drivers_unregister();
#else
    hisonic_cpld_drivers_unregister();
    kssonic_cpld_drivers_unregister();
#endif

    return 0;
}

static struct platform_driver drv_cpld_driver = {
    .driver = {
        .name   = DRVNAME,
        .owner = THIS_MODULE,
    },
    .probe      = drv_cpld_probe,
    .remove     = drv_cpld_remove,
};

static int __init drv_cpld_init(void)
{
    int err;
    int retval;

    drv_cpld_device = platform_device_alloc(DRVNAME, 0);
    if(!drv_cpld_device)
    {
        CPLD_DEBUG("%s(#%d): platform_device_alloc fail\n", __func__, __LINE__);
        return -ENOMEM;
    }

    retval = platform_device_add(drv_cpld_device);
    if(retval)
    {
        CPLD_DEBUG("%s(#%d): platform_device_add failed\n", __func__, __LINE__);
        err = -retval;
        goto dev_add_failed;
    }

    retval = platform_driver_register(&drv_cpld_driver);
    if(retval)
    {
        CPLD_DEBUG("%s(#%d): platform_driver_register failed(%d)\n", __func__, __LINE__, err);
        err = -retval;
        goto dev_reg_failed;
    }

    power_loss_val = sys_cpld_read(SYS_CPLD_POWER_LOSS_REG);
    sys_cpld_write(SYS_CPLD_POWER_LOSS_REG, SYS_CPLD_POWER_LOSS_VAL);

    return 0;

dev_reg_failed:
    platform_device_unregister(drv_cpld_device);
    return err;

dev_add_failed:
    platform_device_put(drv_cpld_device);
    return err;
}

static void __exit drv_cpld_exit(void)
{
    platform_driver_unregister(&drv_cpld_driver);
    platform_device_unregister(drv_cpld_device);

    return;
}

MODULE_DESCRIPTION("Huarong CPLD Driver");
MODULE_VERSION(SWITCH_CPLD_DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(drv_cpld_init);
module_exit(drv_cpld_exit);
