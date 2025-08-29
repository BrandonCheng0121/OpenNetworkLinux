#include <linux/module.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/slab.h>

#include "switch_transceiver_driver.h"
#include "switch_optoe.h"

#define SWITCH_S3IP_TRANSCEIVER_VERSION "0.0.0.1"

unsigned int loglevel = 0;

struct transceiver_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct transceiver_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct transceiver_attribute *attr, const char *buf, size_t count);
};

struct transceiver_drivers_t *cb_func = NULL;

/* For S3IP */
extern struct kobject *switch_kobj;
static struct kobject *transceiver_kobj;
static struct kobject *transceiver_index_kobj[TRANSCEIVER_TOTAL_NUM];

enum transceiver_attrs {
    PRESENT,
    NUM,
    ETH_POWER_ON,
    ETH_TX_FAULT,
    ETH_TX_DISABLE,
    ETH_RX_LOS,
    ETH_RESET,
    ETH_LPMODE,
    ETH_PRESENT,
    ETH_INTERRUPT,
#if 0
    ETH_EEPROM,
#endif
    NUM_TRANSCEIVER_ATTR,
};

unsigned int get_transceiver_index(struct kobject *kobj)
{
    int retval;
    unsigned int transceiver_index;
    char transceiver_index_str[5] = {0};

    memcpy_s(transceiver_index_str, 5, (kobject_name(kobj)+sizeof(ETH_NAME_STRING)-1), 2);
    retval = kstrtoint(transceiver_index_str, 10, &transceiver_index);
    if(retval == 0)
    {
        TRANSCEIVER_DEBUG("[%s] transceiver_index:%d \n", __func__, transceiver_index);
    }
    else
    {
        TRANSCEIVER_DEBUG("[%s] Error:%d, transceiver_index:%s \n", __func__, retval, transceiver_index_str);
        return -EINVAL;
    }

    return transceiver_index;
}

static ssize_t s3ip_debug_help(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    if(cb_func == NULL)
        return -1;

    return cb_func->debug_help(buf);
}

static ssize_t s3ip_get_loglevel(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    long lev;

    if(cb_func == NULL)
        return -1;

    cb_func->get_loglevel(&lev);

    return sprintf_s(buf, PAGE_SIZE, "%ld\n", lev);
}

static ssize_t s3ip_set_loglevel(struct kobject *kobj, struct transceiver_attribute *attr, const char *buf, size_t count)
{
    int retval;
    long lev;

    if(cb_func == NULL)
        return -1;

    retval = kstrtol(buf, 0, &lev);
    if(retval == 0)
    {
        TRANSCEIVER_DEBUG("lev:%ld \n", lev);
    }
    else
    {
        TRANSCEIVER_DEBUG("Error:%d, lev:%s \n", retval, buf);
        return -1;
    }

    loglevel = (unsigned int)lev;

    cb_func->set_loglevel(lev);

    return count;
}

static ssize_t s3ip_get_power_on(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%llx\n",1);
}

static ssize_t s3ip_get_present(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    long long unsigned int  present_value = 0;
    unsigned int status = 0;
    if(cb_func == NULL)
        return -1;
    /*
    rst_str[TRANSCEIVER_TOTAL_NUM] = '\0';
    for(transceiver_index = 1; transceiver_index <= TRANSCEIVER_TOTAL_NUM; transceiver_index++)
    {
        present_value = cb_func->get_eth_present(transceiver_index);
        rst_str[TRANSCEIVER_TOTAL_NUM - transceiver_index] = present_value ? '1':'0';
    }

    return sprintf_s(buf, PAGE_SIZE, "%s\n", rst_str);
    */
    status = cb_func->get_present_all(&present_value);
    if(status < 0)
    {
        return status;
    }
    return sprintf_s(buf, PAGE_SIZE, "%llx\n",present_value);
}

static ssize_t s3ip_get_num(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%d\n", TRANSCEIVER_TOTAL_NUM);
}

