#include <linux/module.h>
#include <linux/fs.h>
#include <uapi/linux/ipmi.h>
#include <linux/poll.h>
#include <linux/ipmi.h>
#include <linux/ipmi_smi.h>
#include <linux/semaphore.h>

#include "cpld_lpc_driver.h"
#include "switch_psu_driver.h"
#include "switch_sensor_driver.h"
#include "switch_bmc_driver.h"
#include "psu_g1156.h"

/* ott_ipmi_comm.h */
#define OTT_IPMI_XFER_DEFAULT_TIMEOUT 5
unsigned int loglevel_bmc = 0;
EXPORT_SYMBOL_GPL(loglevel_bmc);

#define IPMI_INTF_NUM (0)
volatile static unsigned char g_req_netfn = 0;
volatile static unsigned char g_req_cmd = 0;
static struct ipmi_user *g_ipmi_mh_user = NULL;
static struct semaphore g_ipmi_msg_sem;
static struct ipmi_recv_msg g_bmc_recv_msg;
typedef struct {
    long timeout; /* 单位是秒，ott_ipmi_xfer对小于或等于0的值，会用OTT_IPMI_XFER_DEFAULT_TIMEOUT，所以可以初始化成0 */
    struct kernel_ipmi_msg req;
    struct kernel_ipmi_msg rsp;
} ott_ipmi_xfer_info_s;


typedef struct {
    struct wait_queue_head * wait_address;
    poll_table pt;
} ott_ipmi_poll_info_s;

typedef struct {
    unsigned char ccode; /* IPMI completion code */
    unsigned int value;
} bmc_fan_speed_get_s;

enum ipmi_mfr_info_type
{
    ACK_DATA_RESP_CHAR      = 1,
    ACK_DATA_RESP_INT       = 2,
    ACK_DATA_RESP_FLOAT     = 3,
    ACK_DATA_RESP_FLOAT_DIV = 4,
    ACK_DATA_RESP_PRESENT   = 5,
    ACK_DATA_RESP_BITMAP    = 6,
};

enum ipmi_app_power_attribute_type
{
    POWER_GOOD           = 0x02,
    POWER_STATUS         = 0x03,
    POWER_CURRENT_LIMIT  = 0x04,
    POWER_TEMP_LIMIT     = 0x05,
    POWER_INPUT_VOLTAGE  = 0x0E,
    POWER_OUTPUT_VOLTAGE = 0x0F,
    POWER_OUTPUT_CURRENT = 0x10,
    POWER_TEMPERATURE    = 0x11,
    POWER_FAN_SPEED      = 0x12,
    POWER_OUTPUT_POWER   = 0x13,
    POWER_INPUT_POWER    = 0x14,
    POWER_HARDWARE_VER   = 0x21,
    POWER_INPUT_CURRENT  = 0x23,
    POWER_TEMPERATURE_1  = 0x24,
    POWER_TEMPERATURE_2  = 0x25,
    POWER_TEMPERATURE_3  = 0x26,
    POWER_POUT_MAX       = 0x27,
    POWER_VIN_TYPE       = 0x28,
    POWER_LED_STATUS     = 0x29,
};

enum ipmi_app_power_alarm_type
{
    POWER_ALARM_CMD            = 0x1,
    POWER_ALARM_TEMPERATURE    = 0x2,
    POWER_ALARM_VIN_UV_FAULT   = 0x3,
    POWER_ALARM_IOUT_OC_FAULT  = 0x4,
    POWER_ALARM_VOUT_OV_FAULT  = 0x5,
    POWER_ALARM_VIN_OV_DETECT  = 0x6,
    POWER_ALARM_VIN_UNIT_OFF   = 0x7,
    POWER_ALARM_OTHER          = 0x9,
    POWER_ALARM_FAN            = 0xA,
    POWER_ALARM_INPUT          = 0xD,
    POWER_ALARM_IOUT_POUT      = 0xE,
    POWER_ALARM_VOUT           = 0xF,
};


enum psu_eeprom_read_type
{
    eeprom_psu_part_number      = 0x01,
    eeprom_psu_serial_number    = 0x02,
    eeprom_psu_vendor           = 0x03,
    eeprom_psu_model_name       = 0x04,
    eeprom_psu_hardware_version = 0x05,
    eeprom_psu_date             = 0x06,
};

enum Bits
{
    BIT0    = 0x0001,
    BIT1    = 0x0002,
    BIT2    = 0x0004,
    BIT3    = 0x0008,
    BIT4    = 0x0010,
    BIT5    = 0x0020,
    BIT6    = 0x0040,
    BIT7    = 0x0080,
    BIT8    = 0x0100,
    BIT9    = 0x0200,
    BIT10   = 0x0400,
    BIT11   = 0x0800,
    BIT12   = 0x1000,
    BIT13   = 0x2000,
    BIT14   = 0x4000,
    BIT15   = 0x8000,
};

enum fan_eeprom_read_type
{
    eeprom_fan_part_number      = 0x01,
    eeprom_fan_serial_number    = 0x02,
    eeprom_fan_vendor           = 0x03,
    eeprom_fan_model_name       = 0x04,
    eeprom_fan_direction        = 0x05,
    eeprom_fan_hw_version       = 0x06,
};

static DEFINE_MUTEX(ott_ipmi_mutex);

#define IPMI_NETFN                   0x36
#define IPMI_GET_THERMAL_DATA        0x12
#define IPMI_GET_FAN_SPEED           0x14
#define IPMI_GET_FAN_PWM             0x16
#define IPMI_SET_FAN_PWM             0x17
#define IPMI_GET_PSU_ALARM_DATA      0x18
#define IPMI_GET_PSU_ATTRIBUTE_DATA  0x1A
#define IPMI_GET_PSU_E_LABEL_DATA    0x1C
#define IPMI_FAN_GET_EEPROM          0xA0
#define IPMI_PSU_GET_EEPROM          0xA1
#define IPMI_GET_SENSOR_DATA         0xA2
#define IPMI_CMD_GET_FMEA_STATUS     0xA6

