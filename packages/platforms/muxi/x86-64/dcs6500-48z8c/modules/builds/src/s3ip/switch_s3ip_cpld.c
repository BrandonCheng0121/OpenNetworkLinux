#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "switch_cpld_driver.h"

#define SWITCH_S3IP_CPLD_VERSION "0.0.0.1"

unsigned int loglevel = 0;

struct cpld_attribute
{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct cpld_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct cpld_attribute *attr, const char *buf, size_t count);
};

struct cpld_drivers_t *cb_func = NULL;

/* For S3IP */
extern struct kobject *switch_kobj;
static struct kobject *cpld_kobj;
static struct kobject *cpld_index_kobj[CPLD_TOTAL_NUM];

enum cpld_attrs
{
    DEBUG,
    LOGLEVEL,
    REBOOT_CAUSE,
    NUM,
    ALIAS,
    TYPE,
    REG_TEST,
    HW_VERSION,
    BOARD_VERSION,
    NUM_CPLD_ATTR,
};

unsigned int get_cpld_index(struct kobject *kobj)
{
    int retval;
    unsigned int cpld_index;
    char cpld_index_str[2] = {0};

    memcpy_s(cpld_index_str, 2, (kobject_name(kobj) + 4), 1);
    retval = kstrtoint(cpld_index_str, 10, &cpld_index);
    if (retval == 0)
    {
        CPLD_DEBUG("[%s] cpld_index:%d \n", __func__, cpld_index);
    }
    else
    {
        CPLD_DEBUG("[%s] Error:%d, cpld_index:%s \n", __func__, retval, cpld_index_str);
        return -1;
    }

    return (cpld_index-CPLD_INDEX_START);
}

static ssize_t s3ip_debug_help(struct kobject *kobj, struct cpld_attribute *attr, char *buf)
{
    if (cb_func == NULL)
        return -1;

    return cb_func->debug_help(buf);
}

static ssize_t s3ip_debug(struct kobject *kobj, struct cpld_attribute *attr, const char *buf, size_t count)
{
    return count;
}

static ssize_t s3ip_get_loglevel(struct kobject *kobj, struct cpld_attribute *attr, char *buf)
{
    long lev;

    if (cb_func == NULL)
        return -1;

    cb_func->get_loglevel(&lev);

    return sprintf_s(buf, PAGE_SIZE, "%ld\n", lev);
}

static ssize_t s3ip_set_loglevel(struct kobject *kobj, struct cpld_attribute *attr, const char *buf, size_t count)
{
    int retval;
    long lev;

    if (cb_func == NULL)
        return -1;

    retval = kstrtol(buf, 0, &lev);
    if (retval == 0)
    {
        CPLD_DEBUG("lev:%ld \n", lev);
    }
    else
    {
        CPLD_DEBUG("Error:%d, lev:%s \n", retval, buf);
        return -1;
    }

    loglevel = (unsigned int)lev;

    cb_func->set_loglevel(lev);

    return count;
}

static ssize_t s3ip_get_reboot_cause(struct kobject *kobj, struct cpld_attribute *attr, char *buf)
{
    int retval;

    if (cb_func == NULL)
    {
        return -1;
    }

    retval = cb_func->get_reboot_cause(buf);
    if (retval < 0)
    {
        return -1;
    }
    else
    {
        return retval;
    }
}

static ssize_t s3ip_get_num(struct kobject *kobj, struct cpld_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", CPLD_TOTAL_NUM);
}

static ssize_t s3ip_get_alias(struct kobject *kobj, struct cpld_attribute *attr, char *buf)
{
    unsigned int cpld_index;

    if (cb_func == NULL)
        return -1;

    cpld_index = get_cpld_index(kobj);
    if (cpld_index < 0)
    {
        CPLD_DEBUG("[%s] Get cpld index failed.\n", __func__);
        return -1;
    }

    return cb_func->get_alias(cpld_index, buf);
}

