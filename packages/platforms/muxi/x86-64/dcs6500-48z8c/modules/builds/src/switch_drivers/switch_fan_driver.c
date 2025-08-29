#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/version.h>

#include "cpld_lpc_driver.h"
#include "cpld_i2c_driver.h"
#include "switch_fan_driver.h"
#include "switch_at24.h"
#include "fruid_eeprom_parse.h"
#include "sysfs_ipmi.h"

#define DRVNAME "drv_fan_driver"
#define SWITCH_FAN_DRIVER_VERSION "0.0.0.1"


enum ks_led_status {
    KS_LED_DARK,
    KS_LED_GREEN_ON,
    KS_LED_YELLOW_ON,
    KS_LED_RED_ON,
    KS_LED_GREEN_FLASH,
    KS_LED_YELLOW_FLASH,
    KS_LED_RED_FLASH,
    KS_LED_BLUE_ON,
    KS_LED_BLUE_FLASH
};

//reg 0xb0-0xb4 FAN LED Control
#define FAN_LED_SW_GREEN    (0x01)
#define FAN_LED_SW_RED      (0x02)
#define FAN_LED_SW_YELLOW   (0x00)
#define FAN_LED_HW          (0x03)
//reg 0x24 Module Present
#define FAN_INSERTED (0)
#define FAN_NO_INSERTED (1)
//reg 0x0E Fan Module Enable
#define FAN_POWER_DISABLE (0)
#define FAN_POWER_ENABLE (1)

#define DEFAULT_MODEL_NAME      "NA\n"
#define DEFAULT_SN              "NA\n"
#define DEFAULT_VENDOR          "NA\n"
#define DEFAULT_PART_NUMBER     "NA\n"
#define DEFAULT_HW_VERSION      "NA\n"
#define LOW_SPEED_PART_NUMBER     "GFB0412EHS-DB6H"
#define HIGH_SPEED_PART_NUMBER    "DFPK0456B2GY0R5"
#define CPLD_FAN_PLUG_INT_LATCH     0x6E
#define CPLD_FAN01_PLUG_MASK        0x02
#define CPLD_FAN23_PLUG_MASK        0x01
#define FAN_MAX_TOLERANCE 20

unsigned int loglevel = 0x00;
static struct platform_device *drv_fan_device;
unsigned int scan_period = 5;

static uint8_t fan_eeprom[MAX_FAN_NUM][FAN_EEPROM_SIZE];

/* Set to 1 if we've read EEPROM into memory */
static int g_has_been_read[MAX_FAN_NUM] = {0};
struct mutex     update_lock;

// Fan speed supported ratio
enum {
    RATIO_30 = 0,
    RATIO_40,
    RATIO_50,
    RATIO_60,
    RATIO_70,
    RATIO_80,
    RATIO_90,
    RATIO_100,
    MAX_FAN_SPEED_RATIO_NUM,
};
typedef struct st_fan_device_info
{
    char part_number[64];
    unsigned int fan_speed_target_val[MAX_MOTOR_NUM][MAX_FAN_SPEED_RATIO_NUM];
    unsigned int fan_speed_tolerance_val[MAX_MOTOR_NUM][MAX_FAN_SPEED_RATIO_NUM];

}st_fan_device_info_t;

static st_fan_device_info_t fan_device_info[] = {
    { 
        part_number: LOW_SPEED_PART_NUMBER,
        fan_speed_target_val: {
            /* MOTOR0 */
            {
                5100,//5670,
                7500,
                9420,
                11430,
                13350,
                15300,
                17100,
                19110
            },
            /* MOTOR1 */
            {
                4800,//5220,
                6990,
                8730,
                10500,
                12360,
                14100,
                15840,
                17790
            },
        },
        fan_speed_tolerance_val: {
            /* MOTOR0 */
            {
                57*FAN_MAX_TOLERANCE,
                75*FAN_MAX_TOLERANCE,
                95*FAN_MAX_TOLERANCE,
                115*FAN_MAX_TOLERANCE,
                134*FAN_MAX_TOLERANCE,
                153*FAN_MAX_TOLERANCE,
                171*FAN_MAX_TOLERANCE,
                192*FAN_MAX_TOLERANCE
            },
            /* MOTOR1 */
            {
                53*FAN_MAX_TOLERANCE,
                70*FAN_MAX_TOLERANCE,
                88*FAN_MAX_TOLERANCE,
                105*FAN_MAX_TOLERANCE,
                124*FAN_MAX_TOLERANCE,
                141*FAN_MAX_TOLERANCE,
                159*FAN_MAX_TOLERANCE,
                178*FAN_MAX_TOLERANCE
            },
        },
    },
    {
        part_number: HIGH_SPEED_PART_NUMBER,
        fan_speed_target_val: {
            /* MOTOR0 */
            {
                9000,
                12000,
                15000,
                18000,
                21000,
                24000,
                27000,
                30000
            },
            /* MOTOR1 */
            {
                7500,
                10000,
                12500,
                15000,
                17500,
                20000,
                22500,
                25000
            },
        },
        fan_speed_tolerance_val: {
            /* MOTOR0 */
            {
                90*FAN_MAX_TOLERANCE,
                120*FAN_MAX_TOLERANCE,
                150*FAN_MAX_TOLERANCE,
                180*FAN_MAX_TOLERANCE,
                210*FAN_MAX_TOLERANCE,
                240*FAN_MAX_TOLERANCE,
                270*FAN_MAX_TOLERANCE,
                300*FAN_MAX_TOLERANCE
            },
            /* MOTOR1 */
            {
                75*FAN_MAX_TOLERANCE,
                100*FAN_MAX_TOLERANCE,
                125*FAN_MAX_TOLERANCE,
                150*FAN_MAX_TOLERANCE,
                175*FAN_MAX_TOLERANCE,
                200*FAN_MAX_TOLERANCE,
                225*FAN_MAX_TOLERANCE,
                250*FAN_MAX_TOLERANCE
            },
        },
    }
};
#define FAN_DEVICE_INFO_NUMBER sizeof(fan_device_info)/sizeof(fan_device_info[0])
static int fan_device_info_index[MAX_FAN_NUM]={1,1,1,1}; // default HIGH_SPEED fan
const char *delim = "\r\n";
const char *sn_str = "Item=";
const char *part_num_str = "BarCode=";
const char *model_str = "Model=";
const char *vendor_str = "VendorName=";

