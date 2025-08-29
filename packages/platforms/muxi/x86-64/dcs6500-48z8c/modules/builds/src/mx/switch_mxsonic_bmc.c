#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>

#include "pmbus.h"
#include "switch_bmc_driver.h"

#define SWITCH_S3IP_BMC_VERSION "0.0.0.1"

unsigned int loglevel = 0;
struct bmc_drivers_t *cb_func = NULL;

struct bmc_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct bmc_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct bmc_attribute *attr, const char *buf, size_t count);
};

/* For mxsonic */
extern struct kobject *mx_switch_kobj;
static struct kobject *bmc_kobj;

enum bmc_attrs {
    LOGLEVEL,
    STATUS,
    ENABLE,
    NUM_BMC_ATTR,
};

static ssize_t mxsonic_get_loglevel(struct kobject *kobj, struct bmc_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", loglevel);
}

static ssize_t mxsonic_set_loglevel(struct kobject *kobj, struct bmc_attribute *attr, const char *buf, size_t count)
{
    int retval;
    long lev;

    if(cb_func == NULL)
        return -1;

    retval = kstrtol(buf, 0, &lev);
    if(retval == 0)
    {
        BMC_DEBUG("lev:%ld \n", lev);
    }
    else
    {
        BMC_DEBUG("Error:%d, lev:%s \n", retval, buf);
        return -1;
    }

    loglevel = (unsigned int)lev;

    cb_func->set_loglevel(lev);

    return count;
}

static ssize_t mxsonic_get_status(struct kobject *kobj, struct bmc_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_status(buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_set_enable(struct kobject *kobj, struct bmc_attribute *attr, const char *buf, size_t count)
{
    int retval;
    long lev;

    if(cb_func == NULL)
        return -1;

    retval = kstrtol(buf, 0, &lev);
    if(retval == 0)
    {
        BMC_DEBUG("lev:%ld \n", lev);
    }
    else
    {
        BMC_DEBUG("Error:%d, lev:%s \n", retval, buf);
        return -1;
    }

    loglevel = (unsigned int)lev;

    cb_func->set_enable(lev);

    return count;
}

static ssize_t mxsonic_get_enable(struct kobject *kobj, struct bmc_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_enable(buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static struct bmc_attribute bmc_attr[NUM_BMC_ATTR] = {
    [LOGLEVEL]              = {{.name = "loglevel",            .mode = S_IRUGO | S_IWUSR},     mxsonic_get_loglevel,        mxsonic_set_loglevel},
    [STATUS]                = {{.name = "status",              .mode = S_IRUGO},               mxsonic_get_status,          NULL},
    [ENABLE]                = {{.name = "enable",              .mode = S_IRUGO | S_IWUSR},     mxsonic_get_enable,          mxsonic_set_enable},
};

void mxsonic_bmc_drivers_register(struct bmc_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_bmc_drivers_register);

void mxsonic_bmc_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_bmc_drivers_unregister);

static int __init switch_bmc_init(void)
{
    int err;
    int retval;
    int i;

    /* For mxsonic */
    bmc_kobj = kobject_create_and_add("bmc", mx_switch_kobj);
    if(!bmc_kobj)
    {
        BMC_DEBUG( "Failed to create 'bmc'\n");
        return -ENOMEM;
    }

    for(i = 0; i < NUM_BMC_ATTR ; i++)
    {
        BMC_DEBUG( "sysfs_create_file /bmc/%s\n", bmc_attr[i].attr.name);
        retval = sysfs_create_file(bmc_kobj, &bmc_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", bmc_attr[i].attr.name);
            err = -retval;
            goto sysfs_create_mxsonic_bmc_attr_failed;
        }
    }

    return 0;

sysfs_create_mxsonic_bmc_attr_failed:
    for(i = 0; i < NUM_BMC_ATTR ; i++)
        sysfs_remove_file(bmc_kobj, &bmc_attr[i].attr);

    if(bmc_kobj)
    {
        kobject_put(bmc_kobj);
        bmc_kobj = NULL;
    }

    return err;
}

static void __exit switch_bmc_exit(void)
{
    int i;

    /* For mxsonic */
    for(i = 0; i < NUM_BMC_ATTR ; i++)
        sysfs_remove_file(bmc_kobj, &bmc_attr[i].attr);

    if(bmc_kobj)
    {
        kobject_put(bmc_kobj);
        bmc_kobj = NULL;
    }

    cb_func = NULL;

    return;
}

MODULE_DESCRIPTION("S3IP Switch S3IP BMC Driver");
MODULE_VERSION(SWITCH_S3IP_BMC_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_bmc_init);
module_exit(switch_bmc_exit);
