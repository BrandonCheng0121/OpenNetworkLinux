/************************************************************
 * <bsn.cl fy=2014 v=onl>
 *
 *           Copyright 2014 Big Switch Networks, Inc.
 *           Copyright 2014 Muxi Technology Corporation.
 *
 * Licensed under the Eclipse Public License, Version 1.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *        http://www.eclipse.org/legal/epl-v10.html
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific
 * language governing permissions and limitations under the
 * License.
 *
 * </bsn.cl>
 ************************************************************
 *
 *
 *
 ***********************************************************/
#include <onlp/platformi/psui.h>
#include <onlplib/mmap.h>
//#include <stdio.h>
#include <string.h>
#include "platform_lib.h"

#define VALIDATE(_id)                           \
    do {                                        \
        if(!ONLP_OID_IS_PSU(_id)) {             \
            return ONLP_STATUS_E_INVALID;       \
        }                                       \
    } while(0)


static int _s3ip_psu_int_node_get(int id, char *node, int *value)
{
    int ret = ONLP_STATUS_OK;
    char path[ONLP_CONFIG_INFO_STR_MAX] = {0};
    sprintf(path, "/sys/s3ip_switch/psu/psu%d/%s", id, node);
    DEBUG_PRINT("Psu(%d), %s path = (%s)", id, node, path);

    if (onlp_file_read_int(value, path) < 0)
    {
        AIM_LOG_ERROR("Unable to read status from file (%s)\r\n", path);
        return ONLP_STATUS_E_INTERNAL;
    }
    return ret;
}

static int _s3ip_psu_str_node_get(int id, char *node, char *str)
{
    int ret = ONLP_STATUS_OK;
    char *string = NULL;
    int len = 0;
    char path[ONLP_CONFIG_INFO_STR_MAX] = {0};
    sprintf(path, "/sys/s3ip_switch/psu/psu%d/%s", id, node);
    DEBUG_PRINT("Psu(%d), %s path = (%s)", id, node, path);
    len = onlp_file_read_str(&string, path);
    if (string && len)
    {
        if (len >= (ONLP_CONFIG_INFO_STR_MAX - 1))
        {
            len = ONLP_CONFIG_INFO_STR_MAX - 1;
        }
        memcpy(str, string, len);
        str[len] = '\0';
    }
    else
    {
        AIM_LOG_ERROR("Unable to read %s from file (%s)\r\n", node, path);
        ret = ONLP_STATUS_E_INTERNAL;
    }
    AIM_FREE_IF_PTR(string);
    return ret;
}

static int _psu_detail_info_get(onlp_psu_info_t* info)
{
    int value = 0;
    int index = ONLP_OID_ID_GET(info->hdr.id);
    int ret = ONLP_STATUS_OK;
    char string[ONLP_CONFIG_INFO_STR_MAX] = {0};
    int temp_index = 0;
    if (info->status & ONLP_PSU_STATUS_FAILED) {
        return ONLP_STATUS_OK;
    }

    /* Set the associated oid_table */
    info->hdr.coids[0] = ONLP_FAN_ID_CREATE(index + CHASSIS_FAN_COUNT);
    for(temp_index = 1; temp_index <= CHASSIS_PSU_THERMAL_COUNT; temp_index++)
    {
        info->hdr.coids[temp_index] = ONLP_THERMAL_ID_CREATE((index-1)*CHASSIS_PSU_THERMAL_COUNT + CHASSIS_THERMAL_COUNT + temp_index);
    }

    /* Read voltage, current and power */
    ret = _s3ip_psu_int_node_get(index, "out_vol", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read PSU(%d) node(out_vol)\r\n", index);
        return ONLP_STATUS_E_INTERNAL;
    }
    info->mvout = value;
    info->caps |= ONLP_PSU_CAPS_VOUT;

    ret = _s3ip_psu_int_node_get(index, "in_vol", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read PSU(%d) node(in_vol)\r\n", index);
        return ONLP_STATUS_E_INTERNAL;
    }
    info->mvin = value;
    info->caps |= ONLP_PSU_CAPS_VIN;

    ret = _s3ip_psu_int_node_get(index, "out_curr", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read PSU(%d) node(out_curr)\r\n", index);
        return ONLP_STATUS_E_INTERNAL;
    }
    info->miout = value;
    info->caps |= ONLP_PSU_CAPS_IOUT;

    ret = _s3ip_psu_int_node_get(index, "in_curr", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read PSU(%d) node(in_curr)\r\n", index);
        return ONLP_STATUS_E_INTERNAL;
    }
    info->miin = value;
    info->caps |= ONLP_PSU_CAPS_IIN;

    ret = _s3ip_psu_int_node_get(index, "out_power", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read PSU(%d) node(out_power)\r\n", index);
        return ONLP_STATUS_E_INTERNAL;
    }
    info->mpout = value;
    info->caps |= ONLP_PSU_CAPS_POUT;


    ret = _s3ip_psu_int_node_get(index, "in_power", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read PSU(%d) node(in_power)\r\n", index);
        return ONLP_STATUS_E_INTERNAL;
    }
    info->mpin = value;
    info->caps |= ONLP_PSU_CAPS_PIN;

    ret = _s3ip_psu_str_node_get(index, "serial_number", string);
    if (ret) {
        AIM_LOG_ERROR("Unable to read PSU(%d) node(serial_number)\r\n", index);
        return ONLP_STATUS_E_INTERNAL;
    }
    aim_strlcpy(info->serial, string, ONLP_CONFIG_INFO_STR_MAX);

    return ONLP_STATUS_OK;
}

