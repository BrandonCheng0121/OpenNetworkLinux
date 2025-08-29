#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/i2c.h>

#include "pmbus.h"
#include "switch_avs_driver.h"
#include "sysfs_ipmi.h"

#define DRVNAME "drv_avs_driver"
#define SWITCH_AVS_DRIVER_VERSION "0.0.1"
#define MAX_BUFF_SIZE (1024)
unsigned int loglevel = 0;
static struct platform_device *drv_avs_device;

struct mutex     update_lock;

#define MP2976_MAX_TEMP 112000
#define MP2973_MAX_TEMP 112000

#define MP2976_MAX_OUT1_CURRENT 35000
#define MP2976_MAX_OUT2_CURRENT 35000
// Micro short circuit current value
#define MP2976_WARN_OUT1_CURRENT 25000
#define MP2976_WARN_OUT2_CURRENT 24000

#define MP2973_MAX_OUT1_CURRENT 220000
#define MP2973_MAX_OUT2_CURRENT 45000
// Micro short circuit current value
#define MP2973_WARN_OUT1_CURRENT 215000
#define MP2973_WARN_OUT2_CURRENT 21000

// #define MP29XX_FMEA_PAGE0DUMP_NUM (31-15)
// #define MP29XX_FMEA_PAGE1DUMP_NUM (30-16)

typedef struct avs_fmea_dump_reg_s {
    int page;
    int reg;
    PMBUS_PROTOCAL_TYPE_E type;
}AVS_FMEA_DUMP_REG_T;