static ssize_t s3ip_get_eth_power_on(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%llx\n",1);
}

static ssize_t s3ip_get_eth_tx_fault(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    int transceiver_index = 0;
    unsigned char tx_fault_buf[2] = {0};
    unsigned char lane_num = 0;
    unsigned char tx_fault = 0;
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    transceiver_index = get_transceiver_index(kobj);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_eth_tx_fault(transceiver_index, tx_fault_buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }

    lane_num = tx_fault_buf[0];
    tx_fault = tx_fault_buf[1];
    return sprintf_s(buf, PAGE_SIZE, "%d\n", tx_fault);
}

static ssize_t s3ip_get_eth_tx_disable(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    int transceiver_index = 0;
    unsigned char tx_disable_buf[2] = {0};
    unsigned char lane_num = 0;
    unsigned char tx_disable = 0;
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    transceiver_index = get_transceiver_index(kobj);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_eth_tx_disable(transceiver_index, tx_disable_buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }

    lane_num = tx_disable_buf[0];
    tx_disable = tx_disable_buf[1];

    return sprintf_s(buf, PAGE_SIZE, "%d\n", tx_disable);
}

static ssize_t s3ip_set_eth_tx_disable(struct kobject *kobj, struct transceiver_attribute *attr, const char *buf, size_t count)
{
    unsigned int rst_value = 0;
    unsigned int transceiver_index = 0;
    int retval;

    if(cb_func == NULL)
        return -1;

    transceiver_index = get_transceiver_index(kobj);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        return -EINVAL;
    }

    retval = kstrtoint(buf, 0, &rst_value);
    if(retval != 0)
    {
        return -EINVAL;
    }
    if(transceiver_index <= SFP_END)
        cb_func->set_eth_tx_disable(transceiver_index, rst_value);
    else
        return -ENOSYS;
    return count;
}


static ssize_t s3ip_get_eth_rx_los(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    int transceiver_index = 0;
    unsigned char rx_los_buf[2] = {0};
    unsigned char lane_num = 0;
    unsigned char rx_los = 0;
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    transceiver_index = get_transceiver_index(kobj);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->get_eth_rx_los(transceiver_index, rx_los_buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }

    lane_num = rx_los_buf[0];
    rx_los = rx_los_buf[1];

    return sprintf_s(buf, PAGE_SIZE, "%d\n", rx_los);
}

static ssize_t s3ip_set_eth_power_on(struct kobject *kobj, struct transceiver_attribute *attr, const char *buf, size_t count)
{
    unsigned int pwr_value = 0;
    unsigned int transceiver_index = 0;
    int retval;
    return -ENOSYS;

    if(cb_func == NULL)
        return -1;

    transceiver_index = get_transceiver_index(kobj);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        return -EINVAL;
    }

    retval = kstrtoint(buf, 0, &pwr_value);
    if(retval != 0)
    {
        return -EINVAL;
    }

    cb_func->set_eth_power_on(transceiver_index, pwr_value);

    return count;
}

static ssize_t s3ip_get_eth_reset(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    unsigned int rst_value = 0;
    int transceiver_index = 0;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    transceiver_index = get_transceiver_index(kobj);
    TRANSCEIVER_DEBUG("[%s] Get transceiver index is %d .\n", __func__, transceiver_index);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        ERROR_RETURN_NA(-EINVAL);
    }

    if(transceiver_index > SFP_END)
        rst_value = cb_func->get_eth_reset(transceiver_index);
    else
        return sprintf_s(buf, PAGE_SIZE, "%d\n", 0);
    return sprintf_s(buf, PAGE_SIZE, "%d\n", rst_value);
}