#define IPMI_PSU_PRESENT             1
#define IPMI_PSU_ABSENT              2


#define PMBUS_MFR_MODEL_LEN     13+1
#define PMBUS_MFR_SERIAL_LEN    20+1
#define PMBUS_MFR_DATE_LEN      10+1
#define PMBUS_MFR_ID_LEN        6+1
#define PMBUS_MFR_REVISION_LEN  1+1
#define PMBUS_PART_NUM_LEN      16+1
#define PMBUS_HW_VERSION_LEN    8+1
#define PMBUS_MFR_MAX_LEN       (30+1)

#define POWER_ALARM_CMD         1
#define eeprom_fan_direction    5

static void msg_handler(struct ipmi_recv_msg *msg, void *handler_data)
{
    if(((g_req_netfn+1) != msg->msg.netfn) || (g_req_cmd != msg->msg.cmd))
    {
        ipmi_free_recv_msg(msg);
        return;
    }
    g_bmc_recv_msg = *msg;
    g_bmc_recv_msg.msg.data = g_bmc_recv_msg.msg_data;
    ipmi_free_recv_msg(msg);
    up(&g_ipmi_msg_sem);
    return;
}

static struct ipmi_user_hndl ipmi_hndlrs = {
    .ipmi_recv_hndl = msg_handler,
};

#define IPMI_REQ_RETRY_TIME_MS 0
#define IPMI_REQ_MAX_RETRIES -1
#define IPMI_REQ_PRIORITY 1
int ott_ipmi_xfer(ott_ipmi_xfer_info_s * para)
{
    int ret;
    int rsp_len = 0;
    int timeout = 0;
    struct ipmi_system_interface_addr addr;
    struct kernel_ipmi_msg msg =
    {
        .netfn= para->req.netfn,
        .cmd = para->req.cmd,
        .data_len = para->req.data_len,
        .data = para->req.data
    };
    addr.addr_type = IPMI_SYSTEM_INTERFACE_ADDR_TYPE;
    addr.channel = IPMI_BMC_CHANNEL;
    addr.lun = 0;
    mutex_lock(&ott_ipmi_mutex);
    ret = ipmi_request_settime(g_ipmi_mh_user, (struct ipmi_addr *)&addr, 0, &msg, NULL, IPMI_REQ_PRIORITY, IPMI_REQ_MAX_RETRIES, IPMI_REQ_RETRY_TIME_MS);
    if (ret)
    {
        printk("warning: send request fail, led broken\n");
        mutex_unlock(&ott_ipmi_mutex);
        return ret;
    }
    rsp_len = para->rsp.data_len;// Set length at call ex:ipmi_info.rsp.data_len = 2;
    timeout = (timeout <= 0) ? OTT_IPMI_XFER_DEFAULT_TIMEOUT : timeout;

    g_req_netfn = msg.netfn;
    g_req_cmd = msg.cmd;
    ret = down_timeout(&g_ipmi_msg_sem, timeout*HZ);//wait for recv_msg
    if(ret)
    {
        printk("down_interruptible error timeout ret = %d\n", ret);
        mutex_unlock(&ott_ipmi_mutex);
        return ret;
    }
    para->rsp.netfn = g_bmc_recv_msg.msg.netfn;
    para->rsp.cmd = g_bmc_recv_msg.msg.cmd;
    para->rsp.data_len = g_bmc_recv_msg.msg.data_len;

    ret = memcpy_s(para->rsp.data, rsp_len, g_bmc_recv_msg.msg.data, para->rsp.data_len);
    if(ret)
    {
        printk("memcpy_s g_bmc_recv_msg.msg.data to para->rsp.data error\n");
        printk("req:para->req.netfn = %d, para->req.cmd = %d, para->req.data_len = %d, para->req.data = %d\n", para->req.netfn, para->req.cmd, para->req.data_len, para->req.data[0]);
        printk("rsp:para->rsp.netfn = %d, para->rsp.cmd = %d, para->rsp.data_len = %d, para->rsp.data = %d\n", para->rsp.netfn, para->rsp.cmd, para->rsp.data_len, para->rsp.data[0]);
        mutex_unlock(&ott_ipmi_mutex);
        return ret;
    }
    mutex_unlock(&ott_ipmi_mutex);
    return ret;
}

/************************* sensor *************************/
int drv_get_sensor_temp_input_from_bmc(unsigned int index, long *value)
{
    int ipmi_ret;
    char value_get = 0;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[1] = {1};
    unsigned char ack_data[2];
    unsigned int loglevel = loglevel_bmc;
    index++;
    data[0]=(unsigned char)(index);

    memset_s(ack_data,2,0,2);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_GET_THERMAL_DATA;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = 2;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);
    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get temp info fail! ipmi_ret:0x%x,index:%d.\n",
            __LINE__,ipmi_ret,index);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get temp info fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d.\n",
            __LINE__,ipmi_ret,ack_data[0],index);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get temp info fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d.\n",
            __LINE__,ipmi_ret,ack_data[0],index);
        return -EIO;
    }
    value_get = (int)(ack_data[1]);
    *value = value_get * 1000;
    return 0;
}
EXPORT_SYMBOL_GPL(drv_get_sensor_temp_input_from_bmc);


enum ipmi_app_sensor_attribute_index
{
    LM25066_IPMI_INDEX  = 1,
    MP2976_IPMI_INDEX   = 2,
    MP2973_IPMI_INDEX   = 3,
    MAX34461_IPMI_INDEX = 4,
};

