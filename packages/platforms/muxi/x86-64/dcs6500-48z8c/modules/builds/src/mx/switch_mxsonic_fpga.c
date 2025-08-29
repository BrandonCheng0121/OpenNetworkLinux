#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "switch.h"
#include "switch_fpga_driver.h"

#define SWITCH_MXSONIC_FPGA_VERSION "0.0.0.1"

unsigned int loglevel;
#define FPGA0_FMEA_KOBJ_NUM 3

struct fpga_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct fpga_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct fpga_attribute *attr, const char *buf, size_t count);
};

struct fpga_drivers_t *cb_func = NULL;

/* For MXSONiC */
extern struct kobject *mx_switch_kobj;
static struct kobject *fpga_kobj;
static struct kobject *fpga_index_kobj[FPGA_TOTAL_NUM];
/* For FPGA */
static struct kobject *fpga0_fmea_kobj[FPGA0_FMEA_KOBJ_NUM];

static char *fpga0_fmea_kobj_name[FPGA0_FMEA_KOBJ_NUM] = {"selftest", "clock", "corrosion"};

enum fpga_attrs {
    DEBUG,
    LOGLEVEL,
    NUM,
    ALIAS,
    TYPE,
    HW_VERSION,
    BOARD_VERSION,
    NUM_FPGA_ATTR,
};

enum fpga_fmea_attrs {
    RESET,
    STATUS,
    NUM_FPGA_FMEA_ATTR,
};

int get_fpga_index(struct kobject *kobj)
{
    int retval;
    unsigned int fpga_index;
    char fpga_index_str[2] = {0};

    if(memcpy_s(fpga_index_str, 2, (kobject_name(kobj)+4), 1) != 0)
    {
        return -ENOMEM;
    }
    retval = kstrtoint(fpga_index_str, 10, &fpga_index);
    if(retval == 0)
    {
        FPGA_DEBUG("fpga_index:%d \n", fpga_index);
    }
    else
    {
        FPGA_DEBUG("Error:%d, fpga_index:%s \n", retval, fpga_index_str);
        return -1;
    }

    return (fpga_index-FPGA_INDEX_START);
}

int get_fpga_index_from_fmea_kobj(struct kobject *kobj)
{
    int retval;
    unsigned int fpga_index;
    char fpga_index_str[2] = {0};

    if(memcpy_s(fpga_index_str, 2, (kobject_name(kobj->parent)+4), 1) != 0)
    {
        return -ENOMEM;
    }
    retval = kstrtoint(fpga_index_str, 10, &fpga_index);
    if(retval == 0)
    {
        FPGA_DEBUG("fpga_index:%d \n", fpga_index);
    }
    else
    {
        FPGA_DEBUG("Error:%d, fpga_index:%s \n", retval, fpga_index_str);
        return -1;
    }

    return fpga_index;
}