static ssize_t s3ip_get_type(struct kobject *kobj, struct cpld_attribute *attr, char *buf)
{
    unsigned int cpld_index;

    if (cb_func == NULL)
        return -1;

    cpld_index = get_cpld_index(kobj);
    if (cpld_index < 0)
    {
        CPLD_DEBUG("[%s] Get cpld index failed.\n", __func__);
        return -1;
    }

    return cb_func->get_type(cpld_index, buf);
}

static ssize_t s3ip_get_reg_test(struct kobject *kobj, struct cpld_attribute *attr, char *buf)
{
    unsigned int cpld_index;

    if (cb_func == NULL)
        return -1;

    cpld_index = get_cpld_index(kobj);
    if (cpld_index < 0)
    {
        CPLD_DEBUG("[%s] Get cpld index failed.\n", __func__);
        return -1;
    }

    return cb_func->get_fmea_selftest_status(cpld_index, buf);
}

static ssize_t s3ip_get_hw_version(struct kobject *kobj, struct cpld_attribute *attr, char *buf)
{
    unsigned int cpld_index;

    if (cb_func == NULL)
        return -1;

    cpld_index = get_cpld_index(kobj);
    if (cpld_index < 0)
    {
        CPLD_DEBUG("[%s] Get cpld index failed.\n", __func__);
        return -1;
    }

    return cb_func->get_hw_version(cpld_index, buf);
}

static ssize_t s3ip_get_board_version(struct kobject *kobj, struct cpld_attribute *attr, char *buf)
{
    unsigned int cpld_index;

    if (cb_func == NULL)
        return -1;

    cpld_index = get_cpld_index(kobj);
    if (cpld_index < 0)
    {
        CPLD_DEBUG("[%s] Get cpld index failed.\n", __func__);
        return -1;
    }

    return cb_func->get_board_version(cpld_index, buf);
}

static struct cpld_attribute cpld_attr[NUM_CPLD_ATTR] = {
    [DEBUG]                 = {{.name = "debug",                .mode = S_IRUGO | S_IWUSR},     s3ip_debug_help,             s3ip_debug},
    [LOGLEVEL]              = {{.name = "loglevel",             .mode = S_IRUGO | S_IWUSR},     s3ip_get_loglevel,           s3ip_set_loglevel},
    [REBOOT_CAUSE]          = {{.name = "reboot_cause",         .mode = S_IRUGO},               s3ip_get_reboot_cause,       NULL},
    [NUM]                   = {{.name = "number",               .mode = S_IRUGO},               s3ip_get_num,                NULL},
    [ALIAS]                 = {{.name = "alias",                .mode = S_IRUGO},               s3ip_get_alias,              NULL},
    [TYPE]                  = {{.name = "type",                 .mode = S_IRUGO},               s3ip_get_type,               NULL},
    [REG_TEST]              = {{.name = "reg_test",             .mode = S_IRUGO | S_IWUSR},     s3ip_get_reg_test,           NULL},
    [HW_VERSION]            = {{.name = "firmware_version",     .mode = S_IRUGO},               s3ip_get_hw_version,         NULL},
    [BOARD_VERSION]         = {{.name = "board_version",        .mode = S_IRUGO},               s3ip_get_board_version,      NULL},
};

void s3ip_cpld_drivers_register(struct cpld_drivers_t *pfunc)
{    
    cb_func = pfunc;
    
    return;
}
EXPORT_SYMBOL_GPL(s3ip_cpld_drivers_register);

void s3ip_cpld_drivers_unregister(void)
{
    cb_func = NULL;
    
    return;
}
EXPORT_SYMBOL_GPL(s3ip_cpld_drivers_unregister);

