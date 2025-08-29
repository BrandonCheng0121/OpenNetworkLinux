#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <asm/io.h>

#include "switch_wdt_driver.h"

#define DRVNAME "drv_wdt_driver"

#define SWITCH_WDT_DRIVER_VERSION "0.0.1"

unsigned int loglevel = 0;
static struct platform_device *drv_wdt_device;

struct cpuid_result {
	uint32_t eax;
	uint32_t ebx;
	uint32_t ecx;
	uint32_t edx;
};

int g_cpu_type = CPU_TYPE_DEFAULT;
int c3508_timeout = 60;

int get_cpu_name(void)
{
    struct cpuid_result regs;
    unsigned int eax = 0;
    unsigned int i,j=0;
    char temp_processor_name[50] = {0};
    unsigned int *name_as_ints = (unsigned int *)temp_processor_name;
    if(g_cpu_type != CPU_TYPE_DEFAULT)
        return g_cpu_type;
    /**
    * eax == 0x800000000
    * If the CPU supports brand string, the value of > = 0x80000004 will be returned in eax.
    */
    asm volatile ("cpuid"
    : "=a"(eax)
    : "a"(0x80000000)
    );
    WDT_INFO("Is CPU support Brand String? %s\n", eax >= 0x80000004? "yes":"no");

    /**
    * If brand string is supported, eax will increase by 1 from 0x80000002 to 0x80000004, and the CPUID instruction will return:
    * EAX : Processor Brand String
    * EBX : Processor Brand String Continued
    * ECX : Processor Brand String Continued
    * EDX : Processor Brand String Continued
    */
    if(eax >= 0x80000004) 
    {
        for (i = 0x80000002; i <= 0x80000004; i++)
        {
            asm volatile ("cpuid"
            : "=a"(regs.eax), "=b"(regs.ebx), "=c"(regs.ecx), "=d"(regs.edx)
            : "0" (i)
            );
            name_as_ints[j * 4 + 0] = regs.eax;
            name_as_ints[j * 4 + 1] = regs.ebx;
            name_as_ints[j * 4 + 2] = regs.ecx;
            name_as_ints[j * 4 + 3] = regs.edx;
            j++;
        }
        temp_processor_name[49] = '\0';
        printk("%s \n", temp_processor_name);
    }
    if(strstr(temp_processor_name, "C3508"))
        g_cpu_type = CPU_TYPE_C3508;
    else if(strstr(temp_processor_name, "1627"))
        g_cpu_type = CPU_TYPE_D1627;
    else
        g_cpu_type = CPU_TYPE_DEFAULT;
    WDT_INFO("%d \n", g_cpu_type);
    return g_cpu_type;

}
EXPORT_SYMBOL(get_cpu_name);

unsigned char lpc_read_ec(unsigned short offset)
{
    return inb(EC_LPC_IO_BASE_ADDR+offset);
}

void lpc_write_ec(unsigned int offset, unsigned char val)
{
    outb(val, EC_LPC_IO_BASE_ADDR+offset);
}

ssize_t drv_get_identify(char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "COMe EC WDT\n");
}

ssize_t drv_get_state(char *buf)
{
    unsigned char val;
    int cpu_type = 0;
    cpu_type = get_cpu_name();

    if(cpu_type == CPU_TYPE_D1627)
    {
        val = lpc_read_ec(EC_WDT_CFG_REG_OFFSET);
        if((val & BIT0))
            return sprintf_s(buf, PAGE_SIZE, "active\n");
        else
            return sprintf_s(buf, PAGE_SIZE, "inactive\n");
    }else //C3508
    {
        val = lpc_read_ec(C3508_EC_WDT_CFG_REG_OFFSET);
        if((val & BIT0) && (val & BIT2))
            return sprintf_s(buf, PAGE_SIZE, "active\n");
        else
            return sprintf_s(buf, PAGE_SIZE, "inactive\n");
    }
}