typedef struct IN_SENSOR_TBL
{
    int index;
    int in_index;
    int type;
}InSensorTbl;
enum ipmi_app_in_attribute_num
{
    IN1_INPUT = 1,
    IN2_INPUT,
    IN3_INPUT,
    IN4_INPUT,
    IN5_INPUT,
    IN6_INPUT,
    IN7_INPUT,
    IN8_INPUT,
    IN9_INPUT,
    IN10_INPUT,
    IN11_INPUT,
    IN12_INPUT,
    IN13_INPUT,
    IN14_INPUT,
    IN15_INPUT,
    IN16_INPUT,
};
#define ipmitool_raw_get_in_input       1
#define ipmitool_raw_get_curr_input     2
static InSensorTbl in_tbl[IN_TOTAL_NUM] = 
{
    {.index = LM25066_IPMI_INDEX,  .in_index = IN1_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = LM25066_IPMI_INDEX,  .in_index = IN3_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MP2976_IPMI_INDEX,   .in_index = IN1_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MP2976_IPMI_INDEX,   .in_index = IN2_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MP2976_IPMI_INDEX,   .in_index = IN3_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MP2973_IPMI_INDEX,   .in_index = IN1_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MP2973_IPMI_INDEX,   .in_index = IN2_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MP2973_IPMI_INDEX,   .in_index = IN3_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN1_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN2_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN3_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN4_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN5_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN6_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN7_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN8_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN9_INPUT,  .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN10_INPUT, .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN11_INPUT, .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN12_INPUT, .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN13_INPUT, .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN14_INPUT, .type = ipmitool_raw_get_in_input},
    {.index = MAX34461_IPMI_INDEX, .in_index = IN15_INPUT, .type = ipmitool_raw_get_in_input},
};
int drv_get_sensor_in_input_from_bmc(unsigned int index, int pmbus_command, long *value)
{
    int ipmi_ret;
    int value_get = 0;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[3] = {1};
    unsigned char ack_data[5];
    unsigned int loglevel = loglevel_bmc;
    data[0]=(unsigned char)(in_tbl[index].index);
    data[1]=(unsigned char)(in_tbl[index].in_index);
    data[2]=(unsigned char)(in_tbl[index].type);

    memset_s(ack_data,5,0,5);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_GET_SENSOR_DATA;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = 5;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);
    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get temp info fail! ipmi_ret:0x%x,index:%d,pmbus_command:%x.\n",
            __LINE__,ipmi_ret,index,pmbus_command);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get temp info fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,pmbus_command:%x.\n",
            __LINE__,ipmi_ret,ack_data[0],index,pmbus_command);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get temp info fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,pmbus_command:%x.\n",
            __LINE__,ipmi_ret,ack_data[0],index,pmbus_command);
        return -EIO;
    }
    value_get = (((int)(ack_data[1])) << 24) + (((int)(ack_data[2])) << 16) + (((int)(ack_data[3])) << 8) + (int)(ack_data[4]);
    *value = value_get;
    return 0;
}
EXPORT_SYMBOL_GPL(drv_get_sensor_in_input_from_bmc);


typedef struct CURR_SENSOR_TBL
{
    int index;
    int curr_index;
    int type;
}CurrSensorTbl;

enum ipmi_app_curr_attribute_num
{
    CURR1_INPUT = 1,
    CURR2_INPUT,
    CURR3_INPUT,
};

static CurrSensorTbl curr_tbl[CURR_TOTAL_NUM] = 
{
    {.index = LM25066_IPMI_INDEX, .curr_index = CURR1_INPUT, .type = ipmitool_raw_get_curr_input},
    {.index =  MP2976_IPMI_INDEX, .curr_index = CURR1_INPUT, .type = ipmitool_raw_get_curr_input},
    {.index =  MP2976_IPMI_INDEX, .curr_index = CURR2_INPUT, .type = ipmitool_raw_get_curr_input},
    {.index =  MP2976_IPMI_INDEX, .curr_index = CURR3_INPUT, .type = ipmitool_raw_get_curr_input},
    {.index =  MP2973_IPMI_INDEX, .curr_index = CURR1_INPUT, .type = ipmitool_raw_get_curr_input},
    {.index =  MP2973_IPMI_INDEX, .curr_index = CURR2_INPUT, .type = ipmitool_raw_get_curr_input},
    {.index =  MP2973_IPMI_INDEX, .curr_index = CURR3_INPUT, .type = ipmitool_raw_get_curr_input},
};

int drv_get_sensor_curr_input_from_bmc(unsigned int index, int pmbus_command, long *value)
{
    int ipmi_ret;
    int value_get = 0;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[3] = {1};
    unsigned char ack_data[5];
    unsigned int loglevel = loglevel_bmc;

    data[0]=(unsigned char)(curr_tbl[index].index);
    data[1]=(unsigned char)(curr_tbl[index].curr_index);
    data[2]=(unsigned char)(curr_tbl[index].type);

    memset_s(ack_data,5,0,5);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_GET_SENSOR_DATA;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = 5;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);
    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get temp info fail! ipmi_ret:0x%x,index:%d,pmbus_command:%x.\n",
            __LINE__,ipmi_ret,index,pmbus_command);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get temp info fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,pmbus_command:%x.\n",
            __LINE__,ipmi_ret,ack_data[0],index,pmbus_command);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get temp info fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,pmbus_command:%x.\n",
            __LINE__,ipmi_ret,ack_data[0],index,pmbus_command);
        return -EIO;
    }
    value_get = (((int)(ack_data[1])) << 24) + (((int)(ack_data[2])) << 16) + (((int)(ack_data[3])) << 8) + (int)(ack_data[4]);
    *value = value_get;
    return 0;
}
EXPORT_SYMBOL_GPL(drv_get_sensor_curr_input_from_bmc);

