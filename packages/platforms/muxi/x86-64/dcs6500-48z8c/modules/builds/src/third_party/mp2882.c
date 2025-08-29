// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Hardware monitoring driver for MPS Multi-phase Digital VR Controllers
 *
 * Copyright (c) 2020 Nvidia Technologies. All rights reserved.
 */

#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include "pmbus.h"
#include "libboundscheck/include/securec.h"
/* Vendor specific registers. */
#define MP2882_MFR_APS_DECAY_ADV    0x56
#define MP2882_MFR_DC_LOOP_CTRL     0x30
#define MP2882_MFR_VR_CONFIG1       0xE1
#define MP2882_MFR_READ_CS1_2       0x73
#define MP2882_MFR_READ_CS3_4       0x74
#define MP2882_MFR_READ_CS5_6       0x75
#define MP2882_MFR_READ_CS7_8       0x76
#define MP2882_MFR_READ_CS9_10      0x77
#define MP2882_MFR_READ_IOUT_PK     0x90
#define MP2882_MFR_READ_POUT_PK     0x91
#define MP2882_MFR_READ_VREF_R1     0xa1
#define MP2882_MFR_READ_VREF_R2     0xa3
#define MP2882_MFR_OVP_TH_SET       0xee
#define MP2882_MFR_UVP_SET          0xe6

#define MP2882_VOUT_FORMAT          BIT(15)
#define MP2882_VID_STEP_SEL_R1      BIT(4)
#define MP2882_IMVP9_EN_R1          BIT(13)
#define MP2882_VID_STEP_SEL_R2      BIT(3)
#define MP2882_IMVP9_EN_R2          BIT(12)
#define MP2882_PRT_THRES_DIV_OV_EN  BIT(14)
#define MP2882_DRMOS_KCS            GENMASK(13, 12)
#define MP2882_PROT_DEV_OV_OFF      10
#define MP2882_PROT_DEV_OV_ON       5
#define MP2882_SENSE_AMPL           BIT(11)
#define MP2882_SENSE_AMPL_UNIT      1
#define MP2882_SENSE_AMPL_HALF      2
#define MP2882_VIN_UV_LIMIT_UNIT    8

#define MP2882_MAX_PHASE_RAIL1      8
#define MP2882_MAX_PHASE_RAIL2      4
#define MP2882_PAGE_NUM             2
#define MP2975_PSC_VOLTAGE_OUT      0x40

struct mp2882_data {
    struct pmbus_driver_info info;
    int vout_scale;
    int vid_step[MP2882_PAGE_NUM];
    int vref[MP2882_PAGE_NUM];
    int vref_off[MP2882_PAGE_NUM];
    int vout_max[MP2882_PAGE_NUM];
    int vout_ov_fixed[MP2882_PAGE_NUM];
    int vout_format[MP2882_PAGE_NUM];
    int curr_sense_gain[MP2882_PAGE_NUM];
};

#define to_mp2882_data(x)  container_of(x, struct mp2882_data, info)

static int mp2882_read_byte_data(struct i2c_client *client, int page, int reg)
{
    int ret;

    //printk(KERN_ALERT "byte reg: 0x%x\n", reg);

    switch (reg) {
    case PMBUS_VOUT_MODE:
        /*
          * Enforce VOUT direct format, since device allows to set the
          * different formats for the different rails. Conversion from
          * VID to direct provided by driver internally, in case it is
          * necessary.
          */
        return MP2975_PSC_VOLTAGE_OUT;

    case PMBUS_STATUS_BYTE:
        ret = pmbus_read_byte_data(client, page, reg);
        break;

    case PMBUS_STATUS_VOUT:
        ret = pmbus_read_byte_data(client, page, reg);
        break;

    case PMBUS_STATUS_IOUT:
        ret = pmbus_read_byte_data(client, page, reg);
        break;

    case PMBUS_STATUS_INPUT:
        ret = pmbus_read_byte_data(client, page, reg);
        break;

    case PMBUS_STATUS_TEMPERATURE:
        ret = pmbus_read_byte_data(client, page, reg);
        break;

    case PMBUS_STATUS_CML:
        ret = pmbus_read_byte_data(client, page, reg);
        break;

    default:
        return -ENODATA;
    }

    return ret;
}