static ssize_t s3ip_set_eth_reset(struct kobject *kobj, struct transceiver_attribute *attr, const char *buf, size_t count)
{
    unsigned int rst_value = 0;
    int transceiver_index = 0;
    int retval;

    if(cb_func == NULL)
        return -1;

    transceiver_index = get_transceiver_index(kobj);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        return -EINVAL;
    }

    retval = kstrtoint(buf, 0, &rst_value);
    if(retval != 0)
    {
        return -EINVAL;
    }

    if(transceiver_index > SFP_END)
        cb_func->set_eth_reset(transceiver_index, rst_value);
    else
        return -ENOSYS;

    return count;
}

static ssize_t s3ip_get_eth_lpmode(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    unsigned int lpmode_value = 0;
    int transceiver_index = 0;
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    transceiver_index = get_transceiver_index(kobj);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        ERROR_RETURN_NA(-EINVAL);
    }

    if(transceiver_index > SFP_END)
    {
        retval = cb_func->get_eth_lpmode(transceiver_index, &lpmode_value);
        if(retval < 0)
        {
            ERROR_RETURN_NA(retval);
        }
    }else
        return sprintf_s(buf, PAGE_SIZE, "%d\n", 0);
    return sprintf_s(buf, PAGE_SIZE, "%d\n", lpmode_value);
}

static ssize_t s3ip_set_eth_lpmode(struct kobject *kobj, struct transceiver_attribute *attr, const char *buf, size_t count)
{
    unsigned int lpmode_value = 0;
    int transceiver_index = 0;
    int retval;

    if(cb_func == NULL)
        return -1;

    transceiver_index = get_transceiver_index(kobj);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        return -EINVAL;
    }
    if(transceiver_index <= SFP_END)
        return -ENOSYS;

    retval = kstrtoint(buf, 0, &lpmode_value);
    if(retval != 0)
    {
        return -EINVAL;
    }

    retval = cb_func->set_eth_lpmode(transceiver_index, lpmode_value);
    if(retval < 0)
    {
        return retval;
    }

    return count;
}

static ssize_t s3ip_get_eth_present(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    unsigned int present_value = 0;
    unsigned int transceiver_index = 0;

    if(cb_func == NULL)
        return -1;

    transceiver_index = get_transceiver_index(kobj);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        return -EINVAL;
    }

    present_value = cb_func->get_eth_present(transceiver_index);

    return sprintf_s(buf, PAGE_SIZE, "%d\n", present_value);
}

static ssize_t s3ip_get_eth_interrupt(struct kobject *kobj, struct transceiver_attribute *attr, char *buf)
{
    unsigned int interrupt_value = 0;
    unsigned int transceiver_index = 0;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    transceiver_index = get_transceiver_index(kobj);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        ERROR_RETURN_NA(-EINVAL);
    }
    if(transceiver_index <= SFP_END)
        return sprintf_s(buf, PAGE_SIZE, "%d\n", 0);

    interrupt_value = cb_func->get_eth_interrupt(transceiver_index);

    return sprintf_s(buf, PAGE_SIZE, "%d\n", interrupt_value);
}

static ssize_t s3ip_get_eth_eeprom(struct kobject *kobj,struct transceiver_attribute *attr, char *buf, loff_t off, size_t count)
{
    unsigned int transceiver_index = 0;

    if(cb_func == NULL)
        return -1;

    transceiver_index = get_transceiver_index(kobj);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        return -EINVAL;
    }

    return cb_func->get_eth_eeprom(transceiver_index, buf, off, count);
}

static ssize_t s3ip_set_eth_eeprom(struct kobject *kobj, struct transceiver_attribute *attr, char *buf, loff_t off, size_t count)
{
    unsigned int transceiver_index = 0;
    char eeprom_buf[ONE_ADDR_EEPROM_SIZE] = {0};

    if(cb_func == NULL)
        return -1;

    transceiver_index = get_transceiver_index(kobj);
    if(transceiver_index < 0)
    {
        TRANSCEIVER_DEBUG("[%s] Get transceiver index failed.\n", __func__);
        return -EINVAL;
    }

    if(count > ONE_ADDR_EEPROM_SIZE)
    {
        return -EINVAL;
    }
    memcpy_s(eeprom_buf, ONE_ADDR_EEPROM_SIZE, buf, count);

    return cb_func->set_eth_eeprom(transceiver_index, eeprom_buf, off, count);
}