static int i2c_bus_fan_eeprom[MAX_FAN_NUM] = {6, 6, 6, 6};
static int i2c_addr_fan_eeprom[MAX_FAN_NUM] = {0x51, 0x51, 0x55, 0x55};

static unsigned int fan_speed_reg_offset_table[MAX_MOTOR_NUM][MAX_FAN_NUM] = {
    /* MOTOR0 */
    {
        0x3A, //FAN_MOTOR0_SPEED_CNT1_OFFSET
        0x3B, //FAN_MOTOR0_SPEED_CNT2_OFFSET
        0x3C, //FAN_MOTOR0_SPEED_CNT3_OFFSET
        0x3D, //FAN_MOTOR0_SPEED_CNT4_OFFSET
    },
    /* MOTOR1 */
    {
        0x3E, //FAN_MOTOR1_SPEED_CNT1_OFFSET
        0x3F, //FAN_MOTOR1_SPEED_CNT2_OFFSET
        0x40, //FAN_MOTOR1_SPEED_CNT3_OFFSET
        0x41, //FAN_MOTOR1_SPEED_CNT4_OFFSET
    },
};

static unsigned int fan_pwm_reg_offset_table[MAX_FAN_NUM] = {
    0x36, //FAN_PWM_CTL1_OFFSET
    0x37, //FAN_PWM_CTL2_OFFSET
    0x38, //FAN_PWM_CTL3_OFFSET
    0x39, //FAN_PWM_CTL4_OFFSET
};

static int cpld_write(u8 reg, u8 value)
{
    return lpc_write_cpld( LCP_DEVICE_ADDRESS1, reg, value);
}

static int cpld_read(u8 reg)
{
    return lpc_read_cpld( LCP_DEVICE_ADDRESS1, reg);
}

static int check_fan_plug_int(unsigned int fan_index)
{
    int fan_plug_val = 0;
    int plug_int = 0;
    fan_plug_val = cpld_read(CPLD_FAN_PLUG_INT_LATCH);
    switch (fan_index)
    {
    case 0:
    case 1:
        if(fan_plug_val & CPLD_FAN01_PLUG_MASK)
        {
            cpld_write(CPLD_FAN_PLUG_INT_LATCH, CPLD_FAN01_PLUG_MASK);
            g_has_been_read[0] = 0;
            g_has_been_read[1] = 0;
            plug_int = 1;
        }
        break;
    case 2:
    case 3:
        if(fan_plug_val & CPLD_FAN23_PLUG_MASK)
        {
            cpld_write(CPLD_FAN_PLUG_INT_LATCH, CPLD_FAN23_PLUG_MASK);
            g_has_been_read[2] = 0;
            g_has_been_read[3] = 0;
            plug_int = 1;
        }
        break;
    default:
        break;
    }
    return plug_int;
}

int read_fan_eeprom(unsigned int fan_index, uint8_t *eeprom)
{
    int ret;
    if(check_fan_plug_int(fan_index) == 1)
    {
        FAN_DEBUG("There used to be plugging and unplugging fan_index[%d]\n", fan_index);
    }
    if(g_has_been_read[fan_index])
        return 0;

    FAN_DEBUG("Read EEPROM...\n");

    mutex_lock(&update_lock);

    ret = at24_read_eeprom(i2c_bus_fan_eeprom[fan_index], i2c_addr_fan_eeprom[fan_index], eeprom, 0, FAN_EEPROM_SIZE);
    if(ret < 0)
    {
        FAN_ERR("Read EEPROM failed: %d\n", ret);
        mutex_unlock(&update_lock);
        return -1;
    }

    g_has_been_read[fan_index] = 1;
    mutex_unlock(&update_lock);

    return ret;
}