/************************* fan *************************/
#if 0
bool drv_get_wind_from_bmc(unsigned int fan_index, unsigned int *wind)
{
    int ipmi_ret;
    unsigned int fanIdx = 0;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[3] = {1};
    unsigned char ack_data[2];
    unsigned int loglevel = loglevel_bmc;
    data[0]=(unsigned char)(fan_index);
    data[1]=(unsigned char)(fanIdx);
    data[2]=(unsigned char)(eeprom_fan_direction);

    memset_s(ack_data,2,0,2);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_FAN_GET_EEPROM;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = 2;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get fan speed fail! ipmi_ret:0x%x,fan_index:%d.\n", __LINE__,ipmi_ret,fan_index);
        return false;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get fan speed fail! ipmi_ret:0x%x,ack_data[0]:0x%x,fan_index:%d.\n", __LINE__,ipmi_ret,ack_data[0],fan_index);
        return false;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get fan speed fail! ipmi_ret:0x%x,ack_data[0]:0x%x,fan_index:%d.\n", __LINE__,ipmi_ret,ack_data[0],fan_index);
        return false;
    }

    *wind = (unsigned int)(ack_data[1]);
    if(*wind > 1)
    {
        return false;
    }

    return true;
}
EXPORT_SYMBOL_GPL(drv_get_wind_from_bmc);
#endif

ssize_t drv_get_model_name_from_bmc(unsigned int index, char *buf)
{
    int ipmi_ret;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[] = {(unsigned char)index+1, 0, eeprom_fan_model_name};
    unsigned char ack_data[PMBUS_MFR_MAX_LEN];
    unsigned char model_name[PMBUS_MFR_MAX_LEN];
    unsigned int loglevel = loglevel_bmc;

    memset_s(ack_data, PMBUS_MFR_MAX_LEN, 0, PMBUS_MFR_MAX_LEN);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_FAN_GET_EEPROM;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = PMBUS_MFR_MAX_LEN;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get fan model_name fail! ipmi_ret:0x%x,index:%d.\n",__LINE__,ipmi_ret,index);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get fan model_name fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d.\n",__LINE__,ipmi_ret,ack_data[0],index);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get fan model_name fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,,ipmi_info.rsp.netfn:0x%x,ipmi_info.req.cmd:0x%x.\n",__LINE__,ipmi_ret,ack_data[0],index,ipmi_info.rsp.netfn,ipmi_info.req.cmd);
        return -EIO;
    }

    if(memcpy_s(model_name, PMBUS_MFR_MAX_LEN-1, (unsigned char*)&ack_data[1], PMBUS_MFR_MAX_LEN-1) != 0)
    {
        return -ENOMEM;
    }
    model_name[PMBUS_MFR_MAX_LEN-1]='\0';

    return sprintf_s(buf, PAGE_SIZE, "%s\n", model_name);

}
EXPORT_SYMBOL_GPL(drv_get_model_name_from_bmc);

ssize_t drv_get_sn_from_bmc(unsigned int index, char *buf)
{
    int ipmi_ret;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[] = {(unsigned char)index+1, 0, eeprom_fan_serial_number};
    unsigned char ack_data[PMBUS_MFR_MAX_LEN];
    unsigned char serial_number[PMBUS_MFR_MAX_LEN];
    unsigned int loglevel = loglevel_bmc;

    memset_s(ack_data, PMBUS_MFR_MAX_LEN, 0, PMBUS_MFR_MAX_LEN);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_FAN_GET_EEPROM;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = PMBUS_MFR_MAX_LEN;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get fan serial_number fail! ipmi_ret:0x%x,index:%d.\n",__LINE__,ipmi_ret,index);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get fan serial_number fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d.\n",__LINE__,ipmi_ret,ack_data[0],index);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get fan serial_number fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,,ipmi_info.rsp.netfn:0x%x,ipmi_info.req.cmd:0x%x.\n",__LINE__,ipmi_ret,ack_data[0],index,ipmi_info.rsp.netfn,ipmi_info.req.cmd);
        return -EIO;
    }

    if(memcpy_s(serial_number, PMBUS_MFR_MAX_LEN-1, (unsigned char*)&ack_data[1], PMBUS_MFR_MAX_LEN-1) != 0)
    {
        return -ENOMEM;
    }
    serial_number[PMBUS_MFR_MAX_LEN-1]='\0';

    return sprintf_s(buf, PAGE_SIZE, "%s\n", serial_number);
}
EXPORT_SYMBOL_GPL(drv_get_sn_from_bmc);

ssize_t drv_get_vendor_from_bmc(unsigned int index, char *buf)
{
    int ipmi_ret;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[] = {(unsigned char)index+1, 0, eeprom_fan_vendor};
    unsigned char ack_data[PMBUS_MFR_MAX_LEN];
    unsigned char vendor[PMBUS_MFR_MAX_LEN];
    unsigned int loglevel = loglevel_bmc;

    memset_s(ack_data, PMBUS_MFR_MAX_LEN, 0, PMBUS_MFR_MAX_LEN);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_FAN_GET_EEPROM;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = PMBUS_MFR_MAX_LEN;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get fan vendor fail! ipmi_ret:0x%x,index:%d.\n",__LINE__,ipmi_ret,index);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get fan vendor fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d.\n",__LINE__,ipmi_ret,ack_data[0],index);
        return -EIO;
    }
 
    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get fan vendor fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,,ipmi_info.rsp.netfn:0x%x,ipmi_info.req.cmd:0x%x.\n",__LINE__,ipmi_ret,ack_data[0],index,ipmi_info.rsp.netfn,ipmi_info.req.cmd);
        return -EIO;
    }

    if(memcpy_s(vendor, PMBUS_MFR_MAX_LEN-1, (unsigned char*)&ack_data[1], PMBUS_MFR_MAX_LEN-1) != 0)
    {
        return -ENOMEM;
    }
    vendor[PMBUS_MFR_MAX_LEN-1]='\0';

    return sprintf_s(buf, PAGE_SIZE, "%s\n", vendor);

}
EXPORT_SYMBOL_GPL(drv_get_vendor_from_bmc);

