#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/i2c.h>

#include "cpld_lpc_driver.h"
#include "switch_sensor_driver.h"
#include "switch_lm75.h"
#include "pmbus.h"
#include "sysfs_ipmi.h"

#define DRVNAME "drv_sensor_driver"
#define SWITCH_SENSOR_DRIVER_VERSION "0.0.1"

#define VIN1_INPUT   "in1_input"
#define VIN2_INPUT   "in2_input"

#define LM25066_I2C_ADDR        0x40
#define MP2976_I2C_ADDR         0x5f
#define MP2973_I2C_ADDR         0x20
#define MAX34461_I2C_ADDR       0x74

#define ipmitool_raw_get_in_input       1
#define ipmitool_raw_get_curr_input     2

unsigned int loglevel = 0;
static struct platform_device *drv_sensor_device;
typedef struct TEMP_SENSOR
{
    int i2c_bus;
    int i2c_addr;
    int max;
    int max_hyst;
    int min;
    char *alias;
    char *type;
}TempSensor;
static TempSensor temp_sensor[TEMP_TOTAL_NUM] = 
{
    {.i2c_bus = 7, .i2c_addr = 0x4c, .max =  70000, .max_hyst =  68000, .min = -5000, .alias =    "MAC Around", .type = "lm75"},
    {.i2c_bus = 7, .i2c_addr = 0x4b, .max =  70000, .max_hyst =  68000, .min = -5000, .alias =   "COMe bottom", .type = "lm75"},
    {.i2c_bus = 7, .i2c_addr = 0x4a, .max =  70000, .max_hyst =  68000, .min = -5000, .alias =    "Air Outlet", .type = "lm75"},
    {.i2c_bus = 7, .i2c_addr = 0x4c, .max = 105000, .max_hyst = 100000, .min = -5000, .alias = "TEMP_LSW_CORE", .type = "core"},
};