unsigned int drv_get_number(void)
{
    return MAX_FAN_NUM;
}

bool drv_get_present(unsigned short *bitmap)
{
    *bitmap = ((unsigned short)cpld_read(FAN_CPLD_I2C_PRESENT_REG)&FAN_CPLD_I2C_PRESENT_MASK)>>FAN_CPLD_I2C_PRESENT_OFFSET;

    return true;
}
EXPORT_SYMBOL_GPL(drv_get_present);

bool drv_get_alarm(unsigned int fan_index, int *alarm)
{
    return true;
}
EXPORT_SYMBOL_GPL(drv_get_alarm);

ssize_t drv_get_model_name(unsigned int fan_index, char *buf)
{
    int ret;
    char temp[FAN_EEPROM_SIZE];
    fruid_info_t fruid = {0};
    fruid_info_t *pfruid = &fruid;

    ret = read_fan_eeprom(fan_index, fan_eeprom[fan_index]);
    if (ret < 0)
        return sprintf_s(buf, PAGE_SIZE, DEFAULT_MODEL_NAME);

    memcpy_s(temp, FAN_EEPROM_SIZE, fan_eeprom[fan_index], FAN_EEPROM_SIZE);

    ret = fruid_parse_eeprom((uint8_t *)temp, FAN_EEPROM_SIZE, pfruid);
    if (ret != 0)
    {
        free_fruid_info(pfruid);
        return sprintf_s(buf, PAGE_SIZE, DEFAULT_MODEL_NAME);
    }

    if (pfruid->product.flag)
    {
        ret = sprintf_s(buf, PAGE_SIZE, "%s\n", pfruid->product.name);
        FAN_DEBUG("drv_get_model_name %s \n", pfruid->product.name);
    }
    else
    {
        ret = sprintf_s(buf, PAGE_SIZE, DEFAULT_MODEL_NAME);
    }

    free_fruid_info(pfruid);
    return ret;
}

ssize_t drv_get_sn(unsigned int fan_index, char *buf)
{
    int ret;
    char temp[FAN_EEPROM_SIZE];
    fruid_info_t fruid = {0};
    fruid_info_t *pfruid = &fruid;

    ret = read_fan_eeprom(fan_index, fan_eeprom[fan_index]);
    if (ret < 0)
        return sprintf_s(buf, PAGE_SIZE, DEFAULT_SN);

    memcpy_s(temp, FAN_EEPROM_SIZE, fan_eeprom[fan_index], FAN_EEPROM_SIZE);

    ret = fruid_parse_eeprom((uint8_t *)temp, FAN_EEPROM_SIZE, pfruid);
    if (ret != 0)
    {
        free_fruid_info(pfruid);
        return sprintf_s(buf, PAGE_SIZE, DEFAULT_SN);
    }

    if (pfruid->product.flag)
    {
        ret = sprintf_s(buf, PAGE_SIZE, "%s\n", pfruid->product.serial);
        FAN_DEBUG("drv_get_sn %s \n", pfruid->product.serial);
    }
    else
    {
        ret = sprintf_s(buf, PAGE_SIZE, DEFAULT_SN);
    }

    free_fruid_info(pfruid);
    return ret;
}

ssize_t drv_get_vendor(unsigned int fan_index, char *buf)
{
    int ret;
    char temp[FAN_EEPROM_SIZE];
    fruid_info_t fruid = {0};
    fruid_info_t *pfruid = &fruid;

    ret = read_fan_eeprom(fan_index, fan_eeprom[fan_index]);
    if (ret < 0)
        return sprintf_s(buf, PAGE_SIZE, DEFAULT_VENDOR);

    memcpy_s(temp, FAN_EEPROM_SIZE, fan_eeprom[fan_index], FAN_EEPROM_SIZE);

    ret = fruid_parse_eeprom((uint8_t *)temp, FAN_EEPROM_SIZE, pfruid);
    if (ret != 0)
    {
        free_fruid_info(pfruid);
        return sprintf_s(buf, PAGE_SIZE, DEFAULT_VENDOR);
    }

    if (pfruid->product.flag)
    {
        ret = sprintf_s(buf, PAGE_SIZE, "%s\n", pfruid->product.mfg);
        FAN_DEBUG("drv_get_vendor %s \n", pfruid->product.mfg);
    }
    else
    {
        ret = sprintf_s(buf, PAGE_SIZE, DEFAULT_VENDOR);
    }

    free_fruid_info(pfruid);
    return ret;
}

