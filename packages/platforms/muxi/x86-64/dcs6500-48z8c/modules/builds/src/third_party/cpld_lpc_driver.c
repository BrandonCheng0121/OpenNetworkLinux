#include <linux/module.h> /*
                            => module_init()
                            => module_exit()
                            => MODULE_LICENSE()
                            => MODULE_VERSION()
                            => MODULE_AUTHOR()
                            => struct module
                          */
#include <linux/init.h>  /*
                            => typedef int (*initcall_t)(void);
                                 Note: the 'initcall_t' function returns 0 when succeed.
                                       In the Linux kernel, error codes are negative numbers
                                       belonging to the set defined in <linux/errno.h>.
                            => typedef void (*exitcall_t)(void);
                            => __init
                            => __exit
                         */
#include <linux/moduleparam.h>  /*
                                  => moduleparam()
                                */
#include <linux/types.h>  /*
                             => dev_t  (u32)
                          */
#include <linux/kdev_t.h>  /*
                              => MAJOR()
                              => MINOR()
                           */
#include <linux/string.h>  /*
                              => void *memset()
                           */
#include <linux/slab.h>  /*
                            => void kfree()
                          */
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>/*
								=> struct spinlock
                           */
#include <linux/io.h>

#include "cpld_lpc_driver.h"

#if 1
#define CPLD_LPC_DEBUG(...)
#else
#define CPLD_LPC_DEBUG(...) printk(KERN_ALERT __VA_ARGS__)
#endif

#define ERROR_DEBUG(...) printk(KERN_ALERT __VA_ARGS__)

#define DRVNAME "cpld_lpc_driver"

#define CPLD_LPC_DRIVER_VERSION "0.0.0.1"

#define LCP_IO_SIZE 1<<12

static void __iomem *lpc_base0 = NULL;
static void __iomem *lpc_base1 = NULL;
struct mutex     update_lock;


int lpc_read_cpld(u32 lpc_device, u8 reg)
{
    void __iomem *lpc_base;
	u8 val = 0x00;

    if(lpc_device == LCP_DEVICE_ADDRESS1)
    {
        lpc_base = lpc_base0;
    }
    else if(lpc_device == LCP_DEVICE_ADDRESS2)
    {
        lpc_base = lpc_base1;
    }
    else
    {
        return -1;
    }

    mutex_lock(&update_lock);
    val = readb(lpc_base + reg);
    mutex_unlock(&update_lock);
    CPLD_LPC_DEBUG( "[%s] base: 0x%x, reg:0x%x, val:0x%x\n", __func__, lpc_base, reg, val);

    return (int)val;
}
EXPORT_SYMBOL(lpc_read_cpld);

int lpc_write_cpld(u32 lpc_device, u8 reg, u8 value)
{
    void __iomem *lpc_base;

    if(lpc_device == LCP_DEVICE_ADDRESS1)
    {
        lpc_base = lpc_base0;
    }
    else if(lpc_device == LCP_DEVICE_ADDRESS2)
    {
        lpc_base = lpc_base1;
    }
    else
    {
        return -1;
    }

    CPLD_LPC_DEBUG( "[%s] base: 0x%x, reg:0x%x, val:0x%x\n", __func__, lpc_base, reg, value);
    mutex_lock(&update_lock);
    writeb(value, lpc_base + reg);
    mutex_unlock(&update_lock);

    return 0;
}
EXPORT_SYMBOL(lpc_write_cpld);


static int __init cpld_lpc_init(void)
{
    int retval = 0;

    if (!request_mem_region(LCP_DEVICE_ADDRESS1, LCP_IO_SIZE, "cpld-lpc")) {
        printk(KERN_ERR "Cannot allocate cpld-lpc memory region\n");
        return -ENODEV;
    }
    if (!request_mem_region(LCP_DEVICE_ADDRESS2, LCP_IO_SIZE, "cpld-lpc")) {
        printk(KERN_ERR "Cannot allocate cpld-lpc memory region\n");
        return -ENODEV;
    }

    if ((lpc_base0 = ioremap(LCP_DEVICE_ADDRESS1, LCP_IO_SIZE)) == NULL) {
        printk(KERN_ERR "Re-CPLD LPC1 address mapping failed\n");
        retval = -1;
        goto err_ioremap_lable;
    }
    if ((lpc_base1 = ioremap(LCP_DEVICE_ADDRESS2, LCP_IO_SIZE)) == NULL) {
        printk(KERN_ERR "Re-CPLD LPC1 address mapping failed\n");
        retval = -1;
        goto err_ioremap_lable;
    }
    mutex_init(&update_lock);

err_ioremap_lable:
    release_mem_region(LCP_DEVICE_ADDRESS1, LCP_IO_SIZE);
    release_mem_region(LCP_DEVICE_ADDRESS2, LCP_IO_SIZE);

    return (retval);
}

static void __exit cpld_lpc_exit(void)
{

    if(lpc_base0 != NULL)
        iounmap(lpc_base0);
    release_mem_region(LCP_DEVICE_ADDRESS1, LCP_IO_SIZE);
    if(lpc_base1 != NULL)
        iounmap(lpc_base1);
    release_mem_region(LCP_DEVICE_ADDRESS2, LCP_IO_SIZE);
    mutex_destroy(&update_lock);

    printk(KERN_INFO "CPLD_driver_lpc module removed\n");
    return;
}


MODULE_DESCRIPTION("Huarong CPLD LPC driver");
MODULE_VERSION(CPLD_LPC_DRIVER_VERSION);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hui Wu, Huan Ji");

module_init(cpld_lpc_init);
module_exit(cpld_lpc_exit);

