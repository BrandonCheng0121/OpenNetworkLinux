/*
 * A hwmon driver for the Accton as5710 54x fan contrl
 *
 * Copyright (C) 2013 Accton Technology Corporation.
 * Brandon Chuang <brandon_chuang@accton.com.tw>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/module.h>
#include <linux/jiffies.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/err.h>
#include <linux/sysfs.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/kthread.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/printk.h>
#include <linux/list.h>
#include "cpld_i2c_driver.h"


#define DRV_NAME "cpld_i2c_driver"
//#define CPLD_FAN_DEBUG

#define I2C_RW_RETRY_COUNT				10
#define I2C_RW_RETRY_INTERVAL			60 /* ms */
#define ATTR_ALLOC_SIZE	1   /*For last attribute which is NUll.*/
#define NAME_SIZE		24


/* Added by Wesley */
struct cpld_client_node {
    struct i2c_client *client;
    struct list_head   list;
};
static LIST_HEAD(cpld_client_list);
static struct mutex	 list_lock;
/* End of added */


static const struct i2c_device_id  _cpld_id[] = {
    { "muxi_sys_cpld", sys_cpld},
    { "muxi_led_cpld1", led_cpld1},
    { },
};
MODULE_DEVICE_TABLE(i2c, _cpld_id);


/*add cpld client list */
static void _i2c_cpld_add_client(struct i2c_client *client)
{
    struct cpld_client_node *node =
        kzalloc(sizeof(struct cpld_client_node), GFP_KERNEL);

    if (!node) {
        dev_dbg(&client->dev, "Can't allocate cpld_client_node (0x%x)\n",
                client->addr);
        return;
    }
    node->client = client;
    mutex_lock(&list_lock);
    list_add(&node->list, &cpld_client_list);
    mutex_unlock(&list_lock);
}

static void _i2c_cpld_remove_client(struct i2c_client *client)
{
    struct list_head		*list_node = NULL;
    struct cpld_client_node *cpld_node = NULL;
    int found = 0;

    mutex_lock(&list_lock);

    list_for_each(list_node, &cpld_client_list)
    {
        cpld_node = list_entry(list_node, struct cpld_client_node, list);

        if (cpld_node->client == client) {
            found = 1;
            break;
        }
    }

    if (found) {
        list_del(list_node);
        kfree(cpld_node);
    }

    mutex_unlock(&list_lock);
}

int _i2c_cpld_read(u8 cpld_addr,u8 device_id ,u8 reg)
{
    struct list_head   *list_node = NULL;
    struct cpld_client_node *cpld_node = NULL;
    int ret = -EPERM;

    mutex_lock(&list_lock);

    list_for_each(list_node, &cpld_client_list)
    {
        cpld_node = list_entry(list_node, struct cpld_client_node, list);
        if(!strcmp(_cpld_id[device_id].name,cpld_node->client->name)){
            if (cpld_node->client->addr == cpld_addr) {
                ret = i2c_smbus_read_byte_data(cpld_node->client, reg);
                break;
            }
        }
    }

    mutex_unlock(&list_lock);
    return ret;
}
EXPORT_SYMBOL(_i2c_cpld_read);

int _i2c_cpld_write(u8 cpld_addr,u8 device_id , u8 reg, u8 value)
{
    struct list_head   *list_node = NULL;
    struct cpld_client_node *cpld_node = NULL;
    int ret = -EIO;

    mutex_lock(&list_lock);

    list_for_each(list_node, &cpld_client_list)
    {
        cpld_node = list_entry(list_node, struct cpld_client_node, list);
        if(!strcmp(_cpld_id[device_id].name,cpld_node->client->name)){
            if (cpld_node->client->addr == cpld_addr) {
                ret = i2c_smbus_write_byte_data(cpld_node->client, reg, value);
                break;
            }
        }
    }

    mutex_unlock(&list_lock);

    return ret;
}
EXPORT_SYMBOL(_i2c_cpld_write);
/* End of added */

static int cpld_probe(struct i2c_client *client,
                     const struct i2c_device_id *dev_id)
{
    int status;
    struct i2c_adapter *adap = to_i2c_adapter(client->dev.parent);
    struct device *dev = &client->dev;

    if (!i2c_check_functionality(adap, I2C_FUNC_SMBUS_BYTE_DATA))
        return -ENODEV;

    _i2c_cpld_add_client(client);

    dev_info(dev, "cpld '%s'\n",
              client->name);

    return 0;

    return status;
}


static int cpld_remove(struct i2c_client *client)
{

	_i2c_cpld_remove_client(client);
    return 0;
}

static const unsigned short normal_i2c[] = { I2C_CLIENT_END };

static struct i2c_driver _i2c_cpld_driver = {
    .class        = I2C_CLASS_HWMON,
    .driver = {
        .name = DRV_NAME,
    },
    .probe		= cpld_probe,
    .remove	   	= cpld_remove,
    .id_table     = _cpld_id,

};

static int __init _i2c_cpld_init(void)
{
	mutex_init(&list_lock);
    return i2c_add_driver(&_i2c_cpld_driver);
}

static void __exit _i2c_cpld_exit(void)
{
    i2c_del_driver(&_i2c_cpld_driver);
}

MODULE_AUTHOR("Brandon Chuang <brandon_chuang@accton.com.tw>");
MODULE_DESCRIPTION("_i2c_cpld driver");
MODULE_LICENSE("GPL");

module_init(_i2c_cpld_init);
module_exit(_i2c_cpld_exit);