ssize_t drv_get_part_number(unsigned int fan_index, char *buf)
{
    int ret,i;
    char temp[FAN_EEPROM_SIZE];
    fruid_info_t fruid = {0};
    fruid_info_t *pfruid = &fruid;

    ret = read_fan_eeprom(fan_index, fan_eeprom[fan_index]);
    if (ret < 0)
        return sprintf_s(buf, PAGE_SIZE, DEFAULT_PART_NUMBER);

    memcpy_s(temp, FAN_EEPROM_SIZE, fan_eeprom[fan_index], FAN_EEPROM_SIZE);

    ret = fruid_parse_eeprom((uint8_t *)temp, FAN_EEPROM_SIZE, pfruid);
    if (ret != 0)
    {
        free_fruid_info(pfruid);
        return sprintf_s(buf, PAGE_SIZE, DEFAULT_PART_NUMBER);
    }

    if (pfruid->product.flag)
    {
        ret = sprintf_s(buf, PAGE_SIZE, "%s\n", pfruid->product.part);
        FAN_DEBUG("drv_get_part_number %s \n", pfruid->product.part);
    }
    else
    {
        ret = sprintf_s(buf, PAGE_SIZE, DEFAULT_PART_NUMBER);
    }

    free_fruid_info(pfruid);
    for(i=0;i<FAN_DEVICE_INFO_NUMBER; i++)
    {
        if(!strncmp(buf,fan_device_info[i].part_number,strlen(fan_device_info[i].part_number)))
        {
            fan_device_info_index[fan_index]=i;
            FAN_DEBUG("match part number index: %d\n",fan_device_info_index[fan_index]);
            break;
        }
    }

    return ret;
}

ssize_t drv_get_hw_version(unsigned int fan_index, char *buf)
{
    int ret;
    char temp[FAN_EEPROM_SIZE];
    fruid_info_t fruid = {0};
    fruid_info_t *pfruid = &fruid;

    ret = read_fan_eeprom(fan_index, fan_eeprom[fan_index]);
    if (ret < 0)
        return sprintf_s(buf, PAGE_SIZE, DEFAULT_HW_VERSION);

    memcpy_s(temp, FAN_EEPROM_SIZE, fan_eeprom[fan_index], FAN_EEPROM_SIZE);

    ret = fruid_parse_eeprom((uint8_t *)temp, FAN_EEPROM_SIZE, pfruid);
    if (ret != 0)
    {
        free_fruid_info(pfruid);
        return sprintf_s(buf, PAGE_SIZE, DEFAULT_HW_VERSION);
    }

    if (pfruid->product.flag)
    {
        ret = sprintf_s(buf, PAGE_SIZE, "%s\n", pfruid->product.version);
        FAN_DEBUG("drv_get_hw_version %s \n", pfruid->product.version);
    }
    else
    {
        ret = sprintf_s(buf, PAGE_SIZE, DEFAULT_HW_VERSION);
    }

    free_fruid_info(pfruid);
    return ret;
}

ssize_t drv_get_speed(unsigned int fan_index, unsigned int motor_index, unsigned int *speed)
{
    *speed = FAN_TECK_SPEED_CNT * cpld_read(fan_speed_reg_offset_table[motor_index][fan_index]);
    return 0;
}
EXPORT_SYMBOL_GPL(drv_get_speed);


ssize_t drv_get_speed_target(unsigned int fan_index, unsigned int motor_index, unsigned int *speed_target)
{
    int ret = 0;
    int pwm = 0;

    drv_get_pwm(fan_index, &pwm);
    switch (pwm / 10)
    {
    case 3:
        *speed_target = fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_30] +
                        (fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_40] -
                        fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_30]) * (pwm % 10) / 10;
        break;
    case 4:
        *speed_target = fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_40] +
                        (fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_50] -
                        fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_40]) * (pwm % 10) / 10;
        break;
    case 5:
        *speed_target = fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_50] +
                        (fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_60] -
                        fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_50]) * (pwm % 10) / 10;
        break;
    case 6:
        *speed_target = fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_60] +
                        (fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_70] -
                        fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_60]) * (pwm % 10) / 10;
        break;
    case 7:
        *speed_target = fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_70] +
                        (fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_80] -
                        fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_70]) * (pwm % 10) / 10;
        break;
    case 8:
        *speed_target = fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_80] +
                        (fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_90] -
                        fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_80]) * (pwm % 10) / 10;
        break;
    case 9:
        *speed_target = fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_90] +
                        (fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_100] -
                        fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_90]) * (pwm % 10) / 10;
        break;
    case 10:
        *speed_target = fan_device_info[fan_device_info_index[fan_index]].fan_speed_target_val[motor_index][RATIO_100];
        break;
    default:
        *speed_target = -1;
        break;
    }

    return ret;
}