static struct transceiver_attribute transceiver_attr[NUM_TRANSCEIVER_ATTR] = {
    [PRESENT]       = {{.name = "present",          .mode = S_IRUGO},           s3ip_get_present,    NULL},
    [NUM]           = {{.name = "port_num",         .mode = S_IRUGO},           s3ip_get_num,    NULL},
    [ETH_POWER_ON]  = {{.name = "power_on",         .mode = S_IRUGO},           s3ip_get_eth_power_on, NULL},
    [ETH_TX_FAULT]  = {{.name = "tx_fault",         .mode = S_IRUGO},           s3ip_get_eth_tx_fault,   NULL},
    [ETH_TX_DISABLE]= {{.name = "tx_disable",       .mode = S_IRUGO | S_IWUSR}, s3ip_get_eth_tx_disable, s3ip_set_eth_tx_disable},
    [ETH_RX_LOS]    = {{.name = "rx_los",           .mode = S_IRUGO},           s3ip_get_eth_rx_los,     NULL},
    [ETH_RESET]     = {{.name = "reset",            .mode = S_IRUGO | S_IWUSR}, s3ip_get_eth_reset,      s3ip_set_eth_reset},
    [ETH_LPMODE]    = {{.name = "low_power_mode",   .mode = S_IRUGO | S_IWUSR}, s3ip_get_eth_lpmode,     s3ip_set_eth_lpmode},
    [ETH_PRESENT]   = {{.name = "present",          .mode = S_IRUGO},           s3ip_get_eth_present,    NULL},
    [ETH_INTERRUPT] = {{.name = "interrupt",        .mode = S_IRUGO},           s3ip_get_eth_interrupt,  NULL},
#if 0
    [ETH_EEPROM]    = {{.name = "eeprom",           .mode = S_IRUGO | S_IWUSR}, s3ip_get_eth_eeprom,     s3ip_set_eth_eeprom},
#endif
};