ssize_t drv_get_timeleft(char *buf)
{
    unsigned short timeout;
    int cpu_type = 0;
    cpu_type = get_cpu_name();

    if(cpu_type == CPU_TYPE_D1627)
    {
        timeout = (lpc_read_ec(EC_WDT_TIMER_MSB_REG_OFFSET) << 8) | lpc_read_ec(EC_WDT_TIMER_LSB_REG_OFFSET);

        return sprintf_s(buf, PAGE_SIZE, "%d\n", timeout);
    }else //C3508
    {
        timeout = lpc_read_ec(C3508_EC_WDT_TIMER_MIN_REG_OFFSET) * 60 + lpc_read_ec(C3508_EC_WDT_TIMER_SEC_REG_OFFSET);

        return sprintf_s(buf, PAGE_SIZE, "%d\n", timeout);
    }
}

ssize_t drv_get_timeout(char *buf)
{
    unsigned short timeout;
    int cpu_type = 0;
    cpu_type = get_cpu_name();

    if(cpu_type == CPU_TYPE_D1627)
    {
        timeout = (lpc_read_ec(EC_WDT_TIMER_MSB_RESET_REG_OFFSET) << 8) | lpc_read_ec(EC_WDT_TIMER_LSB_RESET_REG_OFFSET);

        return sprintf_s(buf, PAGE_SIZE, "%d\n", timeout);
    }else //C3508
    {
        timeout = c3508_timeout;

        return sprintf_s(buf, PAGE_SIZE, "%d\n", timeout);
    }
}

void drv_set_timeout(unsigned short timeout)
{
    int cpu_type = 0;
    cpu_type = get_cpu_name();

    if(cpu_type == CPU_TYPE_D1627)
    {
        lpc_write_ec(EC_WDT_TIMER_LSB_RESET_REG_OFFSET, ((timeout >> 0) & 0xff));
        lpc_write_ec(EC_WDT_TIMER_MSB_RESET_REG_OFFSET, ((timeout >> 8) & 0xff));

        lpc_write_ec(EC_WDT_TIMER_LSB_REG_OFFSET, ((timeout >> 0) & 0xff));
        lpc_write_ec(EC_WDT_TIMER_MSB_REG_OFFSET, ((timeout >> 8) & 0xff));
    }else //C3508
    {
        c3508_timeout = timeout;
        lpc_write_ec(C3508_EC_WDT_TIMER_SEC_REG_OFFSET, ((timeout % 60)& 0xff));
        lpc_write_ec(C3508_EC_WDT_TIMER_MIN_REG_OFFSET, ((timeout / 60) & 0xff));
    }
    return;
}

void drv_set_timeleft(unsigned short timeleft)
{
    int cpu_type = 0;
    cpu_type = get_cpu_name();

    if(cpu_type == CPU_TYPE_D1627)
    {
        ;
    }else //C3508
    {
        lpc_write_ec(C3508_EC_WDT_TIMER_SEC_REG_OFFSET, ((timeleft % 60)& 0xff));
        lpc_write_ec(C3508_EC_WDT_TIMER_MIN_REG_OFFSET, ((timeleft / 60) & 0xff));
    }
    return;
}

void drv_set_reset(unsigned int reset)
{
    unsigned char val;
    int cpu_type = 0;
    cpu_type = get_cpu_name();

    if(cpu_type == CPU_TYPE_D1627)
    {
        if(reset)
        {
            //Config gpio52 to gpio
            val = inb(GPIO_IO_BASE + GPIO_USE_SEL2_BYTE2);
            val |= BIT4;
            outb(val, GPIO_IO_BASE + GPIO_USE_SEL2_BYTE2);

            //Set gpio52 to output
            val = inb(GPIO_IO_BASE + GP_IO_SEL2_BYTE2 );
            val &= ~BIT4;
            outb(val, GPIO_IO_BASE + GP_IO_SEL2_BYTE2 );

            //Set gpio52 to low
            val = inb(GPIO_IO_BASE + GP_LVL2_BYTE2 );
            val &= ~BIT4;
            outb(val, GPIO_IO_BASE + GP_LVL2_BYTE2 );

            msleep(50);

            //Set gpio52 to high
            val = inb(GPIO_IO_BASE + GP_LVL2_BYTE2 );
            val |= BIT4;
            outb(val, GPIO_IO_BASE + GP_LVL2_BYTE2 );
        }
    }else //C3508
    {
        if(reset)
        {
            drv_set_timeleft(c3508_timeout);
        }
    }

    return;
}