ssize_t drv_get_speed_tolerance(unsigned int fan_index, unsigned int motor_index, unsigned int *speed_tolerance)
{
    int ret = 0;
    int pwm = 0;

    drv_get_pwm(fan_index, &pwm);
    switch(pwm/10)
    {
        case 3:
            *speed_tolerance = fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_30] +
                               (fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_40] -
                               fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_30]) * (pwm % 10) / 10;
            break;
        case 4:
            *speed_tolerance = fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_40] +
                               (fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_50] -
                               fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_40]) * (pwm % 10) / 10;
            break;
        case 5:
            *speed_tolerance = fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_50] +
                               (fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_60] -
                               fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_50]) * (pwm % 10) / 10;
            break;
        case 6:
            *speed_tolerance = fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_60] +
                               (fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_70] -
                               fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_60]) * (pwm % 10) / 10;
            break;
        case 7:
            *speed_tolerance = fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_70] +
                               (fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_80] -
                               fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_70]) * (pwm % 10) / 10;
            break;
        case 8:
            *speed_tolerance = fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_80] +
                               (fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_90] -
                               fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_80]) * (pwm % 10) / 10;
            break;
        case 9:
            *speed_tolerance = fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_90] +
                               (fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_100] -
                               fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_90]) * (pwm % 10) / 10;
            break;
        case 10:
            *speed_tolerance = fan_device_info[fan_device_info_index[fan_index]].fan_speed_tolerance_val[motor_index][RATIO_100];
            break;
        default:
            *speed_tolerance = -1;
            break;
    }

    return ret;
}

ssize_t drv_get_speed_max(unsigned int fan_index, unsigned int motor_index, unsigned int *speed_max)
{
    unsigned int speed_tolerance = 0;
    unsigned int speed_target = 0;
    drv_get_speed_tolerance(fan_index, motor_index, &speed_tolerance);
    drv_get_speed_target(fan_index, motor_index, &speed_target);
    *speed_max = speed_target + speed_tolerance;
    return 0;
}

ssize_t drv_get_speed_min(unsigned int fan_index, unsigned int motor_index, unsigned int *speed_min)
{
    unsigned int speed_tolerance = 0;
    unsigned int speed_target = 0;
    drv_get_speed_tolerance(fan_index, motor_index, &speed_tolerance);
    drv_get_speed_target(fan_index, motor_index, &speed_target);
    *speed_min = speed_target - speed_tolerance;
    return 0;
}

bool drv_get_pwm(unsigned int fan_index, int *pwm)
{
    int val;
    int reg;

    reg = fan_pwm_reg_offset_table[fan_index];

    val = cpld_read(reg);
    val = (val + 2) * 100 / 256;
    *pwm = val;

    return true;
}
EXPORT_SYMBOL_GPL(drv_get_pwm);

bool drv_set_pwm(unsigned int fan_index, int pwm)
{
    int val;
    int reg;

    reg = fan_pwm_reg_offset_table[fan_index];

    if((pwm>100) || (pwm<0))
    {
        FAN_DEBUG("[%s] Invalid pwm value.\n", __func__);
        return false;
    }

    val = pwm;
    if(val < 30)
        val = 30;

    val = val * 255 / 100;
    cpld_write(reg, val);

    return true;
}
EXPORT_SYMBOL_GPL(drv_set_pwm);

ssize_t drv_get_alarm_threshold(char *buf)
{
    return 0;
}

bool drv_get_wind(unsigned int fan_index, unsigned int *wind)
{

    int val = 0;
    val = cpld_read(FAN_DIRECTION_OFFSET)&0x3;
    if(((val >> (((MAX_FAN_NUM - 1) - fan_index)/2)) & 0x1) == FAN_F2B)
    {
        *wind = S3IP_FAN_F2B;
    }
    else
    {
        *wind = S3IP_FAN_B2F;
    }

    return true;
}

bool drv_get_led_status(unsigned int fan_index, unsigned int *val)
{
    unsigned char reg_ctl = 0;
    unsigned char val_temp = 0;

    if(fan_index < 0 || fan_index > MAX_FAN_NUM)
    {
        FAN_ERR("Invalid fan index.\n");
        return false;
    }

    reg_ctl = cpld_read(FAN_LED_CTL1_OFFSET + fan_index);
    switch (reg_ctl&0x3)
    {
        case FAN_LED_HW:
            switch ((reg_ctl>>2)&0x3)
            {
                case FAN_LED_SW_GREEN:
                    val_temp = KS_LED_GREEN_ON;
                    break;
                case FAN_LED_SW_RED:
                    val_temp = KS_LED_RED_ON;
                    break;
                case FAN_LED_SW_YELLOW:
                    val_temp = KS_LED_YELLOW_ON;
                    break;
                default:
                    val_temp = 0xFF;
                    break;
            }
            break;
        case FAN_LED_SW_GREEN:
            val_temp = KS_LED_GREEN_ON;
            break;
        case FAN_LED_SW_RED:
            val_temp = KS_LED_RED_ON;
            break;
        case FAN_LED_SW_YELLOW:
            val_temp = KS_LED_YELLOW_ON;
            break;
        default:
            val_temp = 0xFF;
            break;
    }
    *val = val_temp;
    return true;
}

