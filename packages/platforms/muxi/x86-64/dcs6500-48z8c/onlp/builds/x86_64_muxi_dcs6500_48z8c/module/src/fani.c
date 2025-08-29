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
 * Fan Platform Implementation Defaults.
 *
 ***********************************************************/
#include <onlplib/i2c.h>
#include <onlp/platformi/fani.h>
#include "platform_lib.h"

#define MAX_FRONT_FAN_SPEED 30000
#define MAX_REAR_FAN_SPEED 25000
#define MAX_PSU_FAN_SPEED 23000

enum fan_id
{
    FAN_1_ON_FAN_BOARD = 1,
    FAN_2_ON_FAN_BOARD,
    FAN_3_ON_FAN_BOARD,
    FAN_4_ON_FAN_BOARD,
    FAN_5_ON_FAN_BOARD,
    FAN_6_ON_FAN_BOARD,
    FAN_7_ON_FAN_BOARD,
    FAN_8_ON_FAN_BOARD,
    FAN_1_ON_PSU_1,
    FAN_1_ON_PSU_2,
    ONLP_FAN_MAX,
};

#define CHASSIS_FAN_INFO(fid, fid_index, motor_index)                                        \
    {                                                                                        \
        {ONLP_FAN_ID_CREATE(fid), "Chassis Fan-" #fid_index " motor-" #motor_index, 0},      \
        0x0,                                                                                 \
        ONLP_FAN_CAPS_SET_PERCENTAGE | ONLP_FAN_CAPS_GET_RPM | ONLP_FAN_CAPS_GET_PERCENTAGE, \
        0,                                                                                   \
        0,                                                                                   \
        ONLP_FAN_MODE_INVALID,                                                               \
    }

#define PSU_FAN_INFO(pid, fid)                                                               \
    {                                                                                        \
        {ONLP_FAN_ID_CREATE(FAN_##fid##_ON_PSU_##pid), "PSU-" #pid " Fan-" #fid, 0},         \
        0x0,                                                                                 \
        ONLP_FAN_CAPS_SET_PERCENTAGE | ONLP_FAN_CAPS_GET_RPM | ONLP_FAN_CAPS_GET_PERCENTAGE, \
        0,                                                                                   \
        0,                                                                                   \
        ONLP_FAN_MODE_INVALID,                                                               \
    }

/* Static fan information */
onlp_fan_info_t __onlp_fan_info[] = {
    {}, /* Not used */
    CHASSIS_FAN_INFO(1, 1, 1),
    CHASSIS_FAN_INFO(2, 1, 2),
    CHASSIS_FAN_INFO(3, 2, 1),
    CHASSIS_FAN_INFO(4, 2, 2),
    CHASSIS_FAN_INFO(5, 3, 1),
    CHASSIS_FAN_INFO(6, 3, 2),
    CHASSIS_FAN_INFO(7, 4, 1),
    CHASSIS_FAN_INFO(8, 4, 2),
    PSU_FAN_INFO(1, 1),
    PSU_FAN_INFO(2, 1)
};

#define VALIDATE(_id)                     \
    do                                    \
    {                                     \
        if (!ONLP_OID_IS_FAN(_id))        \
        {                                 \
            return ONLP_STATUS_E_INVALID; \
        }                                 \
    } while (0)

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

static int _s3ip_fan_int_node_get(int id, char *node, int *value)
{
    int ret = ONLP_STATUS_OK;
    char path[ONLP_CONFIG_INFO_STR_MAX] = {0};
    sprintf(path, "/sys/s3ip_switch/fan/fan%d/%s", id, node);
    DEBUG_PRINT("Fan(%d), %s path = (%s)", id, node, path);

    if (onlp_file_read_int(value, path) < 0)
    {
        AIM_LOG_ERROR("Unable to read status from file (%s)\r\n", path);
        return ONLP_STATUS_E_INTERNAL;
    }
    return ret;
}

static int _s3ip_fan_motor_int_node_get(int id, int mid, char *node, int *value)
{
    int ret = ONLP_STATUS_OK;
    char path[ONLP_CONFIG_INFO_STR_MAX] = {0};
    sprintf(path, "/sys/s3ip_switch/fan/fan%d/motor%d/%s", id, mid, node);
    DEBUG_PRINT("Fan(%d), %s path = (%s)", id, node, path);

    if (onlp_file_read_int(value, path) < 0)
    {
        AIM_LOG_ERROR("Unable to read status from file (%s)\r\n", path);
        return ONLP_STATUS_E_INTERNAL;
    }
    return ret;
}

static int _s3ip_fan_int_node_set(int id, char *node, int value)
{
    int ret = ONLP_STATUS_OK;
    char path[ONLP_CONFIG_INFO_STR_MAX] = {0};
    sprintf(path, "/sys/s3ip_switch/fan/fan%d/%s", id, node);
    DEBUG_PRINT("Fan(%d), %s path = (%s)", id, node, path);
    if (onlp_file_write_integer(path, value) < 0)
    {
        AIM_LOG_ERROR("Unable to write data to file (%s)\r\n", path);
        return ONLP_STATUS_E_INTERNAL;
    }
    return ret;
}

static int _s3ip_fan_str_node_get(int id, char *node, char *str)
{
    int ret = ONLP_STATUS_OK;
    char *string = NULL;
    int len = 0;
    char path[ONLP_CONFIG_INFO_STR_MAX] = {0};
    sprintf(path, "/sys/s3ip_switch/fan/fan%d/%s", id, node);
    DEBUG_PRINT("Fan(%d), %s path = (%s)", id, node, path);
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

static int _onlp_fani_info_get_fan(int fid, onlp_fan_info_t *info)
{
    int value = 0;
    int ret = ONLP_STATUS_OK;

    /* get fan present status */
    ret = _s3ip_fan_int_node_get((fid+1)/2, "status", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read status from file\r\n");
        return ONLP_STATUS_E_INTERNAL;
    }
    if (value != FAN_STATUS_NOT_PRESENT)
        info->status |= ONLP_FAN_STATUS_PRESENT;
    else {
        info->status &= (~ONLP_FAN_STATUS_PRESENT); // not present
        return ret;
    }

    /* get fan fault status (turn on when any one fails) */
    if (value == 2) {
        info->status |= ONLP_FAN_STATUS_FAILED;
    }

    /* get fan direction (both : the same) */
    ret = _s3ip_fan_int_node_get((fid+1)/2, "direction", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read direction from file\r\n");
        return ONLP_STATUS_E_INTERNAL;
    }
    if (value == S3IP_FAN_F2B)
        info->status |= ONLP_FAN_STATUS_F2B;
    else if (value == S3IP_FAN_B2F)
        info->status |= ONLP_FAN_STATUS_B2F;

    /* get front fan speed */
    ret = _s3ip_fan_motor_int_node_get((fid+1)/2, (2-fid%2), "speed", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read motor1/speed from file\r\n");
        return ONLP_STATUS_E_INTERNAL;
    }
    info->rpm = value;

    /* get speed percentage from rpm */
    if(fid%2 == 0)
        info->percentage = (info->rpm * 100) / MAX_REAR_FAN_SPEED;
    else
        info->percentage = (info->rpm * 100) / MAX_FRONT_FAN_SPEED;

    /* Model (if applicable) */
    ret = _s3ip_fan_str_node_get((fid+1)/2, "model_name", info->model);
    if (ret) {
        AIM_LOG_ERROR("Unable to read model_name from file\r\n");
    }

    /* Serial Number (if applicable) */
    ret = _s3ip_fan_str_node_get((fid+1)/2, "serial_number", info->serial);
    if (ret) {
        AIM_LOG_ERROR("Unable to read serial_number from file\r\n");
    }
    return ONLP_STATUS_OK;
}

static int _onlp_fani_info_get_fan_on_psu(int pid, onlp_fan_info_t *info)
{
    int value = 0;
    int ret = ONLP_STATUS_OK;

    /* get psu fan present */
    ret = _s3ip_psu_int_node_get(pid, "present", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read present from file\r\n");
        return ONLP_STATUS_E_INTERNAL;
    }
    if (value == PSU_STATUS_PRESENT)
        info->status |= ONLP_FAN_STATUS_PRESENT;
    else {
        info->status &= (~ONLP_FAN_STATUS_PRESENT); // not present
        return ret;
    }

    /* get fan direction */
    info->status |= ONLP_FAN_STATUS_B2F;

    /* get psu fan fault status */
    ret = _s3ip_psu_int_node_get(pid, "in_status", &value);//todo add psu fan status node
    if (ret) {
        AIM_LOG_ERROR("Unable to read in_status from file\r\n");
        return ONLP_STATUS_E_INTERNAL;
    }
    if (value != 1) {
        info->status |= ONLP_FAN_STATUS_FAILED;
    }
    ret = _s3ip_psu_int_node_get(pid, "out_status", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read out_status from file\r\n");
        return ONLP_STATUS_E_INTERNAL;
    }
    if (value != 1) {
        info->status |= ONLP_FAN_STATUS_FAILED;
    }

    /* get fan speed */
    ret = _s3ip_psu_int_node_get(pid, "fan_speed", &value);
    if (ret) {
        AIM_LOG_ERROR("Unable to read fan_speed from file\r\n");
        return ONLP_STATUS_E_INTERNAL;
    }
    info->rpm = value;
    info->percentage = (info->rpm * 100) / MAX_PSU_FAN_SPEED;

    /* Model (if applicable) */
    ret = _s3ip_psu_str_node_get(pid, "model_name", info->model);
    if (ret) {
        AIM_LOG_ERROR("Unable to read model_name from file\r\n");
    }
    /* Serial Number (if applicable) */
    ret = _s3ip_psu_str_node_get(pid, "serial_number", info->serial);
    if (ret) {
        AIM_LOG_ERROR("Unable to read serial_number from file\r\n");
    }

    return ONLP_STATUS_OK;
}

/**
 * @brief Initialize the fan platform subsystem.
 */
int onlp_fani_init(void)
{
    return ONLP_STATUS_OK;
}

/**
 * @brief Get the information structure for the given fan OID.
 * @param id The fan OID
 * @param rv [out] Receives the fan information.
 */
int onlp_fani_info_get(onlp_oid_t id, onlp_fan_info_t *info)
{
    int ret = 0;
    int fid;
    VALIDATE(id);

    fid = ONLP_OID_ID_GET(id);
    *info = __onlp_fan_info[fid];

    switch (fid)
    {
    case FAN_1_ON_PSU_1:
        ret = _onlp_fani_info_get_fan_on_psu(PSU1_ID, info);
        break;
    case FAN_1_ON_PSU_2:
        ret = _onlp_fani_info_get_fan_on_psu(PSU2_ID, info);
        break;
    case FAN_1_ON_FAN_BOARD:
    case FAN_2_ON_FAN_BOARD:
    case FAN_3_ON_FAN_BOARD:
    case FAN_4_ON_FAN_BOARD:
    case FAN_5_ON_FAN_BOARD:
    case FAN_6_ON_FAN_BOARD:
    case FAN_7_ON_FAN_BOARD:
    case FAN_8_ON_FAN_BOARD:
        ret = _onlp_fani_info_get_fan(fid, info);
        break;
    default:
        ret = ONLP_STATUS_E_INVALID;
        break;
    }

    return ret;
}

/**
 * @brief Retrieve the fan's operational status.
 * @param id The fan OID.
 * @param rv [out] Receives the fan's operations status flags.
 * @notes Only operational state needs to be returned -
 *        PRESENT/FAILED
 */
int onlp_fani_status_get(onlp_oid_t id, uint32_t* rv)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/**
 * @brief Retrieve the fan's OID hdr.
 * @param id The fan OID.
 * @param rv [out] Receives the OID header.
 */
int onlp_fani_hdr_get(onlp_oid_t id, onlp_oid_hdr_t* hdr)
{
    int ret = ONLP_STATUS_OK;
    onlp_fan_info_t* info;
    int fan_id;
    VALIDATE(id);

    fan_id = ONLP_OID_ID_GET(id);
    if(fan_id > ONLP_FAN_MAX) {
        ret = ONLP_STATUS_E_INVALID;
    } else {
        info = &__onlp_fan_info[fan_id];
        *hdr = info->hdr;
    }
    return ret;
}

/*
 * This function sets the speed of the given fan in RPM.
 *
 * This function will only be called if the fan supprots the RPM_SET
 * capability.
 *
 * It is optional if you have no fans at all with this feature.
 */
int onlp_fani_rpm_set(onlp_oid_t id, int rpm)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/*
 * This function sets the fan speed of the given OID as a percentage.
 *
 * This will only be called if the OID has the PERCENTAGE_SET
 * capability.
 *
 * It is optional if you have no fans at all with this feature.
 */
int onlp_fani_percentage_set(onlp_oid_t id, int p)
{
    int fid;

    VALIDATE(id);
    fid = ONLP_OID_ID_GET(id);

    /* minimum speed 30% */
    if (p < 30) {
        return ONLP_STATUS_E_INVALID;
    }

    switch (fid) {
        case FAN_1_ON_PSU_1:
            return ONLP_STATUS_E_UNSUPPORTED;
        case FAN_1_ON_PSU_2:
            return ONLP_STATUS_E_UNSUPPORTED;
        case FAN_1_ON_FAN_BOARD:
        case FAN_2_ON_FAN_BOARD:
        case FAN_3_ON_FAN_BOARD:
        case FAN_4_ON_FAN_BOARD:
        case FAN_5_ON_FAN_BOARD:
        case FAN_6_ON_FAN_BOARD:
        case FAN_7_ON_FAN_BOARD:
        case FAN_8_ON_FAN_BOARD:
            if (_s3ip_fan_int_node_set((fid+1)/2, "ratio", p) < 0) {
                return ONLP_STATUS_E_INTERNAL;
            }
            break;
        default:
            return ONLP_STATUS_E_INVALID;
    }
    return ONLP_STATUS_OK;
}

/*
 * This function sets the fan speed of the given OID as per
 * the predefined ONLP fan speed modes: off, slow, normal, fast, max.
 *
 * Interpretation of these modes is up to the platform.
 *
 */
int onlp_fani_mode_set(onlp_oid_t id, onlp_fan_mode_t mode)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/*
 * This function sets the fan direction of the given OID.
 *
 * This function is only relevant if the fan OID supports both direction
 * capabilities.
 *
 * This function is optional unless the functionality is available.
 */
int onlp_fani_dir_set(onlp_oid_t id, onlp_fan_dir_t dir)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/*
 * Generic fan ioctl. Optional.
 */
int onlp_fani_ioctl(onlp_oid_t id, va_list vargs)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}