typedef struct VOL_SENSOR
{
    int i2c_bus;
    int i2c_addr;
    int max;
    int min;
    int nominal_value;
    char *attr;
    char *alias;
    char *type;
}VolSensor;
static VolSensor vol_sensor[IN_TOTAL_NUM] = 
{
    {.i2c_bus = 4, .i2c_addr = LM25066_I2C_ADDR,  .max = 12600, .min = 11400, .nominal_value = 12000, .attr =  "in1_input", .alias =    "LM25066_VIN", .type =  "lm25066"},
    {.i2c_bus = 4, .i2c_addr = LM25066_I2C_ADDR,  .max = 12600, .min = 11400, .nominal_value = 12000, .attr =  "in3_input", .alias =   "LM25066_VOUT", .type =  "lm25066"},
    {.i2c_bus = 4, .i2c_addr = MP2976_I2C_ADDR,   .max = 12600, .min = 11400, .nominal_value = 12000, .attr =  "in1_input", .alias =     "MP2976_VIN", .type =   "mp2976"},
    {.i2c_bus = 4, .i2c_addr = MP2976_I2C_ADDR,   .max =  1019, .min =   922, .nominal_value =   971, .attr =  "in2_input", .alias =   "MP2976_VOUT1", .type =   "mp2976"},
    {.i2c_bus = 4, .i2c_addr = MP2976_I2C_ADDR,   .max =  1019, .min =   922, .nominal_value =   971, .attr =  "in3_input", .alias =   "MP2976_VOUT2", .type =   "mp2976"},
    {.i2c_bus = 4, .i2c_addr = MP2973_I2C_ADDR,   .max = 12600, .min = 11400, .nominal_value = 12000, .attr =  "in1_input", .alias =     "MP2973_VIN", .type =   "mp2973"},
    {.i2c_bus = 4, .i2c_addr = MP2973_I2C_ADDR,   .max =  1000, .min =   760, .nominal_value =   890, .attr =  "in2_input", .alias =   "MP2973_VOUT1", .type =   "mp2973"},
    {.i2c_bus = 4, .i2c_addr = MP2973_I2C_ADDR,   .max =   824, .min =   776, .nominal_value =   800, .attr =  "in3_input", .alias =   "MP2973_VOUT2", .type =   "mp2973"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1890, .min =  1710, .nominal_value =  1800, .attr =  "in1_input", .alias =  "MAX34461_VIN1", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =   840, .min =   760, .nominal_value =   800, .attr =  "in2_input", .alias =  "MAX34461_VIN2", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1020, .min =   780, .nominal_value =   890, .attr =  "in3_input", .alias =  "MAX34461_VIN3", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1890, .min =  1710, .nominal_value =  1800, .attr =  "in4_input", .alias =  "MAX34461_VIN4", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1890, .min =  1710, .nominal_value =  1800, .attr =  "in5_input", .alias =  "MAX34461_VIN5", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1890, .min =  1710, .nominal_value =  1800, .attr =  "in6_input", .alias =  "MAX34461_VIN6", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1260, .min =  1140, .nominal_value =  1200, .attr =  "in7_input", .alias =  "MAX34461_VIN7", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1890, .min =  1710, .nominal_value =  1800, .attr =  "in8_input", .alias =  "MAX34461_VIN8", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1890, .min =  1710, .nominal_value =  1800, .attr =  "in9_input", .alias =  "MAX34461_VIN9", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1970, .min =  1612, .nominal_value =  1791, .attr = "in10_input", .alias = "MAX34461_VIN10", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1970, .min =  1612, .nominal_value =  1791, .attr = "in11_input", .alias = "MAX34461_VIN11", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1890, .min =  1710, .nominal_value =  1800, .attr = "in12_input", .alias = "MAX34461_VIN12", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1050, .min =   950, .nominal_value =  1000, .attr = "in13_input", .alias = "MAX34461_VIN13", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1890, .min =  1710, .nominal_value =  1800, .attr = "in14_input", .alias = "MAX34461_VIN14", .type = "max34461"},
    {.i2c_bus = 8, .i2c_addr = MAX34461_I2C_ADDR, .max =  1800, .min =  1147, .nominal_value =  1207, .attr = "in15_input", .alias = "MAX34461_VIN15", .type = "max34461"},
};

typedef struct CURR_SENSOR
{
    int i2c_bus;
    int i2c_addr;
    int max;
    int min;
    char *attr;
    char *alias;
    char *type;
}CurrSensor;
static CurrSensor curr_sensor[CURR_TOTAL_NUM] = 
{
    {.i2c_bus = 4, .i2c_addr = LM25066_I2C_ADDR, .max =  75000, .min = 0, .attr = "curr1_input", .alias =  "LM255066_IIN", .type = "lm255066"},
    {.i2c_bus = 4, .i2c_addr = MP2976_I2C_ADDR,  .max =  18000, .min = 0, .attr = "curr1_input", .alias =    "MP2976_IIN", .type =   "mp2976"},
    {.i2c_bus = 4, .i2c_addr = MP2976_I2C_ADDR,  .max =  32000, .min = 0, .attr = "curr2_input", .alias =  "MP2976_IOUT1", .type =   "mp2976"},
    {.i2c_bus = 4, .i2c_addr = MP2976_I2C_ADDR,  .max =  32000, .min = 0, .attr = "curr3_input", .alias =  "MP2976_IOUT2", .type =   "mp2976"},
    {.i2c_bus = 4, .i2c_addr = MP2973_I2C_ADDR,  .max =  39000, .min = 0, .attr = "curr1_input", .alias =    "MP2973_IIN", .type =   "mp2973"},
    {.i2c_bus = 4, .i2c_addr = MP2973_I2C_ADDR,  .max = 250000, .min = 0, .attr = "curr2_input", .alias =  "MP2973_IOUT1", .type =   "mp2973"},
    {.i2c_bus = 4, .i2c_addr = MP2973_I2C_ADDR,  .max =  55000, .min = 0, .attr = "curr3_input", .alias =  "MP2973_IOUT2", .type =   "mp2973"},
};