static int
mp2882_read_word_helper(struct i2c_client *client, int page, int phase, u8 reg,
            u16 mask)
{
    int ret = pmbus_read_word_data(client, page, reg);

    return (ret > 0) ? ret & mask : ret;
}

static int mp2882_read_word_data(struct i2c_client *client, int page,
                 int reg)
{
    //const struct pmbus_driver_info *info = pmbus_get_driver_info(client);
    //struct mp2882_data *data = to_mp2882_data(info);
    int phase = 255;
    int ret;

    //printk(KERN_ALERT "word reg: 0x%x\n", reg);

    switch (reg) {
    case PMBUS_POUT_MAX:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(7, 0));
        break;

    // not work, PMBUS_VOUT_OV_FAULT_LIMIT should be removed
    case PMBUS_VOUT_OV_FAULT_LIMIT:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(10, 0));
        break;

    // not work, PMBUS_VOUT_UV_FAULT_LIMIT should be removed
    case PMBUS_VOUT_UV_FAULT_LIMIT:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(10, 0));
        break;

    case PMBUS_IOUT_OC_FAULT_LIMIT:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(7, 0));
        break;

    case PMBUS_IOUT_OC_FAULT_RESPONSE:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(15, 0));
        break;

    case PMBUS_OT_FAULT_LIMIT:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(7, 0));
        break;

    case PMBUS_OT_WARN_LIMIT:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(7, 0));
        break;

    case PMBUS_VIN_OV_FAULT_LIMIT:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(7, 0));
        break;

    case PMBUS_STATUS_WORD:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(15, 0));
        break;

    case 0x83:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(9, 0));
        ret = DIV_ROUND_CLOSEST(ret, 640);
        break;

    case PMBUS_READ_VIN:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(9, 0));
        ret = ret*(3125);
        ret = DIV_ROUND_CLOSEST(ret, 100000);
        break;

    case PMBUS_READ_IIN:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(10, 0));
        break;

    case PMBUS_READ_VOUT:
        if(page==0)
        {
            ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(11, 0));
        }
        else if(page==1)
        {
            ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(10, 0));
        }
        ret = DIV_ROUND_CLOSEST(ret, 512);
        break;

    case PMBUS_READ_IOUT:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(15, 0));
        ret = DIV_ROUND_CLOSEST(ret, 4000);
        break;

    case PMBUS_READ_TEMPERATURE_1:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(7, 0));
        break;

    case PMBUS_READ_POUT:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(10, 0));
        break;

    // not work, PMBUS_HAVE_PIN should be removed
    case PMBUS_READ_PIN:
        ret = mp2882_read_word_helper(client, page, phase, reg,
                          GENMASK(10, 0));
        break;

    case PMBUS_VIRT_READ_POUT_MAX:
        ret = mp2882_read_word_helper(client, page, phase,
                          MP2882_MFR_READ_POUT_PK,
                          GENMASK(12, 0));
        if (ret < 0)
            return ret;

        ret = DIV_ROUND_CLOSEST(ret, 4);
        break;

    case PMBUS_VIRT_READ_IOUT_MAX:
        ret = mp2882_read_word_helper(client, page, phase,
                          MP2882_MFR_READ_IOUT_PK,
                          GENMASK(12, 0));
        if (ret < 0)
            return ret;

        ret = DIV_ROUND_CLOSEST(ret, 4);
        break;

    case PMBUS_UT_WARN_LIMIT:
    case PMBUS_UT_FAULT_LIMIT:
    case PMBUS_VIN_UV_WARN_LIMIT:
    case PMBUS_VIN_UV_FAULT_LIMIT:
    case PMBUS_VOUT_UV_WARN_LIMIT:
    case PMBUS_VOUT_OV_WARN_LIMIT:
    case PMBUS_VIN_OV_WARN_LIMIT:
    case PMBUS_IIN_OC_FAULT_LIMIT:
    case PMBUS_IOUT_OC_LV_FAULT_LIMIT:
    case PMBUS_IIN_OC_WARN_LIMIT:
    case PMBUS_IOUT_OC_WARN_LIMIT:
    case PMBUS_IOUT_UC_FAULT_LIMIT:
    case PMBUS_POUT_OP_FAULT_LIMIT:
    case PMBUS_POUT_OP_WARN_LIMIT:
    case PMBUS_PIN_OP_WARN_LIMIT:
        return -ENXIO;
    default:
        return -ENODATA;
    }

    return ret;
}