static int __init switch_cpld_init(void)
{
    int err;
    int retval;
    int i;
    int cpld_index;
    char *cpld_index_str;
    
    cpld_index_str = (char *)kzalloc(6*sizeof(char), GFP_KERNEL);
    if (!cpld_index_str)
    {
        CPLD_DEBUG( "[%s] Fail to alloc cpld_index_str memory\n", __func__);
        return -ENOMEM;
    }
    
    /* For S3IP */
    cpld_kobj = kobject_create_and_add("cpld", switch_kobj);
    if(!cpld_kobj)
    {
        CPLD_DEBUG( "[%s]Failed to create 'cpld'\n", __func__);
        err = -ENOMEM;
        goto sysfs_create_kobject_cpld_failed; 
    }
    
    for(i=0; i <= NUM; i++)
    {
        CPLD_DEBUG( "[%s]sysfs_create_file /cpld/%s\n", __func__, cpld_attr[i].attr.name);
        retval = sysfs_create_file(cpld_kobj, &cpld_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", cpld_attr[i].attr.name);
            err = -retval;
            goto sysfs_create_s3ip_attr_failed;
        }
    }
    for(cpld_index=0; cpld_index<CPLD_TOTAL_NUM; cpld_index++)
    {
        if(sprintf_s(cpld_index_str, 6, "cpld%d", cpld_index+CPLD_INDEX_START) < 0)
        {
            err = -ENOMEM;
            goto sysfs_create_kobject_switch_cpld_index_failed;
        }
        cpld_index_kobj[cpld_index] = kobject_create_and_add(cpld_index_str, cpld_kobj);
        if(!cpld_index_kobj[cpld_index])
        {
            CPLD_DEBUG( "[%s]Failed to create 'cpld%d'\n", __func__, cpld_index+CPLD_INDEX_START);
            err = -ENOMEM;
            goto sysfs_create_kobject_switch_cpld_index_failed;
        }
        
        for(i=ALIAS; i < NUM_CPLD_ATTR; i++)
        {
            CPLD_DEBUG( "[%s]sysfs_create_file /cpld/cpld%d/%s\n", __func__, cpld_index, cpld_attr[i].attr.name);
            retval = sysfs_create_file(cpld_index_kobj[cpld_index], &cpld_attr[i].attr);
            if(retval)
            {
                printk(KERN_ERR "Failed to create file '%s'\n", cpld_attr[i].attr.name);
                err = -retval;
                goto sysfs_create_s3ip_cpld_attr_failed;
            }
        }
    }
    
    kfree(cpld_index_str);
    
    return 0;
    
sysfs_create_s3ip_cpld_attr_failed:
sysfs_create_kobject_switch_cpld_index_failed:
    for(i=0; i <= NUM; i++)
        sysfs_remove_file(cpld_kobj, &cpld_attr[i].attr);
    
    for(cpld_index=0; cpld_index<CPLD_TOTAL_NUM; cpld_index++)
    {
        if(cpld_index_kobj[cpld_index])
        {
            for(i=ALIAS; i < NUM_CPLD_ATTR; i++)
                sysfs_remove_file(cpld_index_kobj[cpld_index], &cpld_attr[i].attr);
    
            kobject_put(cpld_index_kobj[cpld_index]);
            cpld_index_kobj[cpld_index] = NULL;
        }
    }
	
sysfs_create_s3ip_attr_failed:
    if(cpld_kobj)
    {
        kobject_put(cpld_kobj);
        cpld_kobj = NULL;
    }
    
sysfs_create_kobject_cpld_failed:
    kfree(cpld_index_str);
    
    return err;
}

static void __exit switch_cpld_exit(void)
{
    int i;
    int cpld_index;
    
    /* For S3IP */
    for(i=0; i <= NUM; i++)
        sysfs_remove_file(cpld_kobj, &cpld_attr[i].attr);
    
    for(cpld_index=0; cpld_index<CPLD_TOTAL_NUM; cpld_index++)
    {
        if(cpld_index_kobj[cpld_index])
        {
            for(i=ALIAS; i < NUM_CPLD_ATTR; i++)
                sysfs_remove_file(cpld_index_kobj[cpld_index], &cpld_attr[i].attr);
    
            kobject_put(cpld_index_kobj[cpld_index]);
            cpld_index_kobj[cpld_index] = NULL;
        }
    }
    
    if(cpld_kobj)
    {
        kobject_put(cpld_kobj);
        cpld_kobj = NULL;
    }
    
    cb_func = NULL;
    
    return;
}

MODULE_AUTHOR("Weichen Chen <weichen_chen@accton.com>");
MODULE_DESCRIPTION("Huarong Switch S3IP CPLD Driver");
MODULE_VERSION(SWITCH_S3IP_CPLD_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_cpld_init);
module_exit(switch_cpld_exit);