ssize_t drv_get_scan_period(char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "present soft scan time is %d x 100ms.\n", scan_period);
}

void drv_set_scan_period(unsigned int period)
{
    scan_period = period;

    return;
}


int g_history_pwm[MAX_FAN_NUM] = {0};
int g_history_fan_status[MAX_FAN_NUM] = {1, 1, 1, 1};
int g_pwm_changing[MAX_FAN_NUM] = {0};
static struct timer_list g_fan_status_timer[MAX_FAN_NUM]; 
#define PWM_IS_CHANGING 1
#define PWM_NOT_CHANGING 0
#define PWM_CHANGING_TIMEOUT 15

#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
void GetIntrTimerCallback(unsigned long fan_index);
void GetIntrTimerCallback(unsigned long fan_index)
{
    g_pwm_changing[fan_index] = PWM_NOT_CHANGING;
    FAN_DEBUG("GetIntrTimerCallback index(%d),g_pwm_changing[data](%d)\n", fan_index, g_pwm_changing[fan_index]);
}
#else
void GetIntrTimerCallback(struct timer_list *timer);
void GetIntrTimerCallback(struct timer_list *timer)
{
    int fan_index = 0;
    for(fan_index = 0; fan_index<MAX_FAN_NUM; fan_index++)
    {
        if(timer == &g_fan_status_timer[fan_index])
        {
            g_pwm_changing[fan_index] = PWM_NOT_CHANGING;
            FAN_DEBUG("GetIntrTimerCallback index(%d),g_pwm_changing[data](%d)\n", fan_index, g_pwm_changing[fan_index]);
            break;
        }
    }
}
#endif

void fan_status_timer_init(unsigned long fan_index)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
    init_timer(&g_fan_status_timer[fan_index]);
    g_fan_status_timer[fan_index].data = fan_index;
    g_fan_status_timer[fan_index].expires = jiffies + PWM_CHANGING_TIMEOUT*HZ;
    g_fan_status_timer[fan_index].function = GetIntrTimerCallback;
    add_timer(&g_fan_status_timer[fan_index]);
#else
    g_fan_status_timer[fan_index].expires = jiffies + PWM_CHANGING_TIMEOUT*HZ;
    timer_setup(&g_fan_status_timer[fan_index], GetIntrTimerCallback, 0);
#endif
}

void fan_status_timer_del(unsigned int fan_index)
{
    del_timer(&g_fan_status_timer[fan_index]);
}

void fan_status_timer_mod(unsigned int fan_index)
{
    mod_timer(&g_fan_status_timer[fan_index], jiffies + PWM_CHANGING_TIMEOUT*HZ);
}

ssize_t drv_get_status(unsigned int fan_index, char *buf)
{
    unsigned short bitmap;
    unsigned int speed0, speed1;
    unsigned int speed_target0, speed_target1;
    unsigned int speed_tolerance0, speed_tolerance1;
    unsigned int retval;
    int fan_status = 0;
    int pwm = 0;

    retval = drv_get_present(&bitmap);
    if(retval == false)
    {
        FAN_ERR("Get fan present status failed.\n");
        return -1;
    }

    retval = drv_get_pwm(fan_index, &pwm);
    if(retval < 0)
    {
        FAN_ERR("Get fan%d motor0 speed failed.\n", fan_index);
        return -1;
    }

    if(g_history_pwm[fan_index] != pwm)
    {
        g_history_pwm[fan_index] = pwm;
        //set timer
        g_pwm_changing[fan_index] = PWM_IS_CHANGING;
        fan_status_timer_mod(fan_index);
        FAN_DEBUG("g_pwm_changing[fan_index] is changing (%d) index(%d)\n", g_pwm_changing[fan_index], fan_index);
    }

    retval = drv_get_speed(fan_index, 0, &speed0);
    if(retval < 0)
    {
        FAN_ERR("Get fan%d motor0 speed failed.\n", fan_index);
        return -1;
    }
    // speed0 = speed0 * 30;

    retval = drv_get_speed(fan_index, 1, &speed1);
    if(retval < 0)
    {
        FAN_ERR("Get fan%d motor1 speed failed.\n", fan_index);
        return -1;
    }
    // speed1 = speed1 * 30;

    retval = drv_get_speed_target(fan_index, 0, &speed_target0);
    if(retval < 0)
    {
        FAN_ERR("Get fan%d motor0 speed target failed.\n", fan_index);
        return -1;
    }

    retval = drv_get_speed_target(fan_index, 1, &speed_target1);
    if(retval < 0)
    {
        FAN_ERR("Get fan%d motor1 speed target failed.\n", fan_index);
        return -1;
    }

    retval = drv_get_speed_tolerance(fan_index, 0, &speed_tolerance0);
    if(retval < 0)
    {
        FAN_DEBUG("Get fan%d motor0 speed tolerance failed.\n", fan_index);
        return -1;
    }

    retval = drv_get_speed_tolerance(fan_index, 1, &speed_tolerance1);
    if(retval < 0)
    {
        FAN_DEBUG("Get fan%d motor1 speed tolerance failed.\n", fan_index);
        return -1;
    }

    if(((bitmap >> (((MAX_FAN_NUM - 1) - fan_index)/2)) & 0x1)) //not present
        return sprintf_s(buf, PAGE_SIZE, "0\n");
    else // present
    {
        if((speed0 > (speed_target0 + speed_tolerance0)) || (speed0 < (speed_target0 - speed_tolerance0)) ||
           (speed1 > (speed_target1 + speed_tolerance1)) || (speed1 < (speed_target1 - speed_tolerance1)))
            fan_status = 2; // not ok
        else
            fan_status = 1; // ok
        if(g_pwm_changing[fan_index] == PWM_IS_CHANGING)
        {
            fan_status = g_history_fan_status[fan_index];
        }
        else
        {
            g_history_fan_status[fan_index] = fan_status;
        }
        return sprintf_s(buf, PAGE_SIZE, "%d\n", fan_status);
    }
}

