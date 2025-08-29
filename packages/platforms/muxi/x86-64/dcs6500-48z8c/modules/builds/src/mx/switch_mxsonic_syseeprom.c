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

#include <uapi/mtd/mtd-abi.h>
#include <linux/fs.h>
#include <asm/uaccess.h>

#include "switch.h"
#include "switch_mb_driver.h"

#define ERROR_DEBUG(...) printk(KERN_ALERT __VA_ARGS__)

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    (!FALSE)
#endif

#define SWITCH_MXSONIC_SYSEEPROM_VERSION "0.0.0.1"

unsigned int loglevel = 0;

struct syseeprom_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct syseeprom_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct syseeprom_attribute *attr, const char *buf, size_t count);
};

struct mainboard_drivers_t *cb_func = NULL;

/* For MXSONiC */
extern struct kobject *mx_switch_kobj;
static struct kobject *syseeprom_kobj;

enum syseeprom_attrs {
    EEPROM,
    BSP_VERSION,
    DEBUG,
    LOGLEVEL,
    DATE,
    NAME,
    MGMT_MAC,
    SWITCH_MAC,
    VENDOR,
    LABEL_REVISION,
    SN,
    NUM_SYSEEPROM_ATTR,
};

static ssize_t mxsonic_get_eeprom(struct kobject *kobj, struct syseeprom_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->get_syseeprom(buf);
}

static ssize_t mxsonic_get_bsp_version(struct kobject *kobj, struct syseeprom_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->get_odm_bsp_version(buf);
}

static ssize_t mxsonic_debug_help(struct kobject *kobj, struct syseeprom_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->debug_help(buf);
}

static ssize_t mxsonic_debug(struct kobject *kobj, struct syseeprom_attribute *attr, const char *buf, size_t count)
{
    return count;
}

static ssize_t mxsonic_get_loglevel(struct kobject *kobj, struct syseeprom_attribute *attr, char *buf)
{
    long lev;

    if(cb_func == NULL)
        return -1;

    cb_func->get_loglevel(&lev);

    return sprintf_s(buf, PAGE_SIZE, "%ld\n", lev);
}

static ssize_t mxsonic_set_loglevel(struct kobject *kobj, struct syseeprom_attribute *attr, const char *buf, size_t count)
{
    int retval;
    long lev;

    if(cb_func == NULL)
        return -1;

    retval = kstrtol(buf, 10, &lev);
    if(retval == 0)
    {
        MAINBOARD_DEBUG("[%s] lev:%ld \n", __func__, lev);
    }
    else
    {
        MAINBOARD_DEBUG("[%s] Error:%d, lev:%s \n", __func__, retval, buf);
        return -1;
    }

    loglevel = (unsigned int)lev;

    cb_func->set_loglevel(lev);

    return count;
}

static ssize_t mxsonic_get_date(struct kobject *kobj, struct syseeprom_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->get_date(buf);
}

static ssize_t mxsonic_get_name(struct kobject *kobj, struct syseeprom_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->get_name(buf);
}

static ssize_t mxsonic_get_mgmt_mac(struct kobject *kobj, struct syseeprom_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->get_mgmt_mac(buf);
}

static ssize_t mxsonic_get_switch_mac(struct kobject *kobj, struct syseeprom_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->get_switch_mac(buf);
}

static ssize_t mxsonic_get_switch_vendor(struct kobject *kobj, struct syseeprom_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->get_vendor(buf);
}

static ssize_t mxsonic_get_switch_label_revision(struct kobject *kobj, struct syseeprom_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->get_label_revision(buf);
}

static ssize_t mxsonic_get_sn(struct kobject *kobj, struct syseeprom_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->get_sn(buf);
}

static struct syseeprom_attribute syseeprom_attr[NUM_SYSEEPROM_ATTR] = {
    [EEPROM]                    = {{.name = "eeprom",               .mode = S_IRUGO},               mxsonic_get_eeprom,         NULL},
    [BSP_VERSION]               = {{.name = "bsp_version",          .mode = S_IRUGO},               mxsonic_get_bsp_version,    NULL},
    [DEBUG]                     = {{.name = "debug",                .mode = S_IRUGO | S_IWUSR},     mxsonic_debug_help,         mxsonic_debug},
    [LOGLEVEL]                  = {{.name = "loglevel",             .mode = S_IRUGO | S_IWUSR},     mxsonic_get_loglevel,       mxsonic_set_loglevel},
    [DATE]                      = {{.name = "date",                 .mode = S_IRUGO},               mxsonic_get_date,               NULL},
    [NAME]                      = {{.name = "name",                 .mode = S_IRUGO},               mxsonic_get_name,               NULL},
    [MGMT_MAC]                  = {{.name = "mgmt_mac",             .mode = S_IRUGO},               mxsonic_get_mgmt_mac,           NULL},
    [SWITCH_MAC]                = {{.name = "switch_mac",           .mode = S_IRUGO},               mxsonic_get_switch_mac,         NULL},
    [VENDOR]                    = {{.name = "vendor",               .mode = S_IRUGO},               mxsonic_get_switch_vendor,      NULL},
    [LABEL_REVISION]            = {{.name = "label_revision",       .mode = S_IRUGO},               mxsonic_get_switch_label_revision,NULL},
    [SN]                        = {{.name = "sn",                   .mode = S_IRUGO},               mxsonic_get_sn,                 NULL},
};

void mxsonic_mainboard_drivers_register(struct mainboard_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_mainboard_drivers_register);

void mxsonic_mainboard_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_mainboard_drivers_unregister);

static int __init switch_syseeprom_init(void)
{
    int retval = 0;
    int err;
    int i;

    /* For MXSONiC */
    syseeprom_kobj = kobject_create_and_add("syseeprom", mx_switch_kobj);
    if(!syseeprom_kobj)
    {
        MAINBOARD_DEBUG( "[%s]Failed to create 'syseeprom'\n", __func__);
        return -ENOMEM;
    }

    for(i=0; i < NUM_SYSEEPROM_ATTR; i++)
    {
        MAINBOARD_DEBUG( "[%s]sysfs_create_file /syseeprom/%s\n", __func__, syseeprom_attr[i].attr.name);
        retval = sysfs_create_file(syseeprom_kobj, &syseeprom_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", syseeprom_attr[i].attr.name);
            err = -retval;
            goto sysfs_create_mxsonic_syseeprom_attr_failed;
        }
    }

    return 0;

sysfs_create_mxsonic_syseeprom_attr_failed:
    if(syseeprom_kobj)
    {
        kobject_put(syseeprom_kobj);
        syseeprom_kobj = NULL;
    }

    return err;
}

static void __exit switch_syseeprom_exit(void)
{
    int i;

    /* For MXSONiC */
    for(i=0; i < NUM_SYSEEPROM_ATTR; i++)
        sysfs_remove_file(syseeprom_kobj, &syseeprom_attr[i].attr);

    if(syseeprom_kobj)
    {
        kobject_put(syseeprom_kobj);
        syseeprom_kobj = NULL;
    }

    cb_func = NULL;

    return;
}

MODULE_DESCRIPTION("Huarong Switch MXSONiC SYSEEPROM Driver");
MODULE_VERSION(SWITCH_MXSONIC_SYSEEPROM_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_syseeprom_init);
module_exit(switch_syseeprom_exit);
