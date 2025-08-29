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
 * Thermal Sensor Platform Implementation.
 *
 ***********************************************************/
//#include <unistd.h>
#include <onlplib/file.h>
#include <onlp/platformi/thermali.h>
#include "platform_lib.h"

#define THERMAL_PATH_FORMAT 	"/sys/s3ip_switch/temp_sensor/temp%d/value"
#define THERMAL_HIGH_PATH_FORMAT 	"/sys/s3ip_switch/temp_sensor/temp%d/max"
#define THERMAL_CRIT_PATH_FORMAT 	"/sys/s3ip_switch/temp_sensor/temp%d/crit"

#define PSU1_THERMAL_PATH_FORMAT "/sys/s3ip_switch/psu/psu1/temp%d/value"
#define PSU1_THERMAL_HIGH_PATH_FORMAT "/sys/s3ip_switch/psu/psu1/temp%d/max"
#define PSU1_THERMAL_CRIT_PATH_FORMAT "/sys/s3ip_switch/psu/psu1/temp%d/crit"

#define PSU2_THERMAL_PATH_FORMAT "/sys/s3ip_switch/psu/psu2/temp%d/value"
#define PSU2_THERMAL_HIGH_PATH_FORMAT "/sys/s3ip_switch/psu/psu1/temp%d/max"
#define PSU2_THERMAL_CRIT_PATH_FORMAT "/sys/s3ip_switch/psu/psu1/temp%d/crit"

#define VALIDATE(_id)                           \
    do {                                        \
        if(!ONLP_OID_IS_THERMAL(_id)) {         \
            return ONLP_STATUS_E_INVALID;       \
        }                                       \
    } while(0)

enum onlp_thermal_id
{
    THERMAL_RESERVED = 0,
    THERMAL_1_ON_BROAD,
    THERMAL_2_ON_BROAD,
    THERMAL_3_ON_BROAD,
    THERMAL_MAC_CORE,
    THERMAL_1_ON_PSU1,
    THERMAL_2_ON_PSU1,
    THERMAL_1_ON_PSU2,
    THERMAL_2_ON_PSU2,
};

/* Static values */
static onlp_thermal_info_t linfo[] = {
    { }, /* Not used */
    { { ONLP_THERMAL_ID_CREATE(THERMAL_1_ON_BROAD), "MAC_AROUND", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, ONLP_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
    { { ONLP_THERMAL_ID_CREATE(THERMAL_2_ON_BROAD), "COME_BOTTOM", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, ONLP_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
    { { ONLP_THERMAL_ID_CREATE(THERMAL_3_ON_BROAD), "AIR_OUTLET", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, ONLP_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
    { { ONLP_THERMAL_ID_CREATE(THERMAL_MAC_CORE), "U3_MAC_CORE", 0},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, ONLP_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
    { { ONLP_THERMAL_ID_CREATE(THERMAL_1_ON_PSU1), "PSU-1 Thermal Sensor-1", ONLP_PSU_ID_CREATE(PSU1_ID)},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, ONLP_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
    { { ONLP_THERMAL_ID_CREATE(THERMAL_2_ON_PSU1), "PSU-1 Thermal Sensor-2", ONLP_PSU_ID_CREATE(PSU1_ID)},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, ONLP_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
    { { ONLP_THERMAL_ID_CREATE(THERMAL_1_ON_PSU2), "PSU-2 Thermal Sensor-1", ONLP_PSU_ID_CREATE(PSU2_ID)},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, ONLP_THERMAL_THRESHOLD_INIT_DEFAULTS
        },
    { { ONLP_THERMAL_ID_CREATE(THERMAL_2_ON_PSU2), "PSU-2 Thermal Sensor-2", ONLP_PSU_ID_CREATE(PSU2_ID)},
            ONLP_THERMAL_STATUS_PRESENT,
            ONLP_THERMAL_CAPS_ALL, 0, ONLP_THERMAL_THRESHOLD_INIT_DEFAULTS
        }
};

/*
 * This will be called to intiialize the thermali subsystem.
 */
int onlp_thermali_init(void)
{
    return ONLP_STATUS_OK;
}

/**
 * @brief Get the information for the given thermal OID.
 * @param id The Thermal OID
 * @param rv [out] Receives the thermal information.
 */
int onlp_thermali_info_get(onlp_oid_t id, onlp_thermal_info_t* info)
{
    int   tid;
    int temp_index = 0;
    char *format = NULL;
    char *format_high = NULL;
    char *format_crit = NULL;
    char  path[ONLP_CONFIG_INFO_STR_MAX] = {0};
    VALIDATE(id);

    tid = ONLP_OID_ID_GET(id);

    /* Set the onlp_oid_hdr_t and capabilities */
    *info = linfo[tid];
    switch (tid) {
        case THERMAL_1_ON_BROAD:
        case THERMAL_2_ON_BROAD:
        case THERMAL_3_ON_BROAD:
        case THERMAL_MAC_CORE:
            temp_index = tid;
            format = THERMAL_PATH_FORMAT;
            format_high = THERMAL_HIGH_PATH_FORMAT;
            format_crit = THERMAL_CRIT_PATH_FORMAT;
            break;
        case THERMAL_1_ON_PSU1:
        case THERMAL_2_ON_PSU1:
            temp_index = tid - CHASSIS_THERMAL_COUNT;
            format = PSU1_THERMAL_PATH_FORMAT;
            format_high = PSU1_THERMAL_HIGH_PATH_FORMAT;
            format_crit = PSU1_THERMAL_CRIT_PATH_FORMAT;
            break;
        case THERMAL_1_ON_PSU2:
        case THERMAL_2_ON_PSU2:
            temp_index = tid - CHASSIS_THERMAL_COUNT - CHASSIS_PSU_THERMAL_COUNT;
            format = PSU2_THERMAL_PATH_FORMAT;
            format_high = PSU2_THERMAL_HIGH_PATH_FORMAT;
            format_crit = PSU2_THERMAL_CRIT_PATH_FORMAT;
            break;
        default:
            return ONLP_STATUS_E_INVALID;
    };

    /* get path */
    sprintf(path, format, temp_index);
    if (onlp_file_read_int(&info->mcelsius, path) < 0) {
        AIM_LOG_ERROR("Unable to read data from file (%s)\r\n", path);
        return ONLP_STATUS_E_INTERNAL;
    }
    sprintf(path, format_high, temp_index);
    if (onlp_file_read_int(&info->thresholds.warning, path) < 0) {
        AIM_LOG_ERROR("Unable to read data from file (%s)\r\n", path);
        return ONLP_STATUS_E_INTERNAL;
    }
    sprintf(path, format_crit, temp_index);
    if (onlp_file_read_int(&info->thresholds.error, path) < 0) {
        AIM_LOG_ERROR("Unable to read data from file (%s)\r\n", path);
        return ONLP_STATUS_E_INTERNAL;
    }
    sprintf(path, format_crit, temp_index);
    if (onlp_file_read_int(&info->thresholds.shutdown, path) < 0) {
        AIM_LOG_ERROR("Unable to read data from file (%s)\r\n", path);
        return ONLP_STATUS_E_INTERNAL;
    }
    return ONLP_STATUS_OK;
}