ssize_t drv_get_hw_version_from_bmc(unsigned int index, char *buf)
{
    int ipmi_ret;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[] = {(unsigned char)index+1, 0, eeprom_fan_hw_version};
    unsigned char ack_data[PMBUS_MFR_MAX_LEN];
    unsigned char hw_version[PMBUS_MFR_MAX_LEN];
    unsigned int loglevel = loglevel_bmc;

    memset_s(ack_data, PMBUS_MFR_MAX_LEN, 0, PMBUS_MFR_MAX_LEN);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_FAN_GET_EEPROM;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = PMBUS_MFR_MAX_LEN;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get fan hw_version fail! ipmi_ret:0x%x,index:%d.\n",__LINE__,ipmi_ret,index);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get fan hw_version fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d.\n",__LINE__,ipmi_ret,ack_data[0],index);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get fan hw_version fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,,ipmi_info.rsp.netfn:0x%x,ipmi_info.req.cmd:0x%x.\n",__LINE__,ipmi_ret,ack_data[0],index,ipmi_info.rsp.netfn,ipmi_info.req.cmd);
        return -EIO;
    }

    if(memcpy_s(hw_version, PMBUS_MFR_MAX_LEN-1, (unsigned char*)&ack_data[1], PMBUS_MFR_MAX_LEN-1) != 0)
    {
        return -ENOMEM;
    }
    hw_version[PMBUS_MFR_MAX_LEN-1]='\0';

    return sprintf_s(buf, PAGE_SIZE, "%s\n", hw_version);
}
EXPORT_SYMBOL_GPL(drv_get_hw_version_from_bmc);

ssize_t drv_get_part_number_from_bmc(unsigned int index, char *buf)
{
    int ipmi_ret;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[] = {(unsigned char)index+1, 0, eeprom_fan_part_number};
    unsigned char ack_data[PMBUS_MFR_MAX_LEN];
    unsigned char part_number[PMBUS_MFR_MAX_LEN];
    unsigned int loglevel = loglevel_bmc;

    memset_s(ack_data, PMBUS_MFR_MAX_LEN, 0, PMBUS_MFR_MAX_LEN);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_FAN_GET_EEPROM;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = PMBUS_MFR_MAX_LEN;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get fan part_number fail! ipmi_ret:0x%x,index:%d.\n",__LINE__,ipmi_ret,index);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get fan part_number fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d.\n",__LINE__,ipmi_ret,ack_data[0],index);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get fan part_number fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,,ipmi_info.rsp.netfn:0x%x,ipmi_info.req.cmd:0x%x.\n",__LINE__,ipmi_ret,ack_data[0],index,ipmi_info.rsp.netfn,ipmi_info.req.cmd);
        return -EIO;
    }

    if(memcpy_s(part_number, PMBUS_MFR_MAX_LEN-1, (unsigned char*)&ack_data[1], PMBUS_MFR_MAX_LEN-1) != 0)
    {
        return -ENOMEM;
    }
    part_number[PMBUS_MFR_MAX_LEN-1]='\0';

    return sprintf_s(buf, PAGE_SIZE, "%s\n", part_number);
}
EXPORT_SYMBOL_GPL(drv_get_part_number_from_bmc);

#if 0
ssize_t drv_get_speed_from_bmc(unsigned int slot_id, unsigned int fan_id, unsigned int *speed)
{
    int ipmi_ret;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[2] = {1};
    unsigned char ack_data[5];
    unsigned int loglevel = loglevel_bmc;
    data[0]=(unsigned char)(slot_id);
    data[1]=(unsigned char)(fan_id);

    memset_s(ack_data,5,0,5);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_GET_FAN_SPEED;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = 5;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get fan speed fail! ipmi_ret:0x%x,slot_id:%d,fan_id:%d.\n",
            __LINE__,ipmi_ret,slot_id,fan_id);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get fan speed fail! ipmi_ret:0x%x,ack_data[0]:0x%x,slot_id:%d,fan_id:%d.\n",
            __LINE__,ipmi_ret,ack_data[0],slot_id,fan_id);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get fan speed fail! ipmi_ret:0x%x,ack_data[0]:0x%x,slot_id:%d,fan_id:%d.\n",
            __LINE__,ipmi_ret,ack_data[0],slot_id,fan_id);
        return -EIO;
    }
    *speed = (((int)(ack_data[1])) << 24) + (((int)(ack_data[2])) << 16) + (((int)(ack_data[3])) << 8) + (int)(ack_data[4]);

    return 0;
}
EXPORT_SYMBOL_GPL(drv_get_speed_from_bmc);

bool drv_get_pwm_from_bmc(unsigned int fan_id, int *pwm)
{
    int ipmi_ret;
    unsigned int fanIdx = 0;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[2] = {1};
    unsigned char ack_data[2];
    unsigned int loglevel = loglevel_bmc;
    data[0]=(unsigned char)(fan_id);
    data[1]=(unsigned char)(fanIdx);

    memset_s(ack_data,2,0,2);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_GET_FAN_PWM;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = 2;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get fan speed fail! ipmi_ret:0x%x,fan_id:%d.\n",
            __LINE__,ipmi_ret,fan_id);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get fan speed fail! ipmi_ret:0x%x,ack_data[0]:0x%x,fan_id:%d.\n",
            __LINE__,ipmi_ret,ack_data[0],fan_id);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get fan speed fail! ipmi_ret:0x%x,ack_data[0]:0x%x,fan_id:%d.\n",
            __LINE__,ipmi_ret,ack_data[0],fan_id);
        return -EIO;
    }
    *pwm = (int)(ack_data[1]) / 2;

    return true;
}
EXPORT_SYMBOL_GPL(drv_get_pwm_from_bmc);

bool drv_set_pwm_from_bmc(unsigned int fan_id, int pwm)
{
    int ipmi_ret;
    unsigned int fanIdx = 0;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[3] = {1};
    unsigned char ack_data[2];
    unsigned int loglevel = loglevel_bmc;
    data[0]=(unsigned char)(fan_id);
    data[1]=(unsigned char)(fanIdx);
    data[2]=(unsigned char)(pwm);

    memset_s(ack_data,2,0,2);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_SET_FAN_PWM;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = 2;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get fan speed fail! ipmi_ret:0x%x,fan_id:%d.\n",
            __LINE__,ipmi_ret,fan_id);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get fan speed fail! ipmi_ret:0x%x,ack_data[0]:0x%x,fan_id:%d.\n",
            __LINE__,ipmi_ret,ack_data[0],fan_id);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get fan speed fail! ipmi_ret:0x%x,ack_data[0]:0x%x,fan_id:%d.\n",
            __LINE__,ipmi_ret,ack_data[0],fan_id);
        return -EIO;
    }

    return true;
}
EXPORT_SYMBOL_GPL(drv_set_pwm_from_bmc);
#endif

