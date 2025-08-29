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
#include <onlp/platformi/ledi.h>
#include <sys/mman.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <onlplib/mmap.h>
#include <limits.h>
#include "platform_lib.h"

#define S3IP_LED_PATH "/sys/s3ip_switch/sysled/"

#define VALIDATE(_id)                           \
    do {                                        \
        if(!ONLP_OID_IS_LED(_id)) {             \
            return ONLP_STATUS_E_INVALID;       \
        }                                       \
    } while(0)

/* LED related data */
enum led_light_mode {
    LED_MODE_OFF = 0,
    LED_MODE_GREEN,
    LED_MODE_YELLOW,
    LED_MODE_RED,
    LED_MODE_BLUE,
    LED_MODE_GREEN_BLINK,
    LED_MODE_YELLOW_BLINK,
    LED_MODE_RED_BLINK,
    LED_MODE_BLUE_BLINK,
    LED_MODE_AUTO,
    LED_MODE_UNKNOWN
};

enum onlp_led_id{
    RESERVED_LED = 0,
    SYS_LED,
    BMC_LED,
    // FAN_LED,
    // PSU_LED,
    ID_LED,
    ACT_LED,
    // SYS_MANUAL_CTL,
    // BMC_MANUAL_CTL,
    // SYS_HEARDBEAT,
    // BMC_HEARDBEAT,
    LED_NUM
};

// cat /sys/s3ip_switch/sysled/
static char last_path[][20] =  /* must map with onlp_led_id */
{
    "reserved",
    "sys_led_status",
    "bmc_led_status",
    // "fan_led_status",
    // "psu_led_status",
    "id_led_status",
    "act_led_status",
    // "sys_manual",
    // "bmc_manual",
    // "sys_heardbeat",
    // "bmc_heardbeat",
};

typedef struct led_light_mode_map {
    enum led_light_mode driver_led_mode;
    enum onlp_led_mode_e onlp_led_mode;
} led_light_mode_map_t;

led_light_mode_map_t s3ip_to_onlp_map[] = {
    {LED_MODE_OFF,           ONLP_LED_MODE_OFF},
    {LED_MODE_GREEN,         ONLP_LED_MODE_GREEN},
    {LED_MODE_YELLOW,        ONLP_LED_MODE_ORANGE},
    {LED_MODE_RED,           ONLP_LED_MODE_RED},
    {LED_MODE_BLUE,          ONLP_LED_MODE_BLUE},
    {LED_MODE_GREEN_BLINK,   ONLP_LED_MODE_GREEN_BLINKING},
    {LED_MODE_YELLOW_BLINK,  ONLP_LED_MODE_ORANGE_BLINKING},
    {LED_MODE_RED_BLINK,     ONLP_LED_MODE_RED_BLINKING},
    {LED_MODE_BLUE_BLINK,    ONLP_LED_MODE_BLUE_BLINKING},
    {LED_MODE_AUTO,          ONLP_LED_MODE_AUTO}
};

/*
 * Get the information for the given LED OID.
 */
static onlp_led_info_t led_info[] =
{
    { }, /* Not used */
    {
        { ONLP_LED_ID_CREATE(SYS_LED), "Chassis LED 1 (SYS_LED)", 0 },
        ONLP_LED_STATUS_PRESENT,
        ONLP_LED_CAPS_ON_OFF | ONLP_LED_CAPS_GREEN | ONLP_LED_CAPS_RED,
    },
    {
        { ONLP_LED_ID_CREATE(BMC_LED), "Chassis LED 2 (BMC_LED)", 0 },
        ONLP_LED_STATUS_PRESENT,
        ONLP_LED_CAPS_ON_OFF | ONLP_LED_CAPS_GREEN | ONLP_LED_CAPS_RED,
    },
    // {
    //     { ONLP_LED_ID_CREATE(FAN_LED), "Chassis LED 3 (FAN_LED)", 0 },
    //     ONLP_LED_STATUS_PRESENT,
    //     ONLP_LED_CAPS_ON_OFF | ONLP_LED_CAPS_GREEN | ONLP_LED_CAPS_RED,
    // },
    // {
    //     { ONLP_LED_ID_CREATE(PSU_LED), "Chassis LED 4 (PSU_LED)", 0 },
    //     ONLP_LED_STATUS_PRESENT,
    //     ONLP_LED_CAPS_ON_OFF | ONLP_LED_CAPS_GREEN | ONLP_LED_CAPS_RED,
    // },
    {
        { ONLP_LED_ID_CREATE(ID_LED), "Chassis LED 3 (ID_LED)", 0 },
        ONLP_LED_STATUS_PRESENT,
        ONLP_LED_CAPS_ON_OFF | ONLP_LED_CAPS_BLUE,
    },
    {
        { ONLP_LED_ID_CREATE(ACT_LED), "Chassis LED 4 (ACT_LED)", 0 },
        ONLP_LED_STATUS_PRESENT,
        ONLP_LED_CAPS_ON_OFF | ONLP_LED_CAPS_GREEN | ONLP_LED_CAPS_RED,
    },
    // {
    //     { ONLP_LED_ID_CREATE(SYS_MANUAL_CTL), "Chassis LED 6 (SYS_MANUAL_CTL)", 0 },
    //     ONLP_LED_STATUS_PRESENT,
    //     LED_MODE_AUTO,
    // },
    // {
    //     { ONLP_LED_ID_CREATE(BMC_MANUAL_CTL), "Chassis LED 7 (BMC_MANUAL_CTL)", 0 },
    //     ONLP_LED_STATUS_PRESENT,
    //     LED_MODE_AUTO,
    // },
    // {
    //     { ONLP_LED_ID_CREATE(SYS_HEARDBEAT), "Chassis LED 8 (SYS_HEARDBEAT)", 0 },
    //     ONLP_LED_STATUS_PRESENT,
    //     LED_MODE_AUTO,
    // },
    // {
    //     { ONLP_LED_ID_CREATE(BMC_HEARDBEAT), "Chassis LED 9 (BMC_HEARDBEAT)", 0 },
    //     ONLP_LED_STATUS_PRESENT,
    //     LED_MODE_AUTO,
    // },
};