void drv_get_loglevel(long *lev)
{
    *lev = (long)loglevel;

    return;
}

void drv_set_loglevel(long lev)
{
    loglevel = (unsigned int)lev;

    return;
}

ssize_t drv_debug_help(char *buf)
{
    return sprintf_s(buf, PAGE_SIZE,
        "The test method is as follows:\n"
        "1.Stop the fan speed regulation service \n"
        "    systemctl stop muxi-platform-fan-monitor.service \n"
        "2. Displays current fan speed \n"
        "    cat /sys/switch/fan/fan*/motor*/speed \n"
        "3.Set fan speed 100 percent \n"
        "    echo 100 > /sys/switch/fan/fan1/motor0/ratio \n"
        "    echo 100 > /sys/switch/fan/fan1/motor1/ratio \n"
        "    echo 100 > /sys/switch/fan/fan2/motor0/ratio \n"
        "    echo 100 > /sys/switch/fan/fan2/motor1/ratio \n"
        "    echo 100 > /sys/switch/fan/fan3/motor0/ratio \n"
        "    echo 100 > /sys/switch/fan/fan3/motor1/ratio \n"
        "    echo 100 > /sys/switch/fan/fan4/motor0/ratio \n"
        "    echo 100 > /sys/switch/fan/fan4/motor1/ratio \n"
        "4. Wait 10s for adjust speed \n"
        "5. Displays current fan speed \n"
        "    cat /sys/switch/fan/fan*/motor*/speed \n"
        "6.Set fan speed 50 percent  \n"
        "    echo 50 > /sys/switch/fan/fan1/motor0/ratio \n"
        "    echo 50 > /sys/switch/fan/fan1/motor1/ratio \n"
        "    echo 50 > /sys/switch/fan/fan2/motor0/ratio \n"
        "    echo 50 > /sys/switch/fan/fan2/motor1/ratio \n"
        "    echo 50 > /sys/switch/fan/fan3/motor0/ratio \n"
        "    echo 50 > /sys/switch/fan/fan3/motor1/ratio \n"
        "    echo 50 > /sys/switch/fan/fan4/motor0/ratio \n"
        "    echo 50 > /sys/switch/fan/fan4/motor1/ratio \n"
        "7. Wait 10s for adjust speed \n"
        "8. Displays current fan speed \n"
        "    cat /sys/switch/fan/fan*/motor*/speed \n"
        "9.Set fan speed 30 percent  \n"
        "    echo 30 > /sys/switch/fan/fan1/motor0/ratio \n"
        "    echo 30 > /sys/switch/fan/fan1/motor1/ratio \n"
        "    echo 30 > /sys/switch/fan/fan2/motor0/ratio \n"
        "    echo 30 > /sys/switch/fan/fan2/motor1/ratio \n"
        "    echo 30 > /sys/switch/fan/fan3/motor0/ratio \n"
        "    echo 30 > /sys/switch/fan/fan3/motor1/ratio \n"
        "    echo 30 > /sys/switch/fan/fan4/motor0/ratio \n"
        "    echo 30 > /sys/switch/fan/fan4/motor1/ratio \n"
        "10. Wait 10s for adjust speed \n"
        "11. Displays current fan speed \n"
        "    cat /sys/switch/fan/fan*/motor*/speed \n"
        "12.Start the fan speed regulation service \n"
        "    systemctl start muxi-platform-fan-monitor.service \n");
}