/************************* psu *************************/
int drv_get_mfr_info_from_bmc(unsigned int psu_index, u8 pmbus_command, char *buf)
{
    int ipmi_ret;
    int psu_id = 0;
    int value = 0;
    int psu_present = 0;
    int psu_alarm = 0;
    int ack_data_len = 0;
    int ack_data_handle = 0;
    int multiplier = 1000;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[3] = {0};
    unsigned char ack_data[PMBUS_MFR_MAX_LEN] = {0};
    unsigned int loglevel = loglevel_bmc;
    data[0]=(unsigned char)(psu_index+1);
    data[1]=(unsigned char)(psu_id);

    memset_s(ack_data,PMBUS_MFR_MAX_LEN,0,PMBUS_MFR_MAX_LEN);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = PMBUS_MFR_MAX_LEN;

    switch(pmbus_command)
    {
        case PMBUS_STATUS_WORD:
            // PMBUS_IIN_OC_WARN_LIMIT and PMBUS_VIN_OV_WARN_LIMIT are hard code since PSU not support
            // But we still need to check PSU status. Return NA if get failed
            ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            data[2]=(unsigned char)(POWER_STATUS);
            ack_data_handle = ACK_DATA_RESP_BITMAP;
            break;
        case PMBUS_STATUS_INPUT:
            ipmi_info.req.cmd = IPMI_GET_PSU_ALARM_DATA;
            data[2]=(unsigned char)(POWER_ALARM_INPUT);
            ack_data_handle = ACK_DATA_RESP_PRESENT;
            break;
        case PMBUS_POUT_MAX:
            return sprintf_s(buf, PAGE_SIZE, "650.000");
            // ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            // data[2]=(unsigned char)(POWER_POUT_MAX);
            break;
        case PMBUS_READ_TEMPERATURE_1:
            ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            data[2]=(unsigned char)(POWER_TEMPERATURE_1);
            ack_data_handle = ACK_DATA_RESP_FLOAT;
            break;
        case PMBUS_READ_TEMPERATURE_2:
            ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            data[2]=(unsigned char)(POWER_TEMPERATURE_2);
            ack_data_handle = ACK_DATA_RESP_FLOAT;
            break;
        case PMBUS_READ_VIN:
            ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            data[2]=(unsigned char)(POWER_INPUT_VOLTAGE);
            ack_data_handle = ACK_DATA_RESP_FLOAT;
            break;
        case PMBUS_READ_VOUT:
            ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            data[2]=(unsigned char)(POWER_OUTPUT_VOLTAGE);
            ack_data_handle = ACK_DATA_RESP_FLOAT;
            break;
        case PMBUS_READ_IIN:
            ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            data[2]=(unsigned char)(POWER_INPUT_CURRENT);
            ack_data_handle = ACK_DATA_RESP_FLOAT;
            break;
        case PMBUS_READ_IOUT:
            ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            data[2]=(unsigned char)(POWER_OUTPUT_CURRENT);
            ack_data_handle = ACK_DATA_RESP_FLOAT;
            break;
        case PMBUS_READ_PIN:
            ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            data[2]=(unsigned char)(POWER_INPUT_POWER);
            ack_data_handle = ACK_DATA_RESP_FLOAT_DIV;
            break;
        case PMBUS_READ_POUT:
            ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            data[2]=(unsigned char)(POWER_OUTPUT_POWER);
            ack_data_handle = ACK_DATA_RESP_FLOAT_DIV;
            break;
        case PMBUS_READ_FAN_SPEED_1:
            ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            data[2]=(unsigned char)(POWER_FAN_SPEED);
            break;
        case PMBUS_MFR_SERIAL:
            ack_data_len = PMBUS_MFR_MAX_LEN-1;//PMBUS_MFR_SERIAL_LEN + 1;
            ipmi_info.req.cmd = IPMI_PSU_GET_EEPROM;
            data[2]=(unsigned char)(eeprom_psu_serial_number);
            ack_data_handle = ACK_DATA_RESP_CHAR;
            break;
        case PMBUS_MFR_ID:
            ack_data_len = PMBUS_MFR_MAX_LEN-1;//PMBUS_MFR_ID_LEN + 1;
            ipmi_info.req.cmd = IPMI_PSU_GET_EEPROM;
            data[2]=(unsigned char)(eeprom_psu_vendor);
            ack_data_handle = ACK_DATA_RESP_CHAR;
            break;
        case PMBUS_MFR_MODEL:
            ack_data_len = PMBUS_MFR_MAX_LEN-1;//PMBUS_MFR_MODEL_LEN + 1;
            ipmi_info.req.cmd = IPMI_PSU_GET_EEPROM;
            data[2]=(unsigned char)(eeprom_psu_model_name);
            ack_data_handle = ACK_DATA_RESP_CHAR;
            break;
        case PMBUS_MFR_REVISION:
            ack_data_len = PMBUS_MFR_MAX_LEN-1;//PMBUS_MFR_REVISION_LEN + 1;
            ipmi_info.req.cmd = IPMI_PSU_GET_EEPROM;
            data[2]=(unsigned char)(eeprom_psu_hardware_version);
            ack_data_handle = ACK_DATA_RESP_CHAR;
            break;
        case PMBUS_MFR_DATE:
            ack_data_len = PMBUS_MFR_MAX_LEN-1;//PMBUS_MFR_DATE_LEN + 1;
            ipmi_info.req.cmd = IPMI_PSU_GET_EEPROM;
            data[2]=(unsigned char)(eeprom_psu_date);
            ack_data_handle = ACK_DATA_RESP_CHAR;
            break;
        case PMBUS_MFR_PSU_VIN_TYPE:
            ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            data[2]=(unsigned char)(POWER_VIN_TYPE);
            break;
        case PMBUS_READ_LED_STATUS:
            ipmi_info.req.cmd = IPMI_GET_PSU_ATTRIBUTE_DATA;
            data[2]=(unsigned char)(POWER_LED_STATUS);
            break;
        default:
            if(sprintf_s(buf, PAGE_SIZE, "%x", pmbus_command) < 0)
            {
                return -ENOMEM;
            }
            return 0;
            break;
    }

    ipmi_ret = ott_ipmi_xfer(&ipmi_info);
    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%d, get fan info fail! ipmi_ret:0x%x,psu_index:%d,pmbus_command:%x.\n",
            __LINE__,ipmi_ret,psu_index,pmbus_command);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%d, get fan info fail! ipmi_ret:0x%x,ack_data[0]:0x%x,psu_index:%d,pmbus_command:%x.\n",
            __LINE__,ipmi_ret,ack_data[0],psu_index,pmbus_command);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%d, get fan info fail! ipmi_ret:0x%x,ack_data[0]:0x%x,psu_index:%d,pmbus_command:%x.\n",
            __LINE__,ipmi_ret,ack_data[0],psu_index,pmbus_command);
        return -EIO;
    }

    if(ack_data_handle == ACK_DATA_RESP_CHAR)
    {
        int i = 0;
        char value_rec[ack_data_len];
        for(i=0; i<ack_data_len; i++)
        {
            value_rec[i] = (int)(ack_data[i+1]);
        }

        if(memcpy_s(buf,PAGE_SIZE,value_rec,sizeof(value_rec)) != 0)
        {
            return -ENOMEM;
        }

        return 0;
    }
    else if(ack_data_handle == ACK_DATA_RESP_INT)
    {
        char value = (int)(ack_data[2]);
        if(memcpy_s(buf,PAGE_SIZE,&value,sizeof(value)) != 0)
        {
            return -ENOMEM;
        }
        return 0;
    }
    value = (((int)(ack_data[1])) << 24) + (((int)(ack_data[2])) << 16) + (((int)(ack_data[3])) << 8) + (int)(ack_data[4]);
    if(ack_data_handle == ACK_DATA_RESP_FLOAT)
    {
        if(sprintf_s(buf, PAGE_SIZE, "%d.%03d", value/multiplier,value%multiplier) < 0)
        {
            return -ENOMEM;
        }
        return 0;
    }
    else if(ack_data_handle == ACK_DATA_RESP_FLOAT_DIV)
    {
        value = value/multiplier;
        if(sprintf_s(buf, PAGE_SIZE, "%d.%03d", value/multiplier,value%multiplier) < 0)
        {
            return -ENOMEM;
        }
        return 0;
    }
    else if(ack_data_handle == ACK_DATA_RESP_PRESENT)
    {
        if(value)
        {
            psu_present = IPMI_PSU_ABSENT;
        }
        else
        {
            psu_present = IPMI_PSU_PRESENT;
        }
        if(sprintf_s(buf, PAGE_SIZE, "%d", psu_present) < 0)
        {
            return -ENOMEM;
        }
        return 0;
    }
    else if(ack_data_handle == ACK_DATA_RESP_BITMAP)
    {
        psu_alarm = value & 0x1fff;
        return sprintf_s(buf, PAGE_SIZE, "%d", psu_alarm);
    }

    if(sprintf_s(buf, PAGE_SIZE, "%d", value) < 0)
    {
        return -ENOMEM;
    }

    return 0;
}
EXPORT_SYMBOL_GPL(drv_get_mfr_info_from_bmc);

