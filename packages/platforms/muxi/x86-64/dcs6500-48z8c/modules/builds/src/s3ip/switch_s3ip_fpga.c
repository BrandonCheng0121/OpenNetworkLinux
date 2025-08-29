#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "switch.h"
#include "switch_fpga_driver.h"

#define SWITCH_S3IP_FPGA_VERSION "0.0.0.1"

unsigned int loglevel;

struct fpga_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct fpga_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct fpga_attribute *attr, const char *buf, size_t count);
};

struct fpga_drivers_t *cb_func = NULL;

/* For S3IP */
extern struct kobject *switch_kobj;
static struct kobject *fpga_kobj;
static struct kobject *fpga_index_kobj[FPGA_TOTAL_NUM];

enum fpga_attrs {
    NUM,
    ALIAS,
    TYPE,
    REG_TEST,
    HW_VERSION,
    BOARD_VERSION,
	NUM_FPGA_ATTR,
};

unsigned int get_fpga_index(struct kobject *kobj)
{
    int retval;
    unsigned int fpga_index;
    char fpga_index_str[2] = {0};

    memcpy_s(fpga_index_str, 2, (kobject_name(kobj)+4), 1);
    retval = kstrtoint(fpga_index_str, 10, &fpga_index);
    if(retval == 0)
    {
        FPGA_DEBUG("[%s] fpga_index:%d \n", __func__, fpga_index);
    }
    else
    {
        FPGA_DEBUG("[%s] Error:%d, fpga_index:%s \n", __func__, retval, fpga_index_str);
        return -1;
    }

    return (fpga_index-FPGA_INDEX_START);
}

static ssize_t s3ip_debug_help(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        ERROR_RETURN_NA(-1);

    return cb_func->debug_help(buf);
}

static ssize_t s3ip_get_loglevel(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    long lev;

    if(cb_func == NULL)
        ERROR_RETURN_NA(-1);

    cb_func->get_loglevel(&lev);

    return sprintf_s(buf, PAGE_SIZE, "%ld\n", lev);
}

static ssize_t s3ip_set_loglevel(struct kobject *kobj, struct fpga_attribute *attr, const char *buf, size_t count)
{
    int retval;
    long lev;

    if(cb_func == NULL)
        return -1;

    retval = kstrtol(buf, 10, &lev);
    if(retval == 0)
    {
        FPGA_DEBUG("[%s] lev:%ld \n", __func__, lev);
    }
    else
    {
        FPGA_DEBUG("[%s] Error:%d, lev:%s \n", __func__, retval, buf);
        return -1;
    }

    cb_func->set_loglevel(lev);

    return count;
}

static ssize_t s3ip_get_reboot_cause(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    return 0;
}

static ssize_t s3ip_get_num(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
	return sprintf_s(buf, PAGE_SIZE, "%d\n", FPGA_TOTAL_NUM);
}

static ssize_t s3ip_get_alias(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    unsigned int fpga_index;

    if(cb_func == NULL)
        ERROR_RETURN_NA(-1);

    fpga_index = get_fpga_index(kobj);
    if(fpga_index < 0)
    {
        FPGA_DEBUG("[%s] Get fpga index:%d failed.\n", __func__, fpga_index);
        ERROR_RETURN_NA(-1);
    }

    return cb_func->get_alias(fpga_index, buf);
}

static ssize_t s3ip_get_type(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    unsigned int fpga_index;

    if(cb_func == NULL)
        ERROR_RETURN_NA(-1);

    fpga_index = get_fpga_index(kobj);
    if(fpga_index < 0)
    {
        FPGA_DEBUG("[%s] Get fpga index:%d failed.\n", __func__, fpga_index);
        ERROR_RETURN_NA(-1);
    }

    return cb_func->get_type(fpga_index, buf);
}

static ssize_t s3ip_get_reg_test(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    unsigned int fpga_index;

    if(cb_func == NULL)
        ERROR_RETURN_NA(-1);

    fpga_index = get_fpga_index(kobj);
    if(fpga_index < 0)
    {
        FPGA_DEBUG("[%s] Get fpga index:%d failed.\n", __func__, fpga_index);
        ERROR_RETURN_NA(-1);
    }

    return cb_func->get_fmea_selftest_status(fpga_index, buf);
}

static ssize_t s3ip_get_hw_version(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    unsigned int fpga_index;

    if(cb_func == NULL)
        ERROR_RETURN_NA(-1);

    fpga_index = get_fpga_index(kobj);
    if(fpga_index < 0)
    {
        FPGA_DEBUG("[%s] Get fpga index:%d failed.\n", __func__, fpga_index);
        ERROR_RETURN_NA(-1);
    }

    return cb_func->get_hw_version(fpga_index, buf);
}

static ssize_t s3ip_get_board_version(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    unsigned int fpga_index;

    if(cb_func == NULL)
        ERROR_RETURN_NA(-1);

    fpga_index = get_fpga_index(kobj);
    if(fpga_index < 0)
    {
        FPGA_DEBUG("[%s] Get fpga index failed:%d.\n", __func__, fpga_index);
        ERROR_RETURN_NA(-1);
    }

    return cb_func->get_board_version(fpga_index, buf);
}

