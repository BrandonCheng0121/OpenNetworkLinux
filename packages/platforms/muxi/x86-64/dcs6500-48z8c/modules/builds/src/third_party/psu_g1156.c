/*
 * Hardware monitoring driver for psu_g1156
 *
 * Copyright (c) 2015 Accton Technology
 * Copyright (c) 2015 Zhenling Yin
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/err.h>
#include <linux/init.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/hwmon-sysfs.h>

#include "pmbus.h"
#include "switch_psu_driver.h"
#include "psu_g1156.h"
#include "libboundscheck/include/securec.h"
#define MAX_BUFF_SIZE (1024)
#define PMBUS_MFR_MAX_LEN   (31)
struct i2c_client *s3ip_psu_client[PSU_TOTAL_NUM] = {NULL};

static int two_complement_to_int(u16 data, u8 valid_bit, int mask)
{
    u16  valid_data  = data & mask;
    bool is_negative = valid_data >> (valid_bit - 1);

    return is_negative ? (-(((~valid_data) & mask) + 1)) : valid_data;
}

static int pmbus_linear11_to_val(int data, long *val)
{
    int exponent, mantissa;
    int temp_value = 0;
    int multiplier = 1000;

    if ((data < 0) || (data == 0xffff))
    {
        return -EINVAL;
    }

    exponent = two_complement_to_int(data >> 11, 5, 0x1f);
    mantissa = two_complement_to_int(data & 0x7ff, 11, 0x7ff);
    if (exponent >= 0)
    {
        temp_value = (mantissa << exponent) * multiplier;
    }
    else
    {
        temp_value = (mantissa * multiplier) / (1 << -exponent);
    }
    *val = temp_value;
    return 0;
}

static int pmbus_linear16_to_val(int data, int data_exponent, long *val)
{
    int exponent, mantissa;
    int temp_value = 0;
    int multiplier = 1000;

    if (data < 0)
    {
        return data;
    }
    if (data_exponent < 0)
    {
        return data_exponent;
    }
    exponent = two_complement_to_int(data_exponent, 5, 0x1f);
    mantissa = data;
    if (exponent >= 0)
    {
        temp_value = (mantissa << exponent) * multiplier;
    }
    else
    {
        temp_value = (mantissa * multiplier) / (1 << -exponent);
    }
    *val = temp_value;
    return 0;
}

int psu_pmbus_read_data(int psu_index, u8 pmbus_command, long *val)
{
    int read_value = 0;
    int vout_mode = 0;
    int ret = 0;

    if ((psu_index >= PSU_TOTAL_NUM) || (s3ip_psu_client[psu_index] == NULL))
    {
        pr_err("[%s] index err: psu_index:%d \n", __func__, psu_index);
        return -EINVAL;
    }
    switch(pmbus_command)
    {
        // word date linear11
        case PMBUS_READ_VIN:
        case PMBUS_READ_IIN:
        case PMBUS_READ_IOUT:
        case PMBUS_READ_TEMPERATURE_1:// should provide the PSU inlet temperature.
        case PMBUS_READ_TEMPERATURE_2:// should provide the temperature of the SR heat sink in the PSU.
        case PMBUS_READ_TEMPERATURE_3:// should provide the temperature of the PFC heat sink in the PSU.
            read_value = i2c_smbus_read_word_data(s3ip_psu_client[psu_index], pmbus_command);
            if (read_value < 0)
                return read_value;
            ret = pmbus_linear11_to_val(read_value, val);
            break;
        case PMBUS_READ_PIN:
        case PMBUS_READ_POUT:
            read_value = i2c_smbus_read_word_data(s3ip_psu_client[psu_index], pmbus_command);
            if (read_value < 0)
                return read_value;
            ret = pmbus_linear11_to_val(read_value, val);
            *val = *val * 1000;
            break;
        case PMBUS_READ_FAN_SPEED_1:
            read_value = i2c_smbus_read_word_data(s3ip_psu_client[psu_index], pmbus_command);
            if (read_value < 0)
                return read_value;
            ret = pmbus_linear11_to_val(read_value, val);
            *val = *val / 1000;
            break;
        case PMBUS_POUT_MAX:
            *val = 650*1000*1000;//uW
            break;
        case PMBUS_READ_VOUT:
            read_value = i2c_smbus_read_word_data(s3ip_psu_client[psu_index], pmbus_command);
            if (read_value < 0)
                return read_value;
            vout_mode = i2c_smbus_read_byte_data(s3ip_psu_client[psu_index], PMBUS_VOUT_MODE);
            if (vout_mode < 0)
                return vout_mode;
            ret = pmbus_linear16_to_val(read_value, vout_mode, val);
            break;
        case PMBUS_STATUS_WORD:
            read_value = i2c_smbus_read_word_data(s3ip_psu_client[psu_index], pmbus_command);
            if (read_value < 0)
                return read_value;
            *val = read_value;
        case PMBUS_STATUS_BYTE:
        case PMBUS_STATUS_INPUT:
        case PMBUS_STATUS_VOUT:
        case PMBUS_STATUS_IOUT:
        case PMBUS_STATUS_TEMPERATURE:
        case PMBUS_STATUS_CML:
        case PMBUS_STATUS_FAN_12:
        case PMBUS_MFR_PSU_VIN_TYPE://just this PSU
        case PMBUS_READ_LED_STATUS://just this PSU
            read_value = i2c_smbus_read_byte_data(s3ip_psu_client[psu_index], pmbus_command);
            if (read_value < 0)
                return read_value;
            *val = read_value;
            break;
        default:
            return -EINVAL;
            break;
    }

    return ret;
}
EXPORT_SYMBOL_GPL(psu_pmbus_read_data);

int psu_pmbus_read_char(int psu_index, u8 pmbus_command, char *buf)
{
    int read_value = 0;
    int ret = 0;
    int page = 0;
    char block_buf[PMBUS_MFR_MAX_LEN] = {0};

    if ((psu_index >= PSU_TOTAL_NUM) || (s3ip_psu_client[psu_index] == NULL))
    {
        pr_err("[%s] index err: psu_index:%d \n", __func__, psu_index);
        return -EINVAL;
    }

    switch (pmbus_command)
    {
    // block date
    case PMBUS_MFR_DATE:
    case PMBUS_MFR_MODEL:
    case PMBUS_MFR_SERIAL:
    case PMBUS_MFR_ID:
    case PMBUS_MFR_REVISION:
        read_value = i2c_smbus_read_byte_data(s3ip_psu_client[psu_index], pmbus_command);
        if((read_value >= (PMBUS_MFR_MAX_LEN-1)) || (read_value < 0))
        {
            return -EINVAL;
        }
        ret = i2c_smbus_read_i2c_block_data(s3ip_psu_client[psu_index], pmbus_command, read_value+1, block_buf);
        if(ret < 0)
            return ret;
        return sprintf_s(buf, PMBUS_MFR_MAX_LEN - 1, "%s\n", &block_buf[1]);
        break;
    default:
        return -EINVAL;
        break;
    }
    return 0;
}
EXPORT_SYMBOL_GPL(psu_pmbus_read_char);

static struct pmbus_driver_info psu_g1156_info = {
    .pages = 1,
    .format[PSC_VOLTAGE_IN] = linear,
    .format[PSC_VOLTAGE_OUT] = linear,
    .format[PSC_CURRENT_IN] = linear,
    .format[PSC_CURRENT_OUT] = linear,
    .format[PSC_POWER] = linear,
    .format[PSC_TEMPERATURE] = linear,
    .format[PSC_FAN] = direct,
    .func[0] = PMBUS_HAVE_VIN | PMBUS_HAVE_VOUT
        | PMBUS_HAVE_IIN | PMBUS_HAVE_IOUT
        | PMBUS_HAVE_PIN | PMBUS_HAVE_POUT
        | PMBUS_HAVE_TEMP | PMBUS_HAVE_TEMP2 | PMBUS_HAVE_TEMP3
        | PMBUS_HAVE_FAN12
        | PMBUS_HAVE_STATUS_VOUT | PMBUS_HAVE_STATUS_IOUT
        | PMBUS_HAVE_STATUS_INPUT | PMBUS_HAVE_STATUS_TEMP
        | PMBUS_HAVE_STATUS_FAN12,
    //.groups = psu_g1156_group,
};

enum chips {
    psu1, psu2
};
//The name for all PSU must be different, for KS SONiC
static const struct i2c_device_id psu_g1156_id[] = {
    {PSU_G1156_NAME_1, psu1},
    {PSU_G1156_NAME_2, psu2},
    { }
};
MODULE_DEVICE_TABLE(i2c, psu_g1156_id);

static int psu_g1156_probe(struct i2c_client *client,
             const struct i2c_device_id *id)
{
    struct pmbus_driver_info *info;
    int i = 0;
    int status = 0;
    struct device *dev = &client->dev;
    enum chips chip_id;

    // if (client->dev.of_node)
    //     chip_id = (enum chips)(unsigned long)device_get_match_data(dev);
    // else
    //     chip_id = i2c_match_id(psu_g1156_id, client)->driver_data;
    chip_id = i2c_match_id(psu_g1156_id, client)->driver_data;
    if (!i2c_check_functionality(client->adapter,
            I2C_FUNC_SMBUS_READ_BYTE_DATA | I2C_FUNC_SMBUS_READ_WORD_DATA | I2C_FUNC_SMBUS_READ_BLOCK_DATA))
        return -ENODEV;

    info = devm_kmemdup(&client->dev, &psu_g1156_info,
                sizeof(struct pmbus_driver_info),
                GFP_KERNEL);
    if (!info)
        return -ENOMEM;

    // status = pmbus_do_probe(client, id, info);
    switch(chip_id)
    {
    case psu1:
        if (s3ip_psu_client[0] == NULL)
        {
            s3ip_psu_client[0] = client;
            if (!s3ip_psu_client[0])
            {
                return -ENODEV;
            }
        }
        break;
    case psu2:
        if (s3ip_psu_client[1] == NULL)
        {
            s3ip_psu_client[1] = client;
            if (!s3ip_psu_client[1])
            {
                return -ENODEV;
            }
        }
        break;
    default:
        return -EINVAL;
        break;
    }
    return status;
}

int psu_g1156_remove(struct i2c_client *client)
{
    int status = 0;
    int temp_psu_index = 0;

    for (temp_psu_index = 0; temp_psu_index < PSU_TOTAL_NUM; temp_psu_index++)
    {
        if (s3ip_psu_client[temp_psu_index] == NULL)
        {
            continue;
        }
        if (s3ip_psu_client[temp_psu_index] == client)
        {
            s3ip_psu_client[temp_psu_index] = NULL;
        }
    }
    // status = pmbus_do_remove(client);
    return status;
}

static struct i2c_driver psu_g1156_driver = {
    .driver = {
           .name = "psu_g1156",
           },
    .probe = psu_g1156_probe,
    .remove = psu_g1156_remove,
    .id_table = psu_g1156_id,
};

module_i2c_driver(psu_g1156_driver);

MODULE_AUTHOR("will_chen@accton.com");
MODULE_DESCRIPTION("PMBus driver for PSU_G1156");
MODULE_VERSION("0.0.1");
MODULE_LICENSE("GPL");