/************************* avs *************************/
#define POWER_CHIP_WORK_STATUS      1
#define POWER_CHIP_PMBUS_STATUS     2
#define POWER_CHIP_CURRENT_STATUS   3
ssize_t drv_fmea_get_work_status_from_bmc(unsigned int index, char *buf, char *plt)
{
    int ipmi_ret;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[] = {(unsigned char)(index+2), POWER_CHIP_WORK_STATUS};
    unsigned char ack_data[5];
    unsigned char work_status[6];
    unsigned int loglevel = loglevel_bmc;

    memset_s(ack_data,5,0,5);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_CMD_GET_FMEA_STATUS;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = 5;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%s:%d  fail! ipmi_ret:0x%x,index:%d.\n",__func__,__LINE__,ipmi_ret,index);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%s:%d  fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d.\n",__func__,__LINE__,ipmi_ret,ack_data[0],index);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%s:%d  fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,,ipmi_info.rsp.netfn:0x%x,ipmi_info.req.cmd:0x%x.\n",__func__,__LINE__,ipmi_ret,ack_data[0],index,ipmi_info.rsp.netfn,ipmi_info.req.cmd);
        return -EIO;
    }

    if(memcpy_s(work_status,4,(unsigned char*)&ack_data[1],4) != 0)
    {
        return -ENOMEM;
    }
    work_status[5]='\0';

    return sprintf_s(buf, PAGE_SIZE, "%s\n", work_status);
}
EXPORT_SYMBOL_GPL(drv_fmea_get_work_status_from_bmc);

ssize_t drv_fmea_get_current_status_from_bmc(unsigned int index, char *buf, char *plt)
{
    int ipmi_ret;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[] = {(unsigned char)(index+2), POWER_CHIP_CURRENT_STATUS};
    unsigned char ack_data[5];
    unsigned char current_status[6];
    unsigned int loglevel = loglevel_bmc;

    memset_s(ack_data,5,0,5);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_CMD_GET_FMEA_STATUS;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = 5;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%s:%d  fail! ipmi_ret:0x%x,index:%d.\n",__func__,__LINE__,ipmi_ret,index);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%s:%d  fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d.\n",__func__,__LINE__,ipmi_ret,ack_data[0],index);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%s:%d  fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,,ipmi_info.rsp.netfn:0x%x,ipmi_info.req.cmd:0x%x.\n",__func__,__LINE__,ipmi_ret,ack_data[0],index,ipmi_info.rsp.netfn,ipmi_info.req.cmd);
        return -EIO;
    }

    if(memcpy_s(current_status,4,(unsigned char*)&ack_data[1],4) != 0)
    {
        return -ENOMEM;
    }
    current_status[5]='\0';

    return sprintf_s(buf, PAGE_SIZE, "%s\n", current_status);
}
EXPORT_SYMBOL_GPL(drv_fmea_get_current_status_from_bmc);