ssize_t drv_get_temp_alias(unsigned int index, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%s\n", temp_sensor[index].alias);
}

ssize_t drv_get_temp_type(unsigned int index, char *buf)
{
     return sprintf_s(buf, PAGE_SIZE, "%s\n", temp_sensor[index].type);
}

bool drv_get_temp_max(unsigned int index, long *val)
{
    *val = temp_sensor[index].max;

    return true;
}

bool drv_set_temp_max(unsigned int index, long val)
{
    temp_sensor[index].max = val*1000;

    return true;
}

bool drv_get_temp_max_hyst(unsigned int index, long *val)
{
    *val = temp_sensor[index].max_hyst;

    return true;
}

bool drv_set_temp_max_hyst(unsigned int index, long val)
{
    temp_sensor[index].max_hyst = val*1000;

    return true;
}

bool drv_get_temp_min(unsigned int index, long *val)
{
    *val = temp_sensor[index].min;

    return true;
}

bool drv_set_temp_min(unsigned int index, long val)
{
    temp_sensor[index].min = val*1000;

    return true;
}

bool drv_get_temp_input(unsigned int index, long *temp_input)
{
    int retval = -1;
    int i2c_bus = temp_sensor[index].i2c_bus;
    int i2c_addr = temp_sensor[index].i2c_addr;

    if(index < TOTAL_LM75_TEMP) //lm75
    {
        retval = lm75_read_temp_input(i2c_bus, i2c_addr, temp_input);
        if(retval < 0)
        {
            SENSOR_DEBUG("Get temp%d input failed.\n", index);
            return false;
        }
    }
    else //lsw core
    {
        /* fitting by 1.25*LSW_LM75+10 */
        retval = lm75_read_temp_input(i2c_bus, i2c_addr, temp_input);
        *temp_input = ((*temp_input*1250)/1000)+10000;
        if(retval < 0)
        {
            SENSOR_DEBUG("Get temp%d input failed.\n", index);
            return false;
        }
    }

    return true;
}

bool drv_get_temp_input_from_bmc(unsigned int index, long *temp_input)
{
    int retval;

    if(index < TOTAL_LM75_TEMP) //lm75
    {
        retval = drv_get_sensor_temp_input_from_bmc(index, temp_input);
        if(retval < 0)
        {
            SENSOR_DEBUG("Get temp%d input failed.\n", index);
            return false;
        }
    }
    else //lsw core
    {
        /* fitting by 1.25*LSW_LM75+10 */
        retval = drv_get_sensor_temp_input_from_bmc(TOTAL_LM75_TEMP-1, temp_input);
        *temp_input = ((*temp_input*1250)/1000)+10000;
        if(retval < 0)
        {
            SENSOR_DEBUG("Get temp%d input failed.\n", index);
            return false;
        }
    }

    return true;
}

ssize_t drv_get_temp_status(unsigned int index, char *buf)
{
    long temp_input;
    long temp_max;
    long temp_min;
    long temp_max_hyst;
    unsigned int retval;

    retval = drv_get_temp_input(index, &temp_input);
    if(retval == false)
    {
        SENSOR_ERR("Get temp%d input failed.\n", index);
        return -1;
    }

    retval = drv_get_temp_max(index, &temp_max);
    if(retval == false)
    {
        SENSOR_ERR("Get temp%d max failed.\n", index);
        return -1;
    }

    retval = drv_get_temp_min(index, &temp_min);
    if(retval == false)
    {
        SENSOR_ERR("Get temp%d min failed.\n", index);
        return -1;
    }

    retval = drv_get_temp_max_hyst(index, &temp_max_hyst);
    if(retval == false)
    {
        SENSOR_ERR("Get temp%d max_hyst failed.\n", index);
        return -1;
    }

    if(temp_input < temp_min)
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "3"); // exceed min
    else if(temp_input > temp_max)
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "2"); // exceed fatal
    else if(temp_input > temp_max_hyst)
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "1"); // exceed major
    else
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "0"); // ok
}


