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
#ifndef __PLATFORM_LIB_H__
#define __PLATFORM_LIB_H__

#include <onlplib/file.h>
#include "x86_64_muxi_dcs6500_48z8c_log.h"

#define AIM_FREE_IF_PTR(p) \
    do \
    { \
        if (p) { \
            aim_free(p); \
            p = NULL; \
        } \
    } while (0)

#define CHASSIS_XSFP_COUNT          56
#define CHASSIS_FAN_COUNT           8
#define CHASSIS_THERMAL_COUNT       4
#define CHASSIS_PSU_THERMAL_COUNT   2
#define CHASSIS_PSU_COUNT           2
#define CHASSIS_LED_COUNT           4

#define PSU1_ID 1
#define PSU2_ID 2

#define FAN_STATUS_NOT_PRESENT  0
#define PSU_STATUS_PRESENT      1
#define PSU_STATUS_NOT_PRESENT  0
#define PSU_STATUS_POWER_GOOD 1

// #define PSU_NODE_MAX_INT_LEN  8
// #define PSU_NODE_MAX_PATH_LEN 64

// #define PSU1_AC_PMBUS_PREFIX "/sys/bus/i2c/devices/17-0059/"
// #define PSU2_AC_PMBUS_PREFIX "/sys/bus/i2c/devices/13-005b/"

// #define PSU1_AC_PMBUS_NODE(node) PSU1_AC_PMBUS_PREFIX#node
// #define PSU2_AC_PMBUS_NODE(node) PSU2_AC_PMBUS_PREFIX#node

// #define PSU1_AC_HWMON_PREFIX "/sys/bus/i2c/devices/17-0051/"
// #define PSU2_AC_HWMON_PREFIX "/sys/bus/i2c/devices/13-0053/"

// #define PSU1_AC_HWMON_NODE(node) PSU1_AC_HWMON_PREFIX#node
// #define PSU2_AC_HWMON_NODE(node) PSU2_AC_HWMON_PREFIX#node

// #define FAN_BOARD_PATH	"/sys/s3ip_switch/fan/"
// #define FAN_NODE(node)	FAN_BOARD_PATH#node
// #define FAN_BOARD_CPLD_BUS              11
// #define FAN_BOARD_CPLD_REG              0x66
// #define FAN_BOARD_CPLD_OFFSET_WDT_ENDIS 0x33
// #define FAN_BOARD_CPLD_OFFSET_WDT_TIMER 0x31
// #define FAN_BOARD_CPLD_WDT_ENABLE       0x1
// #define FAN_BOARD_CPLD_WDT_DISABLE      0x0
// #define FAN_BOARD_CPLD_WDT_MAX_PWM      0xF
#define S3IP_FAN_F2B 0
#define S3IP_FAN_B2F 1


#define IDPROM_PATH_1 "/sys/bus/i2c/devices/0-0050/eeprom"

int onlp_file_write_integer(char *filename, int value);
int onlp_file_read_binary(char *filename, char *buffer, int buf_size, int data_len);
int onlp_file_read_string(char *filename, char *buffer, int buf_size, int data_len);


int psu_pmbus_info_get(int id, char *node, int *value);
int psu_pmbus_info_set(int id, char *node, int value);

typedef enum psu_type {
    PSU_TYPE_DC,
    PSU_TYPE_AC,
    PSU_TYPE_UNKNOWN,
} psu_type_t;

psu_type_t get_psu_type(int id, char* modelname, int modelname_len);
int psu_pmbus_serial_number_get(int id, char *serial, int serial_len);
int psu_acbel_serial_number_get(int id, char *serial, int serial_len);

int s3ip_int_path_get(int id, char *abs_path, char *node, int *value);
int s3ip_int_path_set(int id, char *abs_path, char *node, int value);
int s3ip_str_path_get(int id, char *abs_path, char *node, char *str);

#define DEBUG_MODE 0

#if (DEBUG_MODE == 1)
    #define DEBUG_PRINT(format, ...)   printf(format, __VA_ARGS__)
#else
    #define DEBUG_PRINT(format, ...)
#endif

#endif  /* __PLATFORM_LIB_H__ */