/**
 * @brief Initialize the PSU subsystem.
 */
int onlp_psui_init(void)
{
    return ONLP_STATUS_OK;
}

/*
 * Get all information about the given PSU oid.
 */
static onlp_psu_info_t pinfo[] =
{
    { }, /* Not used */
    {
        { ONLP_PSU_ID_CREATE(PSU1_ID), "PSU-1", 0 },
    },
    {
        { ONLP_PSU_ID_CREATE(PSU2_ID), "PSU-2", 0 },
    }
};

/**
 * @brief Get the information structure for the given PSU
 * @param id The PSU OID
 * @param rv [out] Receives the PSU information.
 */
int onlp_psui_info_get(onlp_oid_t id, onlp_psu_info_t* info)
{
    int value = 0;
    int ret   = ONLP_STATUS_OK;
    int index = ONLP_OID_ID_GET(id);
    int psu_type;

    VALIDATE(id);

    memset(info, 0, sizeof(onlp_psu_info_t));
    *info = pinfo[index]; /* Set the onlp_oid_hdr_t */

    /* Get the present state */
    ret = _s3ip_psu_int_node_get(index, "present", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read present from file\r\n");
        return ONLP_STATUS_E_INTERNAL;
    }
    if (value == PSU_STATUS_PRESENT)
        info->status |= ONLP_PSU_STATUS_PRESENT;
    else {
        info->status &= (~ONLP_PSU_STATUS_PRESENT); // not present
        return ret;
    }

    /* Get power good status */
    ret = _s3ip_psu_int_node_get(index, "in_status", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read in_status from file\r\n");
        return ONLP_STATUS_E_INTERNAL;
    }
    if(value != PSU_STATUS_POWER_GOOD)
    {
        info->status |= ONLP_PSU_STATUS_FAILED;
    }
    ret = _s3ip_psu_int_node_get(index, "out_status", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read out_status from file\r\n");
        return ONLP_STATUS_E_INTERNAL;
    }
    if(value != PSU_STATUS_POWER_GOOD)
    {
        info->status |= ONLP_PSU_STATUS_FAILED;
    }

    /* Get PSU type */
    ret = _s3ip_psu_int_node_get(index, "type", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read type from file\r\n");
        psu_type = PSU_TYPE_UNKNOWN;
    }
    switch (psu_type) {
        case PSU_TYPE_DC:
            info->caps = ONLP_PSU_CAPS_DC48;
        case PSU_TYPE_AC:
            info->caps = ONLP_PSU_CAPS_AC;
            break;
        case PSU_TYPE_UNKNOWN:  /* User insert a unknown PSU or unplugged.*/
            info->status |= ONLP_PSU_STATUS_UNPLUGGED;
            info->status &= ~ONLP_PSU_STATUS_FAILED;
            ret = ONLP_STATUS_OK;
            break;
        default:
            ret = ONLP_STATUS_E_UNSUPPORTED;
            break;
    }

    ret = _psu_detail_info_get(info);

    return ret;
}

/**
 * @brief Get the PSU's operational status.
 * @param id The PSU OID.
 * @param rv [out] Receives the operational status.
 */
int onlp_psui_status_get(onlp_oid_t id, uint32_t* rv)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/**
 * @brief Get the PSU's oid header.
 * @param id The PSU OID.
 * @param rv [out] Receives the header.
 */
int onlp_psui_hdr_get(onlp_oid_t id, onlp_oid_hdr_t* rv)
{
    int ret = ONLP_STATUS_OK;
    onlp_psu_info_t* info;
    int psu_id;
    VALIDATE(id);

    psu_id = ONLP_OID_ID_GET(id);
    if(psu_id > CHASSIS_PSU_COUNT) {
        ret = ONLP_STATUS_E_INVALID;
    } else {
        info = &pinfo[psu_id];
        *rv = info->hdr;
    }
    return ret;
}

/**
 * @brief Generic PSU ioctl
 * @param id The PSU OID
 * @param vargs The variable argument list for the ioctl call.
 */
int onlp_psui_ioctl(onlp_oid_t pid, va_list vargs)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