ssize_t drv_get_temp_status_from_bmc(unsigned int index, char *buf)
{
    long temp_input;
    long temp_max;
    long temp_min;
    long temp_max_hyst;
    unsigned int retval;

    retval = drv_get_temp_input_from_bmc(index, &temp_input);
    if(retval == false)
    {
        SENSOR_ERR("Get temp%d input failed.\n", index);
        return -1;
    }

    retval = drv_get_temp_max(index, &temp_max);
    if(retval == false)
    {
        SENSOR_ERR("Get temp%d max failed.\n", index);
        return -1;
    }

    retval = drv_get_temp_min(index, &temp_min);
    if(retval == false)
    {
        SENSOR_ERR("Get temp%d min failed.\n", index);
        return -1;
    }

    retval = drv_get_temp_max_hyst(index, &temp_max_hyst);
    if(retval == false)
    {
        SENSOR_ERR("Get temp%d max_hyst failed.\n", index);
        return -1;
    }

    if(temp_input < temp_min)
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "3"); // exceed min
    else if(temp_input > temp_max)
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "2"); // exceed fatal
    else if(temp_input > temp_max_hyst)
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "1"); // exceed major
    else
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "0"); // ok
}

ssize_t drv_get_in_alias(unsigned int index, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%s\n", vol_sensor[index].alias);
}

ssize_t drv_get_in_type(unsigned int index, char *buf)
{
     return sprintf_s(buf, PAGE_SIZE, "%s\n", vol_sensor[index].type);
}

bool drv_get_in_max(unsigned int index, long *val)
{
    *val = vol_sensor[index].max;

    return true;
}

bool drv_get_in_min(unsigned int index, long *val)
{
    *val = vol_sensor[index].min;

    return true;
}

bool drv_get_in_input(unsigned int index, long *in_input)
{
    int retval;
    int i2c_bus = vol_sensor[index].i2c_bus;
    int i2c_addr = vol_sensor[index].i2c_addr;
    char *attr = vol_sensor[index].attr;
    retval = pmbus_core_read_attrs(i2c_bus, i2c_addr, attr, in_input);
    if(retval < 0)
    {
        SENSOR_DEBUG("Get in%d input failed.\n", index);
        return false;
    }

    return true;
}

bool drv_get_in_input_from_bmc(unsigned int index, long *in_input)
{
    int retval;

    retval = drv_get_sensor_in_input_from_bmc(index, ipmitool_raw_get_in_input, in_input);

    if(retval < 0)
    {
        SENSOR_DEBUG("Get in%d input failed.\n", index);
        return false;
    }

    return true;
}

ssize_t drv_get_in_status(unsigned int index, char *buf)
{
    long in_input;
    long in_max;
    long in_min;
    unsigned int retval;

    retval = drv_get_in_input(index, &in_input);
    if(retval == false)
    {
        SENSOR_ERR("Get in%d input failed.\n", index);
        return -1;
    }

    retval = drv_get_in_max(index, &in_max);
    if(retval == false)
    {
        SENSOR_ERR("Get in%d max failed.\n", index);
        return -1;
    }

    retval = drv_get_in_min(index, &in_min);
    if(retval == false)
    {
        SENSOR_ERR("Get in%d min failed.\n", index);
        return -1;
    }

    if(in_input < in_min)
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "2"); // exceed min
    else if(in_input > in_max)
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "1"); // exceed fatal
    else
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "0"); // ok
}

ssize_t drv_get_in_status_from_bmc(unsigned int index, char *buf)
{
    long in_input;
    long in_max;
    long in_min;
    unsigned int retval;

    retval = drv_get_in_input_from_bmc(index, &in_input);
    if(retval == false)
    {
        SENSOR_ERR("Get in%d input failed.\n", index);
        return -1;
    }

    retval = drv_get_in_max(index, &in_max);
    if(retval == false)
    {
        SENSOR_ERR("Get in%d max failed.\n", index);
        return -1;
    }

    retval = drv_get_in_min(index, &in_min);
    if(retval == false)
    {
        SENSOR_ERR("Get in%d min failed.\n", index);
        return -1;
    }

    if(in_input < in_min)
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "2"); // exceed min
    else if(in_input > in_max)
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "1"); // exceed fatal
    else
        return sprintf_s(buf, PAGE_SIZE, "%s\n", "0"); // ok
}