static AVS_FMEA_DUMP_REG_T mp2973_fmea_dump_reg_list[] = {
    /* Page 0 */
    {0, 0x20, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT_MODE
    {0, 0x21, PMBUS_PROTOCAL_TYPE_WORD},   // VOUT_COMMAND
    {0, 0x79, PMBUS_PROTOCAL_TYPE_WORD},   // STATUS_WORD
    {0, 0x7A, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_VOUT
    {0, 0x7B, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_IOUT
    {0, 0x7C, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_INPUT
    {0, 0x7D, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_TEMPERATURE
    {0, 0x7E, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_CML
    {0, 0x88, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VIN
    {0, 0x8B, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT
    {0, 0x8C, PMBUS_PROTOCAL_TYPE_WORD},   // READ_IOUT
    {0, 0x8D, PMBUS_PROTOCAL_TYPE_WORD},   // READ_TEMPERATURE
    {0, 0x96, PMBUS_PROTOCAL_TYPE_WORD},   // READ_POUT
    {0, 0x9A, PMBUS_PROTOCAL_TYPE_BLOCK},  // SVID_PRODUCT_ID
    {0, 0x82, PMBUS_PROTOCAL_TYPE_WORD},   // READ_CS1_2
    {0, 0x83, PMBUS_PROTOCAL_TYPE_WORD},   // READ_CS3_4
    {0, 0x84, PMBUS_PROTOCAL_TYPE_WORD},   // READ_CS5_6
    {0, 0x80, PMBUS_PROTOCAL_TYPE_BYTE},   // DRMOS_FAULT
    {0, 0x9D, PMBUS_PROTOCAL_TYPE_BLOCK},  // CONFIG_ID
    {0, 0x90, PMBUS_PROTOCAL_TYPE_WORD},   // READ_IOUT_PK
    {0, 0x91, PMBUS_PROTOCAL_TYPE_WORD},   // READ_POUT_PK
    {0, 0xA4, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT_MIN
    {0, 0xA5, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT_MAX
    {0, 0xE5, PMBUS_PROTOCAL_TYPE_BYTE},   // MFR_OVP_TH_SET
    {0, 0xE6, PMBUS_PROTOCAL_TYPE_BYTE},   // MFR_UVP_SET
    {0, 0x9B, PMBUS_PROTOCAL_TYPE_BLOCK},  // PRODUCT_REV_USER
    /* Page 1 */
    {1, 0x20, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT_MODE
    {1, 0x21, PMBUS_PROTOCAL_TYPE_WORD},   // VOUT_COMMAND
    {1, 0x79, PMBUS_PROTOCAL_TYPE_WORD},   // STATUS_WORD
    {1, 0x7A, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_VOUT
    {1, 0x7B, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_IOUT
    {1, 0x88, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VIN
    {1, 0x8B, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT
    {1, 0x8C, PMBUS_PROTOCAL_TYPE_WORD},   // READ_IOUT
    {1, 0x8D, PMBUS_PROTOCAL_TYPE_WORD},   // READ_TEMPERATURE
    {1, 0x96, PMBUS_PROTOCAL_TYPE_WORD},   // READ_POUT
    {1, 0x9A, PMBUS_PROTOCAL_TYPE_WORD},   // MFR_CS_OFFSET2
    {1, 0x82, PMBUS_PROTOCAL_TYPE_WORD},   // READ_TSEN2_SENSE
    {1, 0x83, PMBUS_PROTOCAL_TYPE_WORD},   // READ_PMBUS_ADDR
    {1, 0x85, PMBUS_PROTOCAL_TYPE_WORD},   // READ_CS1_2_L2
    {1, 0x90, PMBUS_PROTOCAL_TYPE_WORD},   // READ_IOUT_PK
    {1, 0x91, PMBUS_PROTOCAL_TYPE_WORD},   // READ_POUT_PK
    {1, 0xA4, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT_MIN
    {1, 0xA5, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT_MAX
    {1, 0xE5, PMBUS_PROTOCAL_TYPE_BYTE},   // MFR_OVP_TH_SET
    {1, 0xE6, PMBUS_PROTOCAL_TYPE_BYTE},   // MFR_UVP_SET
};

static AVS_FMEA_DUMP_REG_T mp2976_fmea_dump_reg_list[] = {
    /* Page 0 */
    {0, 0x20, PMBUS_PROTOCAL_TYPE_BYTE},   // VOUT_MODE
    {0, 0x21, PMBUS_PROTOCAL_TYPE_WORD},   // VOUT_COMMAND
    {0, 0x79, PMBUS_PROTOCAL_TYPE_WORD},   // STATUS_WORD
    {0, 0x7A, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_VOUT
    {0, 0x7B, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_IOUT
    {0, 0x7C, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_INPUT
    {0, 0x7D, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_TEMPERATURE
    {0, 0x7E, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_CML
    {0, 0x88, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VIN
    {0, 0x8B, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT
    {0, 0x8C, PMBUS_PROTOCAL_TYPE_WORD},   // READ_IOUT
    {0, 0x8D, PMBUS_PROTOCAL_TYPE_WORD},   // READ_TEMPERATURE
    {0, 0x96, PMBUS_PROTOCAL_TYPE_WORD},   // READ_POUT
    {0, 0x9A, PMBUS_PROTOCAL_TYPE_WORD},   // SVID_PRODUCT_ID
    {0, 0x82, PMBUS_PROTOCAL_TYPE_WORD},   // READ_CS1_2
    {0, 0x80, PMBUS_PROTOCAL_TYPE_BYTE},   // DRMOS_FAULT
    {0, 0x9D, PMBUS_PROTOCAL_TYPE_WORD},   // CONFIG_ID
    {0, 0x90, PMBUS_PROTOCAL_TYPE_WORD},   // READ_IOUT_PK
    {0, 0x91, PMBUS_PROTOCAL_TYPE_WORD},   // READ_POUT_PK 
    {0, 0xA4, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT_MIN
    {0, 0xA5, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT_MAX
    {0, 0xE5, PMBUS_PROTOCAL_TYPE_BYTE},   // MFR_OVP_TH_SET
    {0, 0xE6, PMBUS_PROTOCAL_TYPE_BYTE},   // MFR_UVP_SET
    {0, 0x3C, PMBUS_PROTOCAL_TYPE_WORD},   // MFR_CONFIG_REV_MPS
    /* Page 1 */
    {1, 0x21, PMBUS_PROTOCAL_TYPE_WORD},   // VOUT_COMMAND
    {1, 0x79, PMBUS_PROTOCAL_TYPE_WORD},   // STATUS_WORD
    {1, 0x7A, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_VOUT
    {1, 0x7B, PMBUS_PROTOCAL_TYPE_BYTE},   // STATUS_IOUT
    {1, 0x8B, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT
    {1, 0x8C, PMBUS_PROTOCAL_TYPE_WORD},   // READ_IOUT
    {1, 0x96, PMBUS_PROTOCAL_TYPE_WORD},   // READ_POUT
    {1, 0x9A, PMBUS_PROTOCAL_TYPE_WORD},   // MFR_CS_OFFSET2
    {1, 0x85, PMBUS_PROTOCAL_TYPE_WORD},   // READ_CS1_2_L2
    {1, 0x90, PMBUS_PROTOCAL_TYPE_WORD},   // READ_IOUT_PK
    {1, 0x91, PMBUS_PROTOCAL_TYPE_WORD},   // READ_POUT_PK
    {1, 0xA4, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT_MIN
    {1, 0xA5, PMBUS_PROTOCAL_TYPE_WORD},   // READ_VOUT_MAX
    {1, 0xE5, PMBUS_PROTOCAL_TYPE_BYTE},   // MFR_OVP_TH_SET
    {1, 0xE6, PMBUS_PROTOCAL_TYPE_BYTE},   // MFR_UVP_SET
};

enum FMEA_CHIPS {
    FMEA_MP2976 = 0,
    FMEA_MP2973,
    FMEA_MAX34461,
    FMEA_MP_MAX
};
enum FMEA_CHIPS_ID {
    FMEA_MP2976_ID = 0,
    FMEA_MP2973_ID,
    FMEA_MAX34461_ID,
    FMEA_MP_ID_MAX = 3
};
typedef struct
{
    int bus;
    int addr;
    int id;
}MP29XX_ATTR;
MP29XX_ATTR mp29xx_attr_table[FMEA_MP_MAX] =
{
    {.bus = 4, .addr = 0x5f, .id = 0x2576},
    {.bus = 4, .addr = 0x20, .id = 0x2573},
    {.bus = 8, .addr = 0x74, .id = 0x4d4e},
};
#define MAX34461_VIN_NUM 14
typedef struct
{
    int vin_min;
    int vin_max;
    char* vin_attr_name;
}MAX34461_ATTR;
MAX34461_ATTR max34461_vin_table[MAX34461_VIN_NUM] =
{
    {.vin_min = 1710,  .vin_max = 1890, .vin_attr_name = "in1_input"},
    {.vin_min =  760,  .vin_max =  840, .vin_attr_name = "in2_input"},
    {.vin_min =  780,  .vin_max = 1020, .vin_attr_name = "in3_input"},
    {.vin_min = 1710,  .vin_max = 1890, .vin_attr_name = "in4_input"},
    {.vin_min = 1710,  .vin_max = 1890, .vin_attr_name = "in5_input"},
    {.vin_min = 1710,  .vin_max = 1890, .vin_attr_name = "in6_input"},
    {.vin_min = 1140,  .vin_max = 1260, .vin_attr_name = "in7_input"},
    {.vin_min = 1710,  .vin_max = 1890, .vin_attr_name = "in8_input"},
    {.vin_min = 1710,  .vin_max = 1890, .vin_attr_name = "in9_input"},
    {.vin_min = 1701,  .vin_max = 1880, .vin_attr_name = "in10_input"},
    {.vin_min = 1701,  .vin_max = 1880, .vin_attr_name = "in11_input"},
    {.vin_min = 1710,  .vin_max = 1890, .vin_attr_name = "in12_input"},
    {.vin_min =  950,  .vin_max = 1050, .vin_attr_name = "in13_input"},
    {.vin_min = 1710,  .vin_max = 1890, .vin_attr_name = "in14_input"}
};


static void dump_fmea_reg(unsigned int index, AVS_FMEA_DUMP_REG_T *dump_reg_list, unsigned int size, char *buf)
{
    int i, block_index;
    int retval;
    int page, last_page = -1;
    int reg;
    PMBUS_PROTOCAL_TYPE_E type;
    unsigned char tmp[64] = {0};
    unsigned char val[I2C_SMBUS_BLOCK_MAX] = {0};

    for(i = 0; i < size; i++)
    {
        page = dump_reg_list[i].page;
        reg = dump_reg_list[i].reg;
        type = dump_reg_list[i].type;

        if(page != last_page)
        {
            if(sprintf_s(tmp, 64, "page%d", page) < 0)
            {
                AVS_DEBUG("avs%d sprintf_s failed\n", index);
            }

            if(strcat_s(buf, 1024, tmp) != 0)
            {
                AVS_DEBUG("avs%d strcat_s page failed\n", index);
            }
            last_page = page;
        }

        memset_s(val, I2C_SMBUS_BLOCK_MAX, 0, sizeof(val));
        retval = pmbus_core_read_reg(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr, page, reg, type, val);
        if(retval < 0)
        {
            // continue to dump other registers
            AVS_DEBUG("Read avs%d page %d reg 0x%x type %d failed\n", index, page, reg, type);
            continue;
        }

        memset_s(tmp, 64, 0, sizeof(tmp));
        if(type == PMBUS_PROTOCAL_TYPE_BYTE)
        {
            if(sprintf_s(tmp, 64, " 0x%x: 0x%02x", reg, val[0]) < 0)
            {
                AVS_DEBUG("avs%d sprintf_s failed\n", index);
            }

            if(strcat_s(buf, 1024, tmp) != 0)
            {
                AVS_DEBUG("avs%d strcat_s failed\n", index);
                continue;
            }
        }
        else if(type == PMBUS_PROTOCAL_TYPE_WORD)
        {
            if(sprintf_s(tmp, 64, " 0x%x: 0x%02x%02x", reg, val[1], val[0]) < 0)
            {
                AVS_DEBUG("avs%d sprintf_s failed\n", index);
            }

            if(strcat_s(buf, 1024, tmp) != 0)
            {
                AVS_DEBUG("avs%d strcat_s failed\n", index);
                continue;
            }
        }
        else if(type == PMBUS_PROTOCAL_TYPE_BLOCK)
        {
            if(sprintf_s(tmp, 64, " 0x%x: 0x", reg) < 0)
            {
                AVS_DEBUG("avs%d sprintf_s failed\n", index);
            }

            if(strcat_s(buf, 1024, tmp) != 0)
            {
                AVS_DEBUG("avs%d strcat_s failed\n", index);
                continue;
            }

            for(block_index = 0; block_index < retval ; block_index++)
            {
                memset_s(tmp, 64, 0, sizeof(tmp));
                if(sprintf_s(tmp, 64, "%02x", val[block_index]) < 0)
                {
                    AVS_DEBUG("avs%d sprintf_s block failed\n", index);
                }
                if(strcat_s(buf, 1024, tmp) != 0)
                {
                    AVS_DEBUG("avs%d strcat_s block failed\n", index);
                    break;
                }
            }
        }
    }

    return;
}

ssize_t drv_fmea_get_work_status(unsigned int index, char *buf, char *plt)
{
    int retval = -1;
    char tmp_buf[MAX_BUFF_SIZE] = {0};
    long val = 0;
    int len = 0;
    int count = 0;
    int i = 0;
    AVS_FMEA_DUMP_REG_T *dump_reg_list = NULL;
    unsigned int dump_size = 0;

    switch(index)
    {
        case FMEA_MP2976:
            dump_size = (sizeof(mp2976_fmea_dump_reg_list)/sizeof(AVS_FMEA_DUMP_REG_T));
            dump_reg_list = mp2976_fmea_dump_reg_list;
            retval = pmbus_core_read_word(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                        0, PMBUS_STATUS_WORD, &val);
            if(retval < 0)
            {
                AVS_DEBUG("Read mp29xx reg failed\n");
                return retval;
            }
            if((val & 0xFBFD) != 0) //Ignore bit[10 1]
                goto mp2976_work_abnormal;
            retval = pmbus_core_read_word(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                            1, PMBUS_STATUS_WORD, &val);
            if(retval < 0)
            {
                AVS_DEBUG("Read mp29xx reg failed\n");
                return retval;
            }
            if((val & 0xFBFD) != 0) //Ignore bit[10 1]
                goto mp2976_work_abnormal;
            retval = pmbus_core_read_attrs(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                            "temp1_input", &val);
            if(retval < 0)
            {
                AVS_DEBUG("Read mp29xx PMBUS_READ_TEMPERATURE_1 failed\n");
                return retval;
            }

            if(val > MP2976_MAX_TEMP) //temp
                goto mp2976_work_abnormal;
            return sprintf_s(buf, PAGE_SIZE, "%s\n", (!strcmp(plt, "hi") ? "0" : "0 0x00000000"));
        mp2976_work_abnormal:
            dump_fmea_reg(index, dump_reg_list, dump_size, tmp_buf);
            pmbus_core_clear_fault(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr);
            return sprintf_s(buf, PAGE_SIZE, "1 %s\n", tmp_buf);
        break;
        case FMEA_MP2973:// judges status register
            dump_size = (sizeof(mp2973_fmea_dump_reg_list)/sizeof(AVS_FMEA_DUMP_REG_T));
            dump_reg_list = mp2973_fmea_dump_reg_list;
            retval = pmbus_core_read_word(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                        0, PMBUS_STATUS_WORD, &val);
            if(retval < 0)
            {
                AVS_DEBUG("Read mp29xx reg failed\n");
                return retval;
            }
            if((val & 0xF7FD) != 0) //Ignore bit[11 1]
                goto mp2973_work_abnormal;
            retval = pmbus_core_read_word(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                            1, PMBUS_STATUS_WORD, &val);
            if(retval < 0)
            {
                AVS_DEBUG("Read mp29xx reg failed\n");
                return retval;
            }
            if((val & 0xF7FD) != 0) //Ignore bit[11 1]
                goto mp2973_work_abnormal;
            retval = pmbus_core_read_attrs(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                            "temp1_input", &val);
            if(retval < 0)
            {
                AVS_DEBUG("Read mp29xx PMBUS_READ_TEMPERATURE_1 failed\n");
                return retval;
            }

            if(val > MP2973_MAX_TEMP) //temp
                goto mp2973_work_abnormal;
            return sprintf_s(buf, PAGE_SIZE, "%s\n", (!strcmp(plt, "hi") ? "0" : "0 0x00000000"));
        mp2973_work_abnormal:
            dump_fmea_reg(index, dump_reg_list, dump_size, tmp_buf);
            pmbus_core_clear_fault(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr);
            return sprintf_s(buf, PAGE_SIZE, "1 %s\n", tmp_buf);
        break;
        case FMEA_MAX34461://no status register, judges by input and output voltage range
            for(i = 0; i< MAX34461_VIN_NUM; i++)
            {
                retval = pmbus_core_read_attrs(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                        max34461_vin_table[i].vin_attr_name, &val);
                if(retval < 0)
                {
                    AVS_DEBUG("Read MAX34461 failed\n");
                    return retval;
                }
                if((val > max34461_vin_table[i].vin_max) || (val < max34461_vin_table[i].vin_min))
                {
                    len = sprintf_s(tmp_buf+count, MAX_BUFF_SIZE, "vin%d error\n", i+1);
                    count += len;
                }
            }
            if(count == 0)
                return sprintf_s(buf, PAGE_SIZE, "%s\n", (!strcmp(plt, "hi") ? "0" : "0 0x00000000"));
            return sprintf_s(buf, PAGE_SIZE, "1\n%s",tmp_buf);
        break;
        default:
            retval = -1;
        break;

    }
    return retval;
}

ssize_t drv_fmea_get_current_status(unsigned int index, char *buf, char *plt)
{
    int retval = -1;
    long multiplier = 1000;
    long vout1 = 0, vout2 = 0, iout1 = 0, iout2 = 0, pout1 = 0, pout2 = 0;

    switch(index)
    {
        case FMEA_MP2976:
            retval = pmbus_core_read_attrs(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                            "curr2_input", &iout1);
            if(retval < 0)
            {
                AVS_DEBUG("Read MP2976 IOUT failed\n");
                return retval;
            }
            retval = pmbus_core_read_attrs(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                            "curr3_input", &iout2);
            if(retval < 0)
            {
                AVS_DEBUG("Read MP2976 IOUT failed\n");
                return retval;
            }
            retval = pmbus_core_read_attrs(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                        "in2_input", &vout1);
            if(retval < 0)
            {
                AVS_DEBUG("Read MP2976 VOUT failed\n");
                return retval;
            }
            retval = pmbus_core_read_attrs(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                        "in3_input", &vout2);
            if(retval < 0)
            {
                AVS_DEBUG("Read MP2976 VOUT failed\n");
                return retval;
            }
            if((iout1 > MP2976_MAX_OUT1_CURRENT) || (iout2 > MP2976_MAX_OUT2_CURRENT))
            {
                pout1 = vout1 * iout1;
                pout2 = vout2 * iout2;
                return sprintf_s(buf, PAGE_SIZE, "1\n %ld.%03ld(V) %s%ld.%03ld(A) %s%ld.%03ld(W)\n %ld.%03ld(V) %s%ld.%03ld(A) %s%ld.%03ld(W)\n",
                                (vout1 / multiplier), (vout1 % multiplier),
                                ((iout1 < 0) ? "-":""), abs(iout1 / multiplier), abs(iout1 % multiplier),
                                ((pout1 < 0) ? "-":""), abs(pout1 / (multiplier * multiplier)), ((abs(pout1 % (multiplier * multiplier))) / multiplier),
                                (vout2 / multiplier), (vout2 % multiplier),
                                ((iout2 < 0) ? "-":""), abs(iout2 / multiplier), abs(iout2 % multiplier),
                                ((pout2 < 0) ? "-":""), abs(pout2 / (multiplier * multiplier)), ((abs(pout2 % (multiplier * multiplier))) / multiplier));
            }
            else if((iout1 > MP2976_WARN_OUT1_CURRENT) || (iout2 > MP2976_WARN_OUT2_CURRENT))
            {
                pout1 = vout1 * iout1;
                pout2 = vout2 * iout2;
                return sprintf_s(buf, PAGE_SIZE, "2\n %ld.%03ld(V) %s%ld.%03ld(A) %s%ld.%03ld(W)\n %ld.%03ld(V) %s%ld.%03ld(A) %s%ld.%03ld(W)\n",
                                (vout1 / multiplier), (vout1 % multiplier),
                                ((iout1 < 0) ? "-":""), abs(iout1 / multiplier), abs(iout1 % multiplier),
                                ((pout1 < 0) ? "-":""), abs(pout1 / (multiplier * multiplier)), ((abs(pout1 % (multiplier * multiplier))) / multiplier),
                                (vout2 / multiplier), (vout2 % multiplier),
                                ((iout2 < 0) ? "-":""), abs(iout2 / multiplier), abs(iout2 % multiplier),
                                ((pout2 < 0) ? "-":""), abs(pout2 / (multiplier * multiplier)), ((abs(pout2 % (multiplier * multiplier))) / multiplier));
            }
            else
                return sprintf_s(buf, PAGE_SIZE, "%s\n", (!strcmp(plt, "hi") ? "0" : "0 0x00000000"));
        case FMEA_MP2973:
            retval = pmbus_core_read_attrs(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                            "curr2_input", &iout1);
            if(retval < 0)
            {
                AVS_DEBUG("Read MP2973 IOUT failed\n");
                return retval;
            }
            retval = pmbus_core_read_attrs(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                            "curr3_input", &iout2);
            if(retval < 0)
            {
                AVS_DEBUG("Read MP2973 IOUT failed\n");
                return retval;
            }
            retval = pmbus_core_read_attrs(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                        "in2_input", &vout1);
            if(retval < 0)
            {
                AVS_DEBUG("Read MP2973 VOUT failed\n");
                return retval;
            }
            retval = pmbus_core_read_attrs(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                        "in3_input", &vout2);
            if(retval < 0)
            {
                AVS_DEBUG("Read MP2973 VOUT failed\n");
                return retval;
            }
            if((iout1 > MP2973_MAX_OUT1_CURRENT) || (iout2 > MP2973_MAX_OUT2_CURRENT))
            {
                pout1 = vout1 * iout1;
                pout2 = vout2 * iout2;
                return sprintf_s(buf, PAGE_SIZE, "1 %ld.%03ld(V) %s%ld.%03ld(A) %s%ld.%03ld(W)  %ld.%03ld(V) %s%ld.%03ld(A) %s%ld.%03ld(W)\n",
                                (vout1 / multiplier), (vout1 % multiplier),
                                ((iout1 < 0) ? "-":""), abs(iout1 / multiplier), abs(iout1 % multiplier),
                                ((pout1 < 0) ? "-":""), abs(pout1 / (multiplier * multiplier)), ((abs(pout1 % (multiplier * multiplier))) / multiplier),
                                (vout2 / multiplier), (vout2 % multiplier),
                                ((iout2 < 0) ? "-":""), abs(iout2 / multiplier), abs(iout2 % multiplier),
                                ((pout2 < 0) ? "-":""), abs(pout2 / (multiplier * multiplier)), ((abs(pout2 % (multiplier * multiplier))) / multiplier));
            }
            else if((iout1 > MP2973_WARN_OUT1_CURRENT) || (iout2 > MP2973_WARN_OUT2_CURRENT))
            {
                pout1 = vout1 * iout1;
                pout2 = vout2 * iout2;
                return sprintf_s(buf, PAGE_SIZE, "2 %ld.%03ld(V) %s%ld.%03ld(A) %s%ld.%03ld(W)  %ld.%03ld(V) %s%ld.%03ld(A) %s%ld.%03ld(W)\n",
                                (vout1 / multiplier), (vout1 % multiplier),
                                ((iout1 < 0) ? "-":""), abs(iout1 / multiplier), abs(iout1 % multiplier),
                                ((pout1 < 0) ? "-":""), abs(pout1 / (multiplier * multiplier)), ((abs(pout1 % (multiplier * multiplier))) / multiplier),
                                (vout2 / multiplier), (vout2 % multiplier),
                                ((iout2 < 0) ? "-":""), abs(iout2 / multiplier), abs(iout2 % multiplier),
                                ((pout2 < 0) ? "-":""), abs(pout2 / (multiplier * multiplier)), ((abs(pout2 % (multiplier * multiplier))) / multiplier));
            }
            else
                return sprintf_s(buf, PAGE_SIZE, "%s\n", (!strcmp(plt, "hi") ? "0" : "0 0x00000000"));

        break;
        case FMEA_MAX34461://no support current get
            return sprintf_s(buf, PAGE_SIZE, "NA\n");
        break;
        default:
            retval = -1;
        break;

    }
    return retval;
}

ssize_t drv_fmea_get_pmbus_status(unsigned int index, char *buf, char *plt)
{
    int retval = -1;
    long mp29xx_id_l = 0;
    long mp29xx_id_h = 0;
    long mp29xx_id = 0;

    switch(index)
    {
        case FMEA_MP2976:

        case FMEA_MAX34461:
            retval = pmbus_core_read_word(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                            0, PMBUS_MFR_ID, &mp29xx_id_h);
            if(retval < 0)
            {
                AVS_DEBUG("Read MP29XX ID failed  %d\n",retval);
                return retval;
            }
            retval = pmbus_core_read_word(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                            0, PMBUS_MFR_MODEL, &mp29xx_id_l);
            if(retval < 0)
            {
                AVS_DEBUG("Read MP29XX MODEL NAME failed\n");
                return retval;
            }
            mp29xx_id = ((mp29xx_id_h&0xff)<<8)+((mp29xx_id_l&0xff)<<0);
            if(mp29xx_id != mp29xx_attr_table[index].id)
                return sprintf_s(buf, PAGE_SIZE, "1 %lx\n", mp29xx_id);
            else
                return sprintf_s(buf, PAGE_SIZE, "%s\n", (!strcmp(plt, "hi") ? "0" : "0 0x00000000"));
        break;
        case FMEA_MP2973:
            retval = pmbus_core_read_word(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                            0, PMBUS_MFR_ID, &mp29xx_id_h);
            if(retval < 0)
            {
                AVS_DEBUG("Read MP29XX ID failed  %d\n",retval);
                return retval;
            }
            retval = pmbus_core_read_word(mp29xx_attr_table[index].bus, mp29xx_attr_table[index].addr,
                                            0, PMBUS_MFR_MODEL, &mp29xx_id_l);
            if(retval < 0)
            {
                AVS_DEBUG("Read MP29XX MODEL NAME failed\n");
                return retval;
            }
            mp29xx_id = (mp29xx_id_h&0xff00)+((mp29xx_id_l&0xff00)>>8);
            if(mp29xx_id != mp29xx_attr_table[index].id)
                return sprintf_s(buf, PAGE_SIZE, "1 %lx\n", mp29xx_id);
            else
                return sprintf_s(buf, PAGE_SIZE, "%s\n", (!strcmp(plt, "hi") ? "0" : "0 0x00000000"));
        break;
        default:
            retval = -1;
        break;
    }
    return retval;
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

static struct avs_drivers_t pfunc = {
    .fmea_get_work_status = drv_fmea_get_work_status,
    .fmea_get_current_status = drv_fmea_get_current_status,
    .fmea_get_pmbus_status = drv_fmea_get_pmbus_status,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
};

static struct avs_drivers_t pfunc_bmc = {
    .fmea_get_work_status = drv_fmea_get_work_status_from_bmc,
    .fmea_get_current_status = drv_fmea_get_current_status_from_bmc,
    .fmea_get_pmbus_status = drv_fmea_get_pmbus_status_from_bmc,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
};

static int drv_avs_probe(struct platform_device *pdev)
{
    if(ipmi_bmc_is_ok())
    {
        mxsonic_avs_drivers_register(&pfunc_bmc);
    }
    else
    {
        mxsonic_avs_drivers_register(&pfunc);
    }
    return 0;
}

static int drv_avs_remove(struct platform_device *pdev)
{
    mxsonic_avs_drivers_unregister();
    return 0;
}

static struct platform_driver drv_avs_driver = {
    .driver = {
        .name   = DRVNAME,
        .owner = THIS_MODULE,
    },
    .probe      = drv_avs_probe,
    .remove     = drv_avs_remove,
};

static int __init drv_avs_init(void)
{
    int err;
    int retval;

    drv_avs_device = platform_device_alloc(DRVNAME, 0);
    if(!drv_avs_device)
    {
        AVS_DEBUG("%s(#%d): platform_device_alloc fail\n", __func__, __LINE__);
        return -ENOMEM;
    }

    retval = platform_device_add(drv_avs_device);
    if(retval)
    {
        AVS_DEBUG("%s(#%d): platform_device_add failed\n", __func__, __LINE__);
        err = -retval;
        goto dev_add_failed;
    }

    retval = platform_driver_register(&drv_avs_driver);
    if(retval)
    {
        AVS_DEBUG("%s(#%d): platform_driver_register failed(%d)\n", __func__, __LINE__, err);
        err = -retval;
        goto dev_reg_failed;
    }

    mutex_init(&update_lock);

    return 0;

dev_reg_failed:
    platform_device_unregister(drv_avs_device);
    return err;

dev_add_failed:
    platform_device_put(drv_avs_device);
    return err;
}

static void __exit drv_avs_exit(void)
{
    platform_driver_unregister(&drv_avs_driver);
    platform_device_unregister(drv_avs_device);

    mutex_destroy(&update_lock);

    return;
}

MODULE_DESCRIPTION("Huarong AVS Driver");
MODULE_VERSION(SWITCH_AVS_DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(drv_avs_init);
module_exit(drv_avs_exit);