static struct fpga_attribute fpga_attr[NUM_FPGA_ATTR] = {
    [NUM]                   = {{.name = "number",               .mode = S_IRUGO},               s3ip_get_num,                NULL},
    [ALIAS]                 = {{.name = "alias",                .mode = S_IRUGO},               s3ip_get_alias,              NULL},
    [TYPE]                  = {{.name = "type",                 .mode = S_IRUGO},               s3ip_get_type,               NULL},
    [REG_TEST]              = {{.name = "reg_test",             .mode = S_IRUGO | S_IWUSR},     s3ip_get_reg_test,           NULL},
    [HW_VERSION]            = {{.name = "firmware_version",     .mode = S_IRUGO},               s3ip_get_hw_version,         NULL},
    [BOARD_VERSION]         = {{.name = "board_version",        .mode = S_IRUGO},               s3ip_get_board_version,      NULL},
};

void s3ip_fpga_drivers_register(struct fpga_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(s3ip_fpga_drivers_register);

void s3ip_fpga_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(s3ip_fpga_drivers_unregister);

static int __init switch_fpga_init(void)
{
    int err;
    int retval;
    int i;
    int fpga_index;
    char *fpga_index_str;

    fpga_index_str = (char *)kzalloc(6*sizeof(char), GFP_KERNEL);
    if (!fpga_index_str)
    {
        FPGA_DEBUG( "[%s] Fail to alloc fpga_index_str memory\n", __func__);
        return -ENOMEM;
    }

    /* For S3IP */
    fpga_kobj = kobject_create_and_add("fpga", switch_kobj);
    if(!fpga_kobj)
    {
        FPGA_DEBUG( "[%s]Failed to create 'fpga'\n", __func__);
        err = -ENOMEM;
        goto sysfs_create_kobject_fpga_failed;
    }

    for(i=0; i <= NUM; i++)
    {
        FPGA_DEBUG( "[%s]sysfs_create_file /fpga/%s\n", __func__, fpga_attr[i].attr.name);
        retval = sysfs_create_file(fpga_kobj, &fpga_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", fpga_attr[i].attr.name);
            err = -retval;
            goto sysfs_create_s3ip_attr_failed;
        }
    }

    for(fpga_index=0; fpga_index<FPGA_TOTAL_NUM; fpga_index++)
    {
        if(sprintf_s(fpga_index_str, 6, "fpga%d", fpga_index+FPGA_INDEX_START) < 0)
        {
            err = -ENOMEM;
            goto sysfs_create_kobject_switch_fpga_index_failed;
        }
        fpga_index_kobj[fpga_index] = kobject_create_and_add(fpga_index_str, fpga_kobj);
        if(!fpga_index_kobj[fpga_index])
        {
            FPGA_DEBUG( "[%s]Failed to create 'fpga%d'\n", __func__, fpga_index+FPGA_INDEX_START);
            err = -ENOMEM;
            goto sysfs_create_kobject_switch_fpga_index_failed;
        }

        for(i=ALIAS; i < NUM_FPGA_ATTR; i++)
        {
            FPGA_DEBUG( "[%s]sysfs_create_file /fpga/fpga%d/%s\n", __func__, fpga_index+FPGA_INDEX_START, fpga_attr[i].attr.name);
            retval = sysfs_create_file(fpga_index_kobj[fpga_index], &fpga_attr[i].attr);
            if(retval)
            {
                printk(KERN_ERR "Failed to create file '%s'\n", fpga_attr[i].attr.name);
                err = -retval;
                goto sysfs_create_s3ip_fpga_attr_failed;
            }
        }
    }

    kfree(fpga_index_str);

    return 0;

sysfs_create_s3ip_fpga_attr_failed:
sysfs_create_kobject_switch_fpga_index_failed:
    for(i=0; i <= NUM; i++)
        sysfs_remove_file(fpga_kobj, &fpga_attr[i].attr);

    for(fpga_index=0; fpga_index<FPGA_TOTAL_NUM; fpga_index++)
    {
        if(fpga_index_kobj[fpga_index])
        {
            for(i=ALIAS; i < NUM_FPGA_ATTR; i++)
                sysfs_remove_file(fpga_index_kobj[fpga_index], &fpga_attr[i].attr);

            kobject_put(fpga_index_kobj[fpga_index]);
            fpga_index_kobj[fpga_index] = NULL;
        }
    }

sysfs_create_s3ip_attr_failed:
    if(fpga_kobj)
    {
        kobject_put(fpga_kobj);
        fpga_kobj = NULL;
    }

sysfs_create_kobject_fpga_failed:
    kfree(fpga_index_str);

    return err;
}

static void __exit switch_fpga_exit(void)
{
    int i;
    int fpga_index;

    /* For S3IP */
    for(i=0; i <= NUM; i++)
        sysfs_remove_file(fpga_kobj, &fpga_attr[i].attr);

    for(fpga_index=0; fpga_index<FPGA_TOTAL_NUM; fpga_index++)
    {
        if(fpga_index_kobj[fpga_index])
        {
            for(i=ALIAS; i < NUM_FPGA_ATTR; i++)
                sysfs_remove_file(fpga_index_kobj[fpga_index], &fpga_attr[i].attr);

            kobject_put(fpga_index_kobj[fpga_index]);
            fpga_index_kobj[fpga_index] = NULL;
        }
    }

    if(fpga_kobj)
    {
        kobject_put(fpga_kobj);
        fpga_kobj = NULL;
    }

    cb_func = NULL;

    return;
}

MODULE_AUTHOR("Weichen Chen <weichen_chen@accton.com>");
MODULE_DESCRIPTION("Muxi Switch S3IP fpga Driver");
MODULE_VERSION(SWITCH_S3IP_FPGA_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_fpga_init);
module_exit(switch_fpga_exit);