void s3ip_transceiver_drivers_register(struct transceiver_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(s3ip_transceiver_drivers_register);

void s3ip_transceiver_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(s3ip_transceiver_drivers_unregister);

static int __init switch_transceiver_init(void)
{
    int err;
    int retval;
    int i;
    int transceiver_index;
    char *transceiver_index_str = NULL;

    transceiver_index_str = (char *)kzalloc(sizeof(ETH_NAME_STRING)+2, GFP_KERNEL);
    if (!transceiver_index_str)
    {
        TRANSCEIVER_DEBUG( "[%s] Fail to alloc transceiver_index_str memory\n", __func__);
        return -ENOMEM;
    }
    if(!switch_kobj){

        TRANSCEIVER_DEBUG( "[%s]Failed to create 'transceiver' switch_kobj is NULL\n", __func__);
        goto sysfs_create_kobject_transceiver_failed;
    }
    /* For S3IP */
    transceiver_kobj = kobject_create_and_add(TRANSCEIVER_NAME_STRING, switch_kobj);
    if(!transceiver_kobj)
    {
        TRANSCEIVER_DEBUG( "[%s]Failed to create 'transceiver'\n", __func__);
        err = -ENOMEM;
        goto sysfs_create_kobject_transceiver_failed;
    }

    for(i=0; i < ETH_POWER_ON; i++)
    {
        TRANSCEIVER_DEBUG( "[%s]sysfs_create_file /transceiver/%s\n", __func__, transceiver_attr[i].attr.name);
        retval = sysfs_create_file(transceiver_kobj, &transceiver_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", transceiver_attr[i].attr.name);
            err = -retval;
            goto sysfs_create_s3ip_attr_failed;
        }
    }

    for(transceiver_index=0; transceiver_index<TRANSCEIVER_TOTAL_NUM; transceiver_index++)
    {
        if(sprintf_s(transceiver_index_str, sizeof(ETH_NAME_STRING)+2, "%s%d", ETH_NAME_STRING, transceiver_index+TRANSCEIVER_INDEX_START) < 0)
        {
            err = -ENOMEM;
            goto sysfs_create_kobject_switch_transceiver_index_failed;
        }
        transceiver_index_kobj[transceiver_index] = kobject_create_and_add(transceiver_index_str, transceiver_kobj);
        if(!transceiver_index_kobj[transceiver_index])
        {
            TRANSCEIVER_DEBUG( "[%s]Failed to create 'transceiver%d'\n", __func__, transceiver_index+TRANSCEIVER_INDEX_START);
            err = -ENOMEM;
            goto sysfs_create_kobject_switch_transceiver_index_failed;
        }

        for(i=ETH_POWER_ON; i < NUM_TRANSCEIVER_ATTR; i++)
        {
            TRANSCEIVER_DEBUG( "[%s]sysfs_create_file /transceiver/transceiver%d/%s\n", __func__, transceiver_index+TRANSCEIVER_INDEX_START, transceiver_attr[i].attr.name);
            retval = sysfs_create_file(transceiver_index_kobj[transceiver_index], &transceiver_attr[i].attr);
            if(retval)
            {
                printk(KERN_ERR "Failed to create file '%s'\n", transceiver_attr[i].attr.name);
                err = -retval;
                goto sysfs_create_s3ip_transceiver_attr_failed;
            }
        }
    }

    kfree(transceiver_index_str);

    return 0;

sysfs_create_s3ip_transceiver_attr_failed:
sysfs_create_kobject_switch_transceiver_index_failed:
    for(i=0; i < ETH_POWER_ON; i++)
        sysfs_remove_file(transceiver_kobj, &transceiver_attr[i].attr);

    for(transceiver_index=0; transceiver_index<TRANSCEIVER_TOTAL_NUM; transceiver_index++)
    {
        if(transceiver_index_kobj[transceiver_index])
        {
            for(i=ETH_POWER_ON; i < NUM_TRANSCEIVER_ATTR; i++)
                sysfs_remove_file(transceiver_index_kobj[transceiver_index], &transceiver_attr[i].attr);

            kobject_put(transceiver_index_kobj[transceiver_index]);
            transceiver_index_kobj[transceiver_index] = NULL;
        }
    }

sysfs_create_s3ip_attr_failed:
    if(transceiver_kobj)
    {
        kobject_put(transceiver_kobj);
        transceiver_kobj = NULL;
    }

sysfs_create_kobject_transceiver_failed:
    kfree(transceiver_index_str);

    return err;
}

static void __exit switch_transceiver_exit(void)
{
    int i;
    int transceiver_index;

    /* For S3IP */
    for(i=0; i < ETH_POWER_ON; i++)
        sysfs_remove_file(transceiver_kobj, &transceiver_attr[i].attr);

    for(transceiver_index=0; transceiver_index<TRANSCEIVER_TOTAL_NUM; transceiver_index++)
    {
        if(transceiver_index_kobj[transceiver_index])
        {
            for(i=ETH_POWER_ON; i < NUM_TRANSCEIVER_ATTR; i++)
                sysfs_remove_file(transceiver_index_kobj[transceiver_index], &transceiver_attr[i].attr);

            kobject_put(transceiver_index_kobj[transceiver_index]);
            transceiver_index_kobj[transceiver_index] = NULL;
        }
    }

    if(transceiver_kobj)
    {
        kobject_put(transceiver_kobj);
        transceiver_kobj = NULL;
    }

    cb_func = NULL;

    return;
}

MODULE_AUTHOR("Will Chen <will_chen@accton.com>");
MODULE_DESCRIPTION("Huarong Switch S3IP TRANSCEIVER Driver");
MODULE_VERSION(SWITCH_S3IP_TRANSCEIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_transceiver_init);
module_exit(switch_transceiver_exit);