bool drv_get_in_alarm(unsigned int index, long *val)
{
    *val = vol_sensor[index].nominal_value;

    return true;
}

ssize_t drv_get_curr_alias(unsigned int index, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%s\n", curr_sensor[index].alias);
}

ssize_t drv_get_curr_type(unsigned int index, char *buf)
{
     return sprintf_s(buf, PAGE_SIZE, "%s\n", curr_sensor[index].type);
}

bool drv_get_curr_max(unsigned int index, long *val)
{
    *val = curr_sensor[index].max;

    return true;
}

bool drv_get_curr_min(unsigned int index, long *val)
{
    *val = curr_sensor[index].min;

    return true;
}

bool drv_get_curr_input(unsigned int index, long *curr_input)
{
    int retval;
    int i2c_bus = curr_sensor[index].i2c_bus;
    int i2c_addr = curr_sensor[index].i2c_addr;
    char *attr = curr_sensor[index].attr;
    retval = pmbus_core_read_attrs(i2c_bus, i2c_addr, attr, curr_input);
    if(retval < 0)
    {
        SENSOR_DEBUG("Get curr%d input failed.\n", index);
        return false;
    }

    return true;
}

bool drv_get_curr_input_from_bmc(unsigned int index, long *curr_input)
{
    int retval;

    retval = drv_get_sensor_curr_input_from_bmc(index, ipmitool_raw_get_curr_input, curr_input);

    if(retval < 0)
    {
        SENSOR_DEBUG("Get curr%d input failed.\n", index);
        return false;
    }

    return true;
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
        "sysname                debug cmd\n"
        "1 temp                 cat /sys/bus/i2c/devices/7-004c/hwmon/hwmon*/temp1_input\n"
        "2 temp                 cat /sys/bus/i2c/devices/7-004b/hwmon/hwmon*/temp1_input\n"
        "3 temp                 cat /sys/bus/i2c/devices/7-004a/hwmon/hwmon*/temp1_input\n"
        "lm25066                cat /sys/bus/i2c/devices/4-0040/hwmon/hwmon*/in*input\n"
        "                       cat /sys/bus/i2c/devices/4-0040/hwmon/hwmon*/curr*input\n"
        "                       cat /sys/bus/i2c/devices/4-0040/hwmon/hwmon*/temp*input\n"
        "mp2976                 cat /sys/bus/i2c/devices/4-005f/hwmon/hwmon*/in*input\n"
        "mp2973                 cat /sys/bus/i2c/devices/4-0020/hwmon/hwmon*/in*input\n"
        "max34461               cat /sys/bus/i2c/devices/8-0074/hwmon/hwmon*/in*input\n");
}

ssize_t drv_debug(const char *buf, int count)
{
    return 0;
}

static struct sensor_drivers_t pfunc_bmc = {
    .get_temp_status = drv_get_temp_status_from_bmc,
    .get_temp_alias = drv_get_temp_alias,
    .get_temp_type = drv_get_temp_type,
    .get_temp_max = drv_get_temp_max,
    .get_temp_max_hyst = drv_get_temp_max_hyst,
    .get_temp_min = drv_get_temp_min,
    .set_temp_max = drv_set_temp_max,
    .set_temp_max_hyst = drv_set_temp_max_hyst,
    .set_temp_min = drv_set_temp_min,
    .get_temp_input = drv_get_temp_input_from_bmc,
    .get_in_alias = drv_get_in_alias,
    .get_in_type = drv_get_in_type,
    .get_in_max = drv_get_in_max,
    .get_in_min = drv_get_in_min,
    .get_in_status = drv_get_in_status_from_bmc,
    .get_in_input = drv_get_in_input_from_bmc,
    .get_in_alarm = drv_get_in_alarm,
    .get_curr_alias = drv_get_curr_alias,
    .get_curr_type = drv_get_curr_type,
    .get_curr_max = drv_get_curr_max,
    .get_curr_min = drv_get_curr_min,
    .get_curr_input = drv_get_curr_input_from_bmc,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
    .debug_help = drv_debug_help,
};