ssize_t drv_get_enable(char *buf)
{
    unsigned char val;
    int cpu_type = 0;
    cpu_type = get_cpu_name();

    if(cpu_type == CPU_TYPE_D1627)
    {
        val = lpc_read_ec(EC_WDT_CFG_REG_OFFSET);

        return sprintf_s(buf, PAGE_SIZE, "%d\n", (val & WDT_CFG_WDT_ENABLE));
    }else //C3508
    {
        val = lpc_read_ec(C3508_EC_WDT_CFG_REG_OFFSET);

        return sprintf_s(buf, PAGE_SIZE, "%d\n", (val & C3508_WDT_CFG_WDT_ENABLE));
    }
}

bool drv_set_enable(unsigned int enable)
{
    unsigned int ec_wdt_timeout_count = 2000;
    int cpu_type = 0;
    cpu_type = get_cpu_name();

    if(cpu_type == CPU_TYPE_D1627)
    {
        if(!enable)
        {
            lpc_write_ec(EC_WDT_CFG_REG_OFFSET, 0x00);
        }
        else if(enable == 1)
        {
            drv_set_reset(1);
            lpc_write_ec(EC_WDT_CFG_REG_OFFSET, WDT_CFG_WDT_CLEAR);

            while((lpc_read_ec(EC_WDT_CFG_REG_OFFSET) & (WDT_CFG_WDT_TRIGGER_REPORT | WDT_CFG_WDT_CLEAR)) && (ec_wdt_timeout_count))
            {
                ec_wdt_timeout_count--;
                usleep_range(99, 101); // Delay 100 us
            }

            if(!ec_wdt_timeout_count)
            {
                WDT_WARNING("The ec_wdt_timeout_count time-out.\n");
                return false;
            }

            lpc_write_ec(EC_WDT_CFG_REG_OFFSET, (WDT_CFG_WDT_M1_DELAY_TRIGGER | WDT_CFG_WDT_M3_ENABLE | WDT_CFG_WDT_M1_ENABLE | WDT_CFG_WDT_ENABLE));
        }
        else
        {
            WDT_WARNING("Invalid argument: %d.\n", enable);
            return false;
        }
    }else //C3508
    {
        if(!enable)
        {
            lpc_write_ec(C3508_EC_WDT_CFG_REG_OFFSET, 0x00);
        }
        else if(enable == 1)
        {
            drv_set_reset(1);
            lpc_write_ec(C3508_EC_WDT_CFG_REG_OFFSET, 0x00);

            while((lpc_read_ec(C3508_EC_WDT_CFG_REG_OFFSET) & (C3508_WDT_CFG_WDT_TRIGGER_REPORT)) && (ec_wdt_timeout_count))
            {
                ec_wdt_timeout_count--;
                usleep_range(99, 101); // Delay 100 us
            }

            if(!ec_wdt_timeout_count)
            {
                WDT_WARNING("The ec_wdt_timeout_count time-out.\n");
                return false;
            }

            lpc_write_ec(C3508_EC_WDT_CFG_REG_OFFSET, ( C3508_WDT_CFG_WDT_M2_ENABLE | C3508_WDT_CFG_WDT_ENABLE));
        }
        else
        {
            WDT_WARNING("Invalid argument: %d.\n", enable);
            return false;
        }
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
    int cpu_type = 0;
    cpu_type = get_cpu_name();

    if(cpu_type == CPU_TYPE_D1627)
    {
        return sprintf_s(buf, PAGE_SIZE,
            "D1627 COMe EC I/O base address: 0x6300\n"
            "Relative registers offset:\n"
            "WDT_TIMER_MSB          0x06\n"
            "WDT_TIMER_LSB          0x07\n"
            "WDT_CFG                0x08\n"
            "WDT_TIMER_MSB_RESET    0x0c\n"
            "WDT_TIMER_LSB_RESET    0x0d\n"
            "\n"
            "Use the following command to debug:\n"
            "inb <base_addr + reg_offset> --hex\n"
            "outb <base_addr + reg_offset> <value>\n");
    }else //C3508
    {
        return sprintf_s(buf, PAGE_SIZE,
            "C3508 COMe EC I/O base address: 0x6300\n"
            "Relative registers offset:\n"
            "WDT_TIMER_MIN          0x07\n"
            "WDT_TIMER_SEC          0x08\n"
            "WDT_CFG                0x06\n"
            "\n"
            "Use the following command to debug:\n"
            "inb <base_addr + reg_offset> --hex\n"
            "outb <base_addr + reg_offset> <value>\n");
    }
}

ssize_t drv_debug(const char *buf, int count)
{
	return 0;
}

static struct wdt_drivers_t pfunc = {
    .get_identify = drv_get_identify,
    .get_state = drv_get_state,
    .get_timeleft = drv_get_timeleft,
    .get_timeout = drv_get_timeout,
    .set_timeout = drv_set_timeout,
    .set_reset = drv_set_reset,
    .get_enable = drv_get_enable,
    .set_enable = drv_set_enable,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
    .debug_help = drv_debug_help,
};

static int drv_wdt_probe(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    s3ip_wdt_drivers_register(&pfunc);
    mxsonic_wdt_drivers_register(&pfunc);
#else
    kssonic_wdt_drivers_register(&pfunc);
#endif

    return 0;
}

static int drv_wdt_remove(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    s3ip_wdt_drivers_unregister();
    mxsonic_wdt_drivers_unregister();
#else
    kssonic_wdt_drivers_unregister();
#endif

    return 0;
}

static struct platform_driver drv_wdt_driver = {
    .driver = {
        .name   = DRVNAME,
        .owner = THIS_MODULE,
    },
    .probe      = drv_wdt_probe,
    .remove     = drv_wdt_remove,
};

static int __init drv_wdt_init(void)
{
    int err;
    int retval;

    drv_wdt_device = platform_device_alloc(DRVNAME, 0);
    if(!drv_wdt_device)
    {
        WDT_DEBUG("%s(#%d): platform_device_alloc fail\n", __func__, __LINE__);
        return -ENOMEM;
    }

    retval = platform_device_add(drv_wdt_device);
    if(retval)
    {
        WDT_DEBUG("%s(#%d): platform_device_add failed\n", __func__, __LINE__);
        err = -retval;
        goto dev_add_failed;
    }

    retval = platform_driver_register(&drv_wdt_driver);
    if(retval)
    {
        WDT_DEBUG("%s(#%d): platform_driver_register failed(%d)\n", __func__, __LINE__, err);
        err = -retval;
        goto dev_reg_failed;
    }

    return 0;

dev_reg_failed:
    platform_device_unregister(drv_wdt_device);
    return err;
dev_add_failed:
    platform_device_put(drv_wdt_device);
    return err;
}

static void __exit drv_wdt_exit(void)
{
    platform_driver_unregister(&drv_wdt_driver);
    platform_device_unregister(drv_wdt_device);

    return;
}

MODULE_DESCRIPTION("Huarong Watchdog Driver");
MODULE_VERSION(SWITCH_WDT_DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(drv_wdt_init);
module_exit(drv_wdt_exit);