static int
mp2882_current_sense_gain_get(struct i2c_client *client,
                  struct mp2882_data *data)
{
    int i, ret;

    /*
     * Obtain DrMOS current sense gain of power stage from the register
     * MP2882_MFR_VR_CONFIG1, bits 13-12. The value is selected as below:
     * 00b - 5µA/A, 01b - 8.5µA/A, 10b - 9.7µA/A, 11b - 10µA/A. Other
     * values are invalid.
     */
    for (i = 0 ; i < data->info.pages; i++) {
        ret = i2c_smbus_write_byte_data(client, PMBUS_PAGE, i);
        if (ret < 0)
            return ret;
        ret = i2c_smbus_read_word_data(client,
                           MP2882_MFR_VR_CONFIG1);
        if (ret < 0)
            return ret;

        switch ((ret & MP2882_DRMOS_KCS) >> 12) {
        case 0:
            data->curr_sense_gain[i] = 50;
            break;
        case 1:
            data->curr_sense_gain[i] = 85;
            break;
        case 2:
            data->curr_sense_gain[i] = 97;
            break;
        default:
            data->curr_sense_gain[i] = 100;
            break;
        }
    }

    return 0;
}

static int
mp2882_vref_get(struct i2c_client *client, struct mp2882_data *data,
        struct pmbus_driver_info *info)
{
    int ret;

    ret = i2c_smbus_write_byte_data(client, PMBUS_PAGE, 3);
    if (ret < 0)
        return ret;

    /* Get voltage reference value for rail 1. */
    ret = i2c_smbus_read_word_data(client, MP2882_MFR_READ_VREF_R1);
    if (ret < 0)
        return ret;

    data->vref[0] = ret * data->vid_step[0];

    /* Get voltage reference value for rail 2, if connected. */
    if (data->info.pages == MP2882_PAGE_NUM) {
        ret = i2c_smbus_read_word_data(client, MP2882_MFR_READ_VREF_R2);
        if (ret < 0)
            return ret;

        data->vref[1] = ret * data->vid_step[1];
    }
    return 0;
}

static int
mp2882_vout_ov_scale_get(struct i2c_client *client, struct mp2882_data *data,
             struct pmbus_driver_info *info)
{
    int thres_dev, sense_ampl, ret;

    ret = i2c_smbus_write_byte_data(client, PMBUS_PAGE, 0);
    if (ret < 0)
        return ret;

    /*
     * Get divider for over- and under-voltage protection thresholds
     * configuration from the Advanced Options of Auto Phase Shedding and
     * decay register.
     */
    ret = i2c_smbus_read_word_data(client, MP2882_MFR_APS_DECAY_ADV);
    if (ret < 0)
        return ret;
    thres_dev = ret & MP2882_PRT_THRES_DIV_OV_EN ? MP2882_PROT_DEV_OV_ON :
                                                   MP2882_PROT_DEV_OV_OFF;