static int s3ip_to_onlp_led_mode(enum led_light_mode driver_led_mode)
{
    int i, nsize = sizeof(s3ip_to_onlp_map)/sizeof(s3ip_to_onlp_map[0]);
    DEBUG_PRINT("driver_led_mode=%d %s(%d)\r\n", driver_led_mode, __FUNCTION__, __LINE__);
    for (i = 0; i < nsize; i++)
    {
        if (driver_led_mode == s3ip_to_onlp_map[i].driver_led_mode)
        {
            return s3ip_to_onlp_map[i].onlp_led_mode;
        }
    }

    return 0;
}

static int onlp_to_s3ip_led_mode(onlp_led_mode_t onlp_led_mode)
{
    int i, nsize = sizeof(s3ip_to_onlp_map)/sizeof(s3ip_to_onlp_map[0]);

    for(i = 0; i < nsize; i++)
    {
        if (onlp_led_mode == s3ip_to_onlp_map[i].onlp_led_mode)
        {
            return s3ip_to_onlp_map[i].driver_led_mode;
        }
    }

    return 0;
}

/**
 * @brief Initialize the LED subsystem.
 */
int onlp_ledi_init(void)
{
    return ONLP_STATUS_OK;
}

/**
 * @brief Get the information for the given LED
 * @param id The LED OID
 * @param info [out] Receives the LED information.
 */
int onlp_ledi_info_get(onlp_oid_t id, onlp_led_info_t* info)
{

    int  led_id;
	char data[2] = {0};
    char fullpath[PATH_MAX] = {0};

    VALIDATE(id);

    led_id = ONLP_OID_ID_GET(id);

    /* get fullpath */
    sprintf(fullpath, "%s%s", S3IP_LED_PATH, last_path[led_id]);

	/* Set the onlp_oid_hdr_t and capabilities */
    *info = led_info[led_id];

    /* get LED mode */
    if (onlp_file_read_string(fullpath, data, sizeof(data), 0) != 0) {
        DEBUG_PRINT("%s(%d)\r\n", __FUNCTION__, __LINE__);
        return ONLP_STATUS_E_INTERNAL;
    }
    DEBUG_PRINT("fullpath=%s data=%s %s(%d)\r\n", fullpath, data, __FUNCTION__, __LINE__);
    info->mode = s3ip_to_onlp_led_mode(atoi(data));
    DEBUG_PRINT("info->mode=%d %s(%d)\r\n", info->mode, __FUNCTION__, __LINE__);
    /* Set the on/off status */
    if (info->mode != ONLP_LED_MODE_OFF) {
        info->status |= ONLP_LED_STATUS_ON;
    }

    return ONLP_STATUS_OK;
}

/**
 * @brief Get the LED operational status.
 * @param id The LED OID
 * @param rv [out] Receives the operational status.
 */
int onlp_ledi_status_get(onlp_oid_t id, uint32_t* rv)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/**
 * @brief Get the LED header.
 * @param id The LED OID
 * @param rv [out] Receives the header.
 */
int onlp_ledi_hdr_get(onlp_oid_t id, onlp_oid_hdr_t* rv)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/**
 * @brief Turn an LED on or off
 * @param id The LED OID
 * @param on_or_off (boolean) on if 1 off if 0
 * @param This function is only relevant if the ONOFF capability is set.
 * @notes See onlp_led_set() for a description of the default behavior.
 */
int onlp_ledi_set(onlp_oid_t id, int on_or_off)
{
    VALIDATE(id);

    if (!on_or_off) {
        return onlp_ledi_mode_set(id, ONLP_LED_MODE_OFF);
    }

    return ONLP_STATUS_E_UNSUPPORTED;
}

/**
 * @brief LED ioctl
 * @param id The LED OID
 * @param vargs The variable argument list for the ioctl call.
 */
int onlp_ledi_ioctl(onlp_oid_t id, va_list vargs)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/**
 * @brief Set the LED mode.
 * @param id The LED OID
 * @param mode The new mode.
 * @notes Only called if the mode is advertised in the LED capabilities.
 */
int onlp_ledi_mode_set(onlp_oid_t id, onlp_led_mode_t mode)
{

    int  led_id;
    char fullpath[PATH_MAX] = {0};

    VALIDATE(id);

    led_id = ONLP_OID_ID_GET(id);
    sprintf(fullpath, "%s%s", S3IP_LED_PATH, last_path[led_id]);

    if (onlp_file_write_integer(fullpath, onlp_to_s3ip_led_mode(mode)) != 0)
    {
        return ONLP_STATUS_E_INTERNAL;
    }

    return ONLP_STATUS_OK;
}

/**
 * @brief Set the LED character.
 * @param id The LED OID
 * @param c The character..
 * @notes Only called if the char capability is set.
 */
int onlp_ledi_char_set(onlp_oid_t id, char c)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}