static struct sensor_drivers_t pfunc = {
    .get_temp_status = drv_get_temp_status,
    .get_temp_alias = drv_get_temp_alias,
    .get_temp_type = drv_get_temp_type,
    .get_temp_max = drv_get_temp_max,
    .get_temp_max_hyst = drv_get_temp_max_hyst,
    .get_temp_min = drv_get_temp_min,
    .set_temp_max = drv_set_temp_max,
    .set_temp_max_hyst = drv_set_temp_max_hyst,
    .set_temp_min = drv_set_temp_min,
    .get_temp_input = drv_get_temp_input,
    .get_in_alias = drv_get_in_alias,
    .get_in_type = drv_get_in_type,
    .get_in_max = drv_get_in_max,
    .get_in_min = drv_get_in_min,
    .get_in_status = drv_get_in_status,
    .get_in_input = drv_get_in_input,
    .get_in_alarm = drv_get_in_alarm,
    .get_curr_alias = drv_get_curr_alias,
    .get_curr_type = drv_get_curr_type,
    .get_curr_max = drv_get_curr_max,
    .get_curr_min = drv_get_curr_min,
    .get_curr_input = drv_get_curr_input,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
    .debug_help = drv_debug_help,
};

static int drv_sensor_probe(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    if(ipmi_bmc_is_ok())
    {
        s3ip_sensor_drivers_register(&pfunc_bmc);
        mxsonic_sensor_drivers_register(&pfunc_bmc);
    }
    else
    {
        s3ip_sensor_drivers_register(&pfunc);
        mxsonic_sensor_drivers_register(&pfunc);
    }

#else
    hisonic_sensor_drivers_register(&pfunc);
    kssonic_sensor_drivers_register(&pfunc);
#endif

    return 0;
}

static int drv_sensor_remove(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    s3ip_sensor_drivers_unregister();
    mxsonic_sensor_drivers_unregister();
#else
    hisonic_sensor_drivers_unregister();
    kssonic_sensor_drivers_unregister();
#endif

    return 0;
}

static struct platform_driver drv_sensor_driver = {
    .driver = {
        .name   = DRVNAME,
        .owner = THIS_MODULE,
    },
    .probe      = drv_sensor_probe,
    .remove     = drv_sensor_remove,
};

static int __init drv_sensor_init(void)
{
    int err;
    int retval;

    drv_sensor_device = platform_device_alloc(DRVNAME, 0);
    if(!drv_sensor_device)
    {
        SENSOR_DEBUG("%s(#%d): platform_device_alloc fail\n", __func__, __LINE__);
        return -ENOMEM;
    }

    retval = platform_device_add(drv_sensor_device);
    if(retval)
    {
        SENSOR_DEBUG("%s(#%d): platform_device_add failed\n", __func__, __LINE__);
        err = -retval;
        goto dev_add_failed;
    }

    retval = platform_driver_register(&drv_sensor_driver);
    if(retval)
    {
        SENSOR_DEBUG("%s(#%d): platform_driver_register failed(%d)\n", __func__, __LINE__, err);
        err = -retval;
        goto dev_reg_failed;
    }

    return 0;

dev_reg_failed:
    platform_device_unregister(drv_sensor_device);
    return err;
dev_add_failed:
    platform_device_put(drv_sensor_device);
    return err;
}

static void __exit drv_sensor_exit(void)
{
    platform_driver_unregister(&drv_sensor_driver);
    platform_device_unregister(drv_sensor_device);

    return;
}

MODULE_DESCRIPTION("Huarong Sensor Driver");
MODULE_VERSION(SWITCH_SENSOR_DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(drv_sensor_init);
module_exit(drv_sensor_exit);