ssize_t drv_debug(const char *buf, int count)
{
    return 0;
}

static struct fan_drivers_t pfunc = {
    .get_scan_period = drv_get_scan_period,
    .set_scan_period = drv_set_scan_period,
    .get_model_name = drv_get_model_name,
    .get_sn = drv_get_sn,
    .get_vendor = drv_get_vendor,
    .get_part_number = drv_get_part_number,
    .get_hw_version = drv_get_hw_version,
    .get_present = drv_get_present,
    .get_speed = drv_get_speed,
    .get_speed_target = drv_get_speed_target,
    .get_speed_tolerance = drv_get_speed_tolerance,
    .get_speed_max = drv_get_speed_max,
    .get_speed_min = drv_get_speed_min,
    .get_pwm = drv_get_pwm,
    .set_pwm = drv_set_pwm,
    .get_wind = drv_get_wind,
    .get_led_status = drv_get_led_status,
    .get_status = drv_get_status,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
    .debug_help = drv_debug_help,
};

static struct fan_drivers_t pfunc_bmc = {
    .get_scan_period = drv_get_scan_period,
    .set_scan_period = drv_set_scan_period,
    .get_model_name = drv_get_model_name_from_bmc,
    .get_sn = drv_get_sn_from_bmc,
    .get_vendor = drv_get_vendor_from_bmc,
    .get_part_number = drv_get_part_number_from_bmc,
    .get_hw_version = drv_get_hw_version_from_bmc,
    .get_present = drv_get_present,
    .get_speed = drv_get_speed,
    .get_speed_target = drv_get_speed_target,
    .get_speed_tolerance = drv_get_speed_tolerance,
    .get_speed_max = drv_get_speed_max,
    .get_speed_min = drv_get_speed_min,
    .get_pwm = drv_get_pwm,
    .set_pwm = drv_set_pwm,
    .get_wind = drv_get_wind,
    .get_led_status = drv_get_led_status,
    .get_status = drv_get_status,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
    .debug_help = drv_debug_help,
};

static int drv_fan_probe(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    if(ipmi_bmc_is_ok())
    {
        s3ip_fan_drivers_register(&pfunc_bmc);
        mxsonic_fan_drivers_register(&pfunc_bmc);
    }
    else
    {
        s3ip_fan_drivers_register(&pfunc);
        mxsonic_fan_drivers_register(&pfunc);
    }
#else
    hisonic_fan_drivers_register(&pfunc);
    kssonic_fan_drivers_register(&pfunc);
#endif

    return 0;
}

static int drv_fan_remove(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    s3ip_fan_drivers_unregister();
    mxsonic_fan_drivers_unregister();
#else
    hisonic_fan_drivers_unregister();
    kssonic_fan_drivers_unregister();
#endif

    return 0;
}

static struct platform_driver drv_fan_driver = {
    .driver = {
        .name   = DRVNAME,
        .owner = THIS_MODULE,
    },
    .probe      = drv_fan_probe,
    .remove     = drv_fan_remove,
};

static int __init drv_fan_init(void)
{
    int err;
    int retval;

    drv_fan_device = platform_device_alloc(DRVNAME, 0);
    if(!drv_fan_device)
    {
        FAN_DEBUG("%s(#%d): platform_device_alloc fail\n", __func__, __LINE__);
        return -ENOMEM;
    }

    retval = platform_device_add(drv_fan_device);
    if(retval)
    {
        err = -retval;
        FAN_DEBUG("%s(#%d): platform_device_add failed\n", __func__, __LINE__);
        goto dev_add_failed;
    }

    retval = platform_driver_register(&drv_fan_driver);
    if(retval)
    {
        err = -retval;
        FAN_DEBUG("%s(#%d): platform_driver_register failed(%d)\n", __func__, __LINE__, err);
        goto dev_reg_failed;
    }
    fan_status_timer_init(0);
    fan_status_timer_init(1);
    fan_status_timer_init(2);
    fan_status_timer_init(3);
    mutex_init(&update_lock);
    return 0;

dev_reg_failed:
    platform_device_unregister(drv_fan_device);
    return err;
dev_add_failed:
    platform_device_put(drv_fan_device);
    return err;
}

static void __exit drv_fan_exit(void)
{
    platform_driver_unregister(&drv_fan_driver);
    platform_device_unregister(drv_fan_device);
    fan_status_timer_del(0);
    fan_status_timer_del(1);
    fan_status_timer_del(2);
    fan_status_timer_del(3);
    mutex_destroy(&update_lock);
    return;
}

MODULE_DESCRIPTION("Huarong Fan Driver");
MODULE_VERSION(SWITCH_FAN_DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(drv_fan_init);
module_exit(drv_fan_exit);