    /* Select the gain of remote sense amplifier. */
    ret = i2c_smbus_read_word_data(client, PMBUS_VOUT_SCALE_LOOP);
    if (ret < 0)
        return ret;
    sense_ampl = ret & MP2882_SENSE_AMPL ? MP2882_SENSE_AMPL_HALF :
                           MP2882_SENSE_AMPL_UNIT;

    data->vout_scale = sense_ampl * thres_dev;

    return 0;
}

static struct pmbus_driver_info mp2882_info = {
    .pages = 2,
    .format[PSC_VOLTAGE_IN] = linear,
    .format[PSC_VOLTAGE_OUT] = direct,
    .format[PSC_TEMPERATURE] = direct,
    .format[PSC_CURRENT_IN] = linear,
    .format[PSC_CURRENT_OUT] = direct,
    .format[PSC_POWER] = direct,
    .m[PSC_TEMPERATURE] = 1,
    .m[PSC_VOLTAGE_OUT] = 1,
    .R[PSC_VOLTAGE_OUT] = 3,
    .m[PSC_CURRENT_OUT] = 1,
    .m[PSC_POWER] = 1,
    .func[0] = PMBUS_HAVE_VIN | PMBUS_HAVE_VOUT | PMBUS_HAVE_STATUS_VOUT |
        PMBUS_HAVE_IOUT | PMBUS_HAVE_STATUS_IOUT |
        PMBUS_HAVE_TEMP | PMBUS_HAVE_STATUS_TEMP | PMBUS_HAVE_STATUS_INPUT,
    .func[1] = PMBUS_HAVE_VIN | PMBUS_HAVE_VOUT | PMBUS_HAVE_STATUS_VOUT |
        PMBUS_HAVE_IOUT | PMBUS_HAVE_STATUS_IOUT |
        PMBUS_HAVE_TEMP | PMBUS_HAVE_STATUS_TEMP | PMBUS_HAVE_STATUS_INPUT,
    .read_byte_data = mp2882_read_byte_data,
    .read_word_data = mp2882_read_word_data,
};

static int mp2882_probe(struct i2c_client *client,
            const struct i2c_device_id *id)
{
    struct pmbus_driver_info *info;
    struct mp2882_data *data;
    int ret;

    data = devm_kzalloc(&client->dev, sizeof(struct mp2882_data),
                GFP_KERNEL);
    if (!data)
        return -ENOMEM;

    memcpy_s(&data->info, sizeof(*info), &mp2882_info, sizeof(*info));
    info = &data->info;

    data->info.pages = MP2882_PAGE_NUM;

    /* Obtain current sense gain of power stage. */
    ret = mp2882_current_sense_gain_get(client, data);
    if (ret)
        return ret;

    /* Obtain voltage reference values. */
    ret = mp2882_vref_get(client, data, info);
    if (ret)
        return ret;

    /* Obtain vout over-voltage scales. */
    ret = mp2882_vout_ov_scale_get(client, data, info);
    if (ret < 0)
        return ret;

    return pmbus_do_probe(client, id, info);
}

static const struct i2c_device_id mp2882_id[] = {
    {"mp2882", 0},
    {}
};

MODULE_DEVICE_TABLE(i2c, mp2882_id);

static const struct of_device_id __maybe_unused mp2882_of_match[] = {
    {.compatible = "mps,mp2882"},
    {}
};
MODULE_DEVICE_TABLE(of, mp2882_of_match);

static struct i2c_driver mp2882_driver = {
    .driver = {
        .name = "mp2882",
        .of_match_table = of_match_ptr(mp2882_of_match),
    },
    .probe = mp2882_probe,
    .remove = pmbus_do_remove,
    .id_table = mp2882_id,
};

module_i2c_driver(mp2882_driver);

MODULE_AUTHOR("Vadim Pasternak <vadimp@nvidia.com>");
MODULE_DESCRIPTION("PMBus driver for MPS MP2882 device");
MODULE_LICENSE("GPL");
