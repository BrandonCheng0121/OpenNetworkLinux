#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/kernel.h>

#include "switch_pcie_driver.h"

#define SWITCH_MXSONiC_PCIE_VERSION "0.0.0.1"

unsigned int loglevel = 0;
struct pcie_drivers_t *cb_func = NULL;

extern struct kobject *mx_switch_kobj;
static struct kobject *pcie_kobj;
static struct kobject *fpga_pcie_status_kobj;
static struct kobject *lsw_pcie_status_kobj;
static struct kobject *lan_pcie_status_kobj;

enum pcie_attrs {
    LINK_STATUS,
    SPEED_STATUS,
    NUM_PCIE_ATTR
};

struct pcie_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct pcie_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct pcie_attribute *attr, const char *buf, size_t count);
    u8 reg;
};

void release_pcie_kobj_and_attribute(int num_attr, struct kobject **parent, const struct pcie_attribute *attr)
{
    int j = 0;

    for(j = 0; j < num_attr; j++)
    {
        sysfs_remove_file((*parent), &(attr[j].attr));
    }

    if(*parent)
    {
        kobject_put(*parent);
        *parent = NULL;
    }
}

static ssize_t mxsonic_fpga_link_status_read(struct kobject *kobj, struct pcie_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->fpga_link_status_read(attr->reg, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_fpga_speed_status_read(struct kobject *kobj, struct pcie_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->fpga_speed_status_read(attr->reg, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_lsw_link_status_read(struct kobject *kobj, struct pcie_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->lsw_link_status_read(attr->reg, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_lsw_speed_status_read(struct kobject *kobj, struct pcie_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->lsw_speed_status_read(attr->reg, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_lan_link_status_read(struct kobject *kobj, struct pcie_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->lan_link_status_read(attr->reg, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_lan_speed_status_read(struct kobject *kobj, struct pcie_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    retval = cb_func->lan_speed_status_read(attr->reg, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static struct pcie_attribute fpga_attr[NUM_PCIE_ATTR] = {
    [LINK_STATUS]           = {{.name = LINK_STATUS_NAME,          .mode = 0444}, mxsonic_fpga_link_status_read, NULL, FPGA_ICH_LINK_STATUS_REG_OFFSET},
    [SPEED_STATUS]          = {{.name = SPEED_STATUS_NAME,         .mode = 0444}, mxsonic_fpga_speed_status_read, NULL, FPGA_ICH_LINK_STATUS_REG_OFFSET},
};

static struct pcie_attribute lsw_attr[NUM_PCIE_ATTR] = {
    [LINK_STATUS]           = {{.name = LINK_STATUS_NAME,          .mode = 0444}, mxsonic_lsw_link_status_read, NULL, LSW_ICH_LINK_STATUS_REG_OFFSET},
    [SPEED_STATUS]          = {{.name = SPEED_STATUS_NAME,         .mode = 0444}, mxsonic_lsw_speed_status_read, NULL, LSW_ICH_LINK_STATUS_REG_OFFSET},
};

static struct pcie_attribute lan_attr[NUM_PCIE_ATTR] = {
    [LINK_STATUS]           = {{.name = LINK_STATUS_NAME,          .mode = 0444}, mxsonic_lan_link_status_read, NULL,LAN_ICH_LINK_STATUS_REG_OFFSET},
    [SPEED_STATUS]          = {{.name = SPEED_STATUS_NAME,         .mode = 0444}, mxsonic_lan_speed_status_read, NULL,LAN_ICH_LINK_STATUS_REG_OFFSET},
};

void mxsonic_pcie_drivers_register(struct pcie_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_pcie_drivers_register);

void mxsonic_pcie_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_pcie_drivers_unregister);

static int __init switch_pcie_init(void)
{
    int retval;
    int i;

    /*pcie check*/
    pcie_kobj = kobject_create_and_add("pcie", mx_switch_kobj);
    if(!pcie_kobj)
    {
        printk(KERN_ERR "[%s]Failed to create 'pcie'\n", __func__);
        retval = -ENOMEM;
        goto err_create_pcie_file_lable;
    }

    /*fpga*/
    fpga_pcie_status_kobj = kobject_create_and_add("fpga", pcie_kobj);
    if(!fpga_pcie_status_kobj)
    {
        printk(KERN_ERR "[%s]Failed to create 'fpga'\n", __func__);
        retval = -ENOMEM;
        goto err_create_fpga_pcie_status_kobj_lable;
    }

    for(i = 0; i < NUM_PCIE_ATTR;i++)
    {
        PCIE_DEBUG( "[%s]sysfs_create_file /pcie/fpga/%s\n", __func__, fpga_attr[i].attr.name);
        retval = sysfs_create_file(fpga_pcie_status_kobj, &fpga_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", fpga_attr[i].attr.name);
            retval = -ENOMEM;
            goto err_create_fpga_file_lable;
        }
    }

    /*lsw*/
    lsw_pcie_status_kobj = kobject_create_and_add("lsw", pcie_kobj);
    if(!lsw_pcie_status_kobj)
    {
        printk(KERN_ERR "[%s]Failed to create 'lsw'\n", __func__);
        retval = -ENOMEM;
        goto err_create_lsw_pcie_status_kobj_lable;
    }

    for(i = 0; i < NUM_PCIE_ATTR;i++)
    {
        PCIE_DEBUG( "[%s]sysfs_create_file /pcie/lsw/%s\n", __func__, lsw_attr[i].attr.name);
        retval = sysfs_create_file(lsw_pcie_status_kobj, &lsw_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", lsw_attr[i].attr.name);
            retval = -ENOMEM;
            goto err_create_lsw_file_lable;
        }
    }

    /*lan*/
    lan_pcie_status_kobj = kobject_create_and_add("lan", pcie_kobj);
    if(!lan_pcie_status_kobj)
    {
        printk(KERN_ERR "[%s]Failed to create 'lan'\n", __func__);
        retval = -ENOMEM;
        goto err_create_lan_pcie_status_kobj_lable;
    }

    for(i = 0; i < NUM_PCIE_ATTR;i++)
    {
        PCIE_DEBUG( "[%s]sysfs_create_file /pcie/lan/%s\n", __func__, lan_attr[i].attr.name);
        retval = sysfs_create_file(lan_pcie_status_kobj, &lan_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", lan_attr[i].attr.name);
            retval = -ENOMEM;
            goto err_create_lan_file_lable;
        }
    }

    return 0;
err_create_lan_file_lable:
    release_pcie_kobj_and_attribute(i, &lan_pcie_status_kobj, lan_attr);

err_create_lan_pcie_status_kobj_lable:
err_create_lsw_file_lable:
    release_pcie_kobj_and_attribute(i, &lsw_pcie_status_kobj, lsw_attr);

err_create_lsw_pcie_status_kobj_lable:
err_create_fpga_file_lable:
    release_pcie_kobj_and_attribute(i, &fpga_pcie_status_kobj, fpga_attr);

err_create_fpga_pcie_status_kobj_lable:
err_create_pcie_file_lable:
    kobject_put(pcie_kobj);
    pcie_kobj = NULL;

    return retval;
}

static void __exit switch_pcie_exit(void)
{
    release_pcie_kobj_and_attribute(NUM_PCIE_ATTR, &lan_pcie_status_kobj, lan_attr);
    release_pcie_kobj_and_attribute(NUM_PCIE_ATTR, &lsw_pcie_status_kobj, lsw_attr);
    release_pcie_kobj_and_attribute(NUM_PCIE_ATTR, &fpga_pcie_status_kobj, fpga_attr);

    if(pcie_kobj)
    {
        kobject_put(pcie_kobj);
        pcie_kobj = NULL;
    }

    cb_func = NULL;

    return;
}

MODULE_DESCRIPTION("Switch MXSONiC PCIE Driver");
MODULE_VERSION(SWITCH_MXSONiC_PCIE_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_pcie_init);
module_exit(switch_pcie_exit);