static ssize_t mxsonic_debug_help(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->debug_help(buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_debug(struct kobject *kobj, struct fpga_attribute *attr, const char *buf, size_t count)
{
    return count;
}

static ssize_t mxsonic_get_loglevel(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", loglevel);
}

static ssize_t mxsonic_set_loglevel(struct kobject *kobj, struct fpga_attribute *attr, const char *buf, size_t count)
{
    int retval;
    long lev;

    if(cb_func == NULL)
        return -1;

    retval = kstrtol(buf, 10, &lev);
    if(retval == 0)
    {
        FPGA_DEBUG("lev:%ld \n", lev);
    }
    else
    {
        FPGA_DEBUG("Error:%d, lev:%s \n", retval, buf);
        return -1;
    }

    loglevel = (unsigned int)lev;

    cb_func->set_loglevel(lev);

    return count;
}

static ssize_t mxsonic_get_num(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", FPGA_TOTAL_NUM);
}

static ssize_t mxsonic_get_alias(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    int retval;
    unsigned int fpga_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    fpga_index = get_fpga_index(kobj);
    if(fpga_index < 0)
    {
        FPGA_DEBUG("Get fpga index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_alias(fpga_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_get_type(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    int retval;
    unsigned int fpga_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    fpga_index = get_fpga_index(kobj);
    if(fpga_index < 0)
    {
        FPGA_DEBUG("Get fpga index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_type(fpga_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_get_hw_version(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    int retval;
    unsigned int fpga_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    fpga_index = get_fpga_index(kobj);
    if(fpga_index < 0)
    {
        FPGA_DEBUG("Get fpga index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_hw_version(fpga_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_get_board_version(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    int retval;
    unsigned int fpga_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    fpga_index = get_fpga_index(kobj);
    if(fpga_index < 0)
    {
        FPGA_DEBUG("Get fpga index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->get_board_version(fpga_index, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_get_fmea_status(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    int len;
    int fpga_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }
    
    fpga_index = get_fpga_index_from_fmea_kobj(kobj);
    if(fpga_index < 0)
    {
        FPGA_DEBUG("Get fpga index failed.\n");
        ERROR_RETURN_NA(-1);
    }

    if(!strcmp(kobject_name(kobj), "selftest"))
    {
        len = cb_func->get_fmea_selftest_status(fpga_index, buf);
    }
    else if(!strcmp(kobject_name(kobj), "corrosion"))
    {
        len = cb_func->get_fmea_corrosion_status(fpga_index, buf);
    }
    else if(!strcmp(kobject_name(kobj), "voltage"))
    {
        len = cb_func->get_fmea_voltage_status(fpga_index, buf);
    }
    else if(!strcmp(kobject_name(kobj), "clock"))
    {
        len = cb_func->get_fmea_clock_status(fpga_index, buf);
    }
    else if(!strcmp(kobject_name(kobj), "battery"))
    {
        len = cb_func->get_fmea_battery_status(buf);
    }
    else;

    return len;
}

static ssize_t mxsonic_get_fmea_reset(struct kobject *kobj, struct fpga_attribute *attr, char *buf)
{
    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    return cb_func->get_fmea_reset(buf);
}

static ssize_t mxsonic_set_fmea_reset(struct kobject *kobj, struct fpga_attribute *attr, const char *buf, size_t count)
{
    int retval;
    int reset;

    if(cb_func == NULL)
    {
        return -ENXIO;
    }

    retval = kstrtoint(buf, 10, &reset);
    if(retval == 0)
    {
        FPGA_DEBUG("reset:%d \n", reset);
    }
    else
    {
        FPGA_DEBUG("Error:%d, reset:%s \n", retval, buf);
        return -ENXIO;
    }

    retval = cb_func->set_fmea_reset(reset);
    if(retval == false)
    {
        FPGA_DEBUG("Set FPGA reset failed.\n");
        return -ENXIO;
    }

    return count;
}

static struct fpga_attribute fpga_attr[NUM_FPGA_ATTR] = {
    [DEBUG]                 = {{.name = "debug",                .mode = S_IRUGO | S_IWUSR},     mxsonic_debug_help,             mxsonic_debug},
    [LOGLEVEL]              = {{.name = "loglevel",             .mode = S_IRUGO | S_IWUSR},     mxsonic_get_loglevel,           mxsonic_set_loglevel},
    [NUM]                   = {{.name = "num",                  .mode = S_IRUGO},               mxsonic_get_num,                NULL},
    [ALIAS]                 = {{.name = "alias",                .mode = S_IRUGO},               mxsonic_get_alias,              NULL},
    [TYPE]                  = {{.name = "type",                 .mode = S_IRUGO},               mxsonic_get_type,               NULL},
    [HW_VERSION]            = {{.name = "hw_version",           .mode = S_IRUGO},               mxsonic_get_hw_version,         NULL},
    [BOARD_VERSION]         = {{.name = "board_version",        .mode = S_IRUGO},               mxsonic_get_board_version,      NULL},
};

static struct fpga_attribute fpga_fmea_attr[NUM_FPGA_FMEA_ATTR] = {
    [RESET]                 = {{.name = "reset",                .mode = S_IRUGO | S_IWUSR},     mxsonic_get_fmea_reset,                     mxsonic_set_fmea_reset},
    [STATUS]                = {{.name = "status",               .mode = S_IRUGO},               mxsonic_get_fmea_status,                    NULL},
};

void mxsonic_fpga_drivers_register(struct fpga_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_fpga_drivers_register);

void mxsonic_fpga_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_fpga_drivers_unregister);

static int __init switch_fpga_init(void)
{
    int err;
    int retval;
    int i,j;
    int fpga_index;
    char *fpga_index_str;

    fpga_index_str = (char *)kzalloc(8*sizeof(char), GFP_KERNEL);
    if (!fpga_index_str)
    {
        FPGA_DEBUG( "[%s] Fail to alloc fpga_index_str memory\n", __func__);
        return -ENOMEM;
    }

    /* For MXSONiC */
    fpga_kobj = kobject_create_and_add("fpga", mx_switch_kobj);
    if(!fpga_kobj)
    {
        FPGA_DEBUG( "[%s]Failed to create 'fpga'\n", __func__);
        err = -ENOMEM;
        goto sysfs_create_kobject_fpga_failed;
    }

    for(i=DEBUG; i <= NUM; i++)
    {
        FPGA_DEBUG( "[%s]sysfs_create_file /fpga/%s\n", __func__, fpga_attr[i].attr.name);
        retval = sysfs_create_file(fpga_kobj, &fpga_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", fpga_attr[i].attr.name);
            err = -retval;
            goto sysfs_create_mxsonic_attr_failed;
        }
    }

    for(fpga_index=0; fpga_index<FPGA_TOTAL_NUM; fpga_index++)
    {
        if(sprintf_s(fpga_index_str, 8, "fpga%d", fpga_index+FPGA_INDEX_START) < 0)
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
            FPGA_DEBUG( "sysfs_create_file /fpga/fpga%d/%s\n", fpga_index+FPGA_INDEX_START, fpga_attr[i].attr.name);
            retval = sysfs_create_file(fpga_index_kobj[fpga_index], &fpga_attr[i].attr);
            if(retval)
            {
                printk(KERN_ERR "Failed to create file '%s'\n", fpga_attr[i].attr.name);
                err = -retval;
                goto sysfs_create_mxsonic_fpga_attr_failed;
            }
        }

        if(fpga_index == 0)
        {
            for(i = 0; i < FPGA0_FMEA_KOBJ_NUM; i++)
            {
                fpga0_fmea_kobj[i] = kobject_create_and_add(fpga0_fmea_kobj_name[i], fpga_index_kobj[fpga_index]);
                if(!fpga0_fmea_kobj[i])
                {
                    FPGA_DEBUG( "Failed to create '%s'\n", fpga0_fmea_kobj_name[i]);
                    err = -ENOMEM;
                    goto sysfs_create_mxsonic_fpga_fmea_kobj_failed;
                }

                for(j = STATUS; j < NUM_FPGA_FMEA_ATTR; j++)
                {
                    FPGA_DEBUG( "sysfs_create_file /fpga/fpga%d/%s/%s\n", fpga_index, fpga0_fmea_kobj_name[i], fpga_fmea_attr[j].attr.name);
                    retval = sysfs_create_file(fpga0_fmea_kobj[i], &fpga_fmea_attr[j].attr);
                    if(retval)
                    {
                        printk(KERN_ERR "Failed to create file '%s'\n", fpga_fmea_attr[i].attr.name);
                        err = -retval;
                        goto sysfs_create_mxsonic_fpga_fmea_attr_failed;
                    }
                }
            }
        }
    }
    kfree(fpga_index_str);

    return 0;
sysfs_create_mxsonic_fpga_fmea_kobj_failed:
sysfs_create_mxsonic_fpga_fmea_attr_failed:
sysfs_create_mxsonic_fpga_attr_failed:
sysfs_create_kobject_switch_fpga_index_failed:
    for(i=DEBUG; i <= NUM; i++)
        sysfs_remove_file(fpga_kobj, &fpga_attr[i].attr);

    for(fpga_index=0; fpga_index<FPGA_TOTAL_NUM; fpga_index++)
    {
        if(fpga_index_kobj[fpga_index])
        {
            for(i=ALIAS; i < NUM_FPGA_ATTR; i++)
                sysfs_remove_file(fpga_index_kobj[fpga_index], &fpga_attr[i].attr);
            if(fpga_index == 0)
            {
                for(i = 0; i < FPGA0_FMEA_KOBJ_NUM; i++)
                {
                    if(fpga0_fmea_kobj[i])
                    {
                        for(j = STATUS; j < NUM_FPGA_FMEA_ATTR; j++)
                            sysfs_remove_file(fpga0_fmea_kobj[i], &fpga_fmea_attr[j].attr);
                    }

                    kobject_put(fpga0_fmea_kobj[i]);
                    fpga0_fmea_kobj[i] = NULL;
                }
            }

            kobject_put(fpga_index_kobj[fpga_index]);
            fpga_index_kobj[fpga_index] = NULL;
        }
    }

sysfs_create_mxsonic_attr_failed:
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
    int i,j;
    int fpga_index;

    /* For MXSONiC */
    for(i=DEBUG; i <= NUM; i++)
        sysfs_remove_file(fpga_kobj, &fpga_attr[i].attr);

    for(fpga_index=0; fpga_index<FPGA_TOTAL_NUM; fpga_index++)
    {
        if(fpga_index_kobj[fpga_index])
        {
            for(i=ALIAS; i < NUM_FPGA_ATTR; i++)
                sysfs_remove_file(fpga_index_kobj[fpga_index], &fpga_attr[i].attr);

            if(fpga_index == 0)
            {
                for(i = 0; i < FPGA0_FMEA_KOBJ_NUM; i++)
                {
                    if(fpga0_fmea_kobj[i])
                    {
                        for(j = STATUS; j < NUM_FPGA_FMEA_ATTR; j++)
                            sysfs_remove_file(fpga0_fmea_kobj[i], &fpga_fmea_attr[j].attr);
                    }

                    kobject_put(fpga0_fmea_kobj[i]);
                    fpga0_fmea_kobj[i] = NULL;
                }
            }

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
MODULE_DESCRIPTION("Muxi Switch MXSONiC fpga Driver");
MODULE_VERSION(SWITCH_MXSONIC_FPGA_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_fpga_init);
module_exit(switch_fpga_exit);