ssize_t drv_fmea_get_pmbus_status_from_bmc(unsigned int index, char *buf, char *plt)
{
    int ipmi_ret;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char data[] = {(unsigned char)(index+2), POWER_CHIP_PMBUS_STATUS};
    unsigned char ack_data[5];
    unsigned char pmbus_status[6];
    unsigned int loglevel = loglevel_bmc;

    memset_s(ack_data,5,0,5);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_CMD_GET_FMEA_STATUS;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = 5;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("%s:%d  fail! ipmi_ret:0x%x,index:%d.\n",__func__,__LINE__,ipmi_ret,index);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("%s:%d  fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d.\n",__func__,__LINE__,ipmi_ret,ack_data[0],index);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("%s:%d  fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,,ipmi_info.rsp.netfn:0x%x,ipmi_info.req.cmd:0x%x.\n",__func__,__LINE__,ipmi_ret,ack_data[0],index,ipmi_info.rsp.netfn,ipmi_info.req.cmd);
        return -EIO;
    }

    if(memcpy_s(pmbus_status,4,(unsigned char*)&ack_data[1],4) != 0)
    {
        return -ENOMEM;
    }
    pmbus_status[5]='\0';

    return sprintf_s(buf, PAGE_SIZE, "%s\n", pmbus_status);
}
EXPORT_SYMBOL_GPL(drv_fmea_get_pmbus_status_from_bmc);

#if 0
ssize_t drv_fmea_get_mfr_id_from_bmc(unsigned int index, int *value)
{
    int ipmi_ret;
    ott_ipmi_xfer_info_s ipmi_info = {0};
    unsigned char ack_data[3];
    unsigned char data[] = {(unsigned char)index, 4};
    unsigned int loglevel = loglevel_bmc;

    memset_s(ack_data,3,0,3);
    ipmi_info.req.netfn = IPMI_NETFN;
    ipmi_info.req.cmd = IPMI_CMD_GET_FMEA_STATUS;
    ipmi_info.req.data = data;
    ipmi_info.req.data_len = sizeof(data);

    ipmi_info.rsp.data = ack_data;
    ipmi_info.rsp.data_len = 3;
    ipmi_ret = ott_ipmi_xfer(&ipmi_info);

    if(ipmi_ret != 0)
    {
        SENSOR_DEBUG("Fail! ipmi_ret:0x%x,index:%d.\n",ipmi_ret,index);
        return (unsigned int)ipmi_ret;
    }

    if(ack_data[0] != 0)
    {
        SENSOR_DEBUG("Fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d.\n",ipmi_ret,ack_data[0],index);
        return -EIO;
    }

    if((ipmi_info.rsp.netfn != (ipmi_info.req.netfn + 1)) || (ipmi_info.rsp.cmd != ipmi_info.req.cmd))
    {
        SENSOR_DEBUG("Fail! ipmi_ret:0x%x,ack_data[0]:0x%x,index:%d,,ipmi_info.rsp.netfn:0x%x,ipmi_info.req.cmd:0x%x.\n",ipmi_ret,ack_data[0],index,ipmi_info.rsp.netfn,ipmi_info.req.cmd);
        return -EIO;
    }

    if(memcpy_s((char *)value,2,(unsigned char*)&ack_data[1],2) != 0)
    {
        return -ENOMEM;
    }

    return 0;
}
EXPORT_SYMBOL_GPL(drv_fmea_get_mfr_id_from_bmc);
#endif

/************************* bmc *************************/
static int sys_cpld_read(u8 reg)
{
    return lpc_read_cpld( LCP_DEVICE_ADDRESS1, reg);
}

bool ipmi_bmc_is_ok(void)
{
    int val;
    int bmc_enable = -1;
    int bmc_present = -1;
    int bmc_heart = -1;

    val = sys_cpld_read(SYS_CPLD_BMC_EN_REG);
    if(val < 0)
        return false;
    bmc_enable = (val >> BMC_ENABLE_OFFSET) & BMC_ENABLE_MASK;

    val = sys_cpld_read(SYS_CPLD_BMC_STATUS_REG);
    if(val < 0)
        return false;
    bmc_present = (val >> BMC_PRESENT_OFFSET) & BMC_PRESENT_MASK;
    bmc_heart =  (val >> BMC_HEART_OFFSET) & BMC_HEART_MASK;

    if((bmc_enable == BMC_EN_ENABLE) && (bmc_present == BMC_PRESENT) && (bmc_heart == BMC_HEART_FRQ_2HZ))
    {
        return true;
    }

    return false;
}
EXPORT_SYMBOL_GPL(ipmi_bmc_is_ok);


static int __init sys_impi_init(void)
{
    int ret = 0;
    sema_init(&g_ipmi_msg_sem, 0);
    ret = ipmi_create_user(IPMI_INTF_NUM, &ipmi_hndlrs, NULL, &g_ipmi_mh_user);
    if(ret)
    {
        printk("error:ipmi_create_user failed!!!\n");
    }

    return 0;
}
static void __exit sys_impi_exit(void)
{
    ipmi_destroy_user(g_ipmi_mh_user);
    return;
}

MODULE_AUTHOR("Zhonghu Zhang <zhonghu_zhang@accton.com>");
MODULE_DESCRIPTION("sys ipmi");
MODULE_VERSION("0.0.0.1");
MODULE_LICENSE("GPL");
module_init(sys_impi_init);
module_exit(sys_impi_exit);