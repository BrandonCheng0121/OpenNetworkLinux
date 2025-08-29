#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include "cpld_i2c_driver.h"
#include "cpld_lpc_driver.h"
#include "fpga_driver.h"
#include "switch_transceiver_driver.h"
#include "switch_optoe.h"

#define DRVNAME "drv_transceiver_driver"
#define SWITCH_TRANSCEIVER_DRIVER_VERSION "0.0.1"
#define CPLD_NUM 2

unsigned int loglevel = 0x0;
static struct platform_device *drv_transceiver_device;
int port_cpld_addr[2] = {
    0x62,
    0x64
};

struct eeprom_parse_value{
    int temperature;
    int voltage;
    int tx_bias[8];
    int tx_power[8];
    int rx_power[8];
};

int calc_temperature(unsigned int transceiver_index, int offset)
{
    int val = 0;
    unsigned char data = 0;

    optoe_bin_read_by_index(transceiver_index, &data, offset, 1);
    val += (data * 100);

    optoe_bin_read_by_index(transceiver_index, &data, offset+1, 1);
    val += ((data * 100)/256);

    return val;
}

int calc_voltage(unsigned int transceiver_index, int offset)
{
    int val = 0;
    unsigned char data = 0;

    optoe_bin_read_by_index(transceiver_index, &data, offset, 1);
    val += (data * 256);

    optoe_bin_read_by_index(transceiver_index, &data, offset+1, 1);
    val += data;

    return val;
}

int calc_bias(unsigned int transceiver_index, int offset)
{
    int val = 0;
    unsigned char data = 0;

    optoe_bin_read_by_index(transceiver_index, &data, offset, 1);
    val += (data * 256);

    optoe_bin_read_by_index(transceiver_index, &data, offset+1, 1);
    val += data;

    val = (val * 2);

    return val;
}

int calc_tx_power(unsigned int transceiver_index, int offset)
{
    int val = 0;
    unsigned char data = 0;

    optoe_bin_read_by_index(transceiver_index, &data, offset, 1);
    val += (data * 256);

    optoe_bin_read_by_index(transceiver_index, &data, offset+1, 1);
    val += data;

    return val;
}

int calc_rx_power(unsigned int transceiver_index, int offset)
{
    int val = 0;
    unsigned char data = 0;

    optoe_bin_read_by_index(transceiver_index, &data, offset, 1);
    val += (data * 256);

    optoe_bin_read_by_index(transceiver_index, &data, offset+1, 1);
    val += data;

    return val;
}

/*parse power temperature voltage tx_bias*/
static ssize_t sfp_eeprom_parse(unsigned int transceiver_index, struct eeprom_parse_value *parse_value)
{
    int i;

    parse_value->temperature = calc_temperature(transceiver_index, SFP_TEMPERATURE_OFFSET);
    parse_value->voltage = calc_voltage(transceiver_index, SFP_VOLTAGE_OFFSET);

    for(i = 0;i < 1;i++){
        parse_value->tx_bias[i] = calc_bias(transceiver_index, SFP_BIAS_OFFSET + (i * 2));
    }
    for(i = 0;i < 1;i++){
        parse_value->tx_power[i] = calc_tx_power(transceiver_index, SFP_TX_POWER_OFFSET + (i * 2));
    }
    for(i = 0;i < 1;i++){
        parse_value->rx_power[i] = calc_rx_power(transceiver_index, SFP_RX_POWER_OFFSET + (i * 2));
    }

    return 0;
}

static ssize_t qsfp_eeprom_parse(unsigned int transceiver_index, struct eeprom_parse_value *parse_value)
{
    int i;

    parse_value->temperature = calc_temperature(transceiver_index, QSFP_TEMPERATURE_OFFSET);
    parse_value->voltage = calc_voltage(transceiver_index, QSFP_VOLTAGE_OFFSET);

    for(i = 0;i < 4;i++){
        parse_value->tx_bias[i] = calc_bias(transceiver_index, QSFP_BIAS_OFFSET + (i * 2));
    }
    for(i = 0;i < 4;i++){
        parse_value->tx_power[i] = calc_tx_power(transceiver_index, QSFP_TX_POWER_OFFSET + (i * 2));
    }
    for(i = 0;i < 4;i++){
        parse_value->rx_power[i] = calc_rx_power(transceiver_index, QSFP_RX_POWER_OFFSET + (i * 2));
    }

    return 0;
}

static ssize_t qsfp_dd_eeprom_parse(unsigned int transceiver_index, struct eeprom_parse_value *parse_value)
{
    int i;

    parse_value->temperature = calc_temperature(transceiver_index, QSFP_DD_TEMPERATURE_OFFSET);
    parse_value->voltage = calc_voltage(transceiver_index, QSFP_DD_VOLTAGE_OFFSET);

    for(i = 0;i < 8;i++){
        parse_value->tx_bias[i] = calc_bias(transceiver_index, QSFP_DD_BIAS_OFFSET + (i * 2));
    }
    for(i = 0;i < 8;i++){
        parse_value->tx_power[i] = calc_tx_power(transceiver_index, QSFP_DD_TX_POWER_OFFSET + (i * 2));
    }
    for(i = 0;i < 8;i++){
        parse_value->rx_power[i] = calc_rx_power(transceiver_index, QSFP_DD_RX_POWER_OFFSET + (i * 2));
    }

    return 0;
}

int drv_get_eth_diagnostic(unsigned int transceiver_index, char *buf)
{
    int i,lane_num = 0;
    ssize_t ret;
    struct eeprom_parse_value parse_value;
    unsigned char identifier = 0;
    unsigned char buffer = 0;
    int rv = 0;

    /* Detect the transceiver type */
    rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
    if(rv < 0)
        return rv;

    if(((identifier == 0x0) || (identifier >= 0x1f)) && (transceiver_index != 56))
    {
        /* If identifier is 0, check the extended identifier */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;
    }

    switch(identifier)
    {
        case 0x00:
        case 0x03: /* SFP/SFP+/SFP28 */
            sfp_eeprom_parse(transceiver_index, &parse_value);
            lane_num = SFP_LANE_NUM;
            break;

        case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
        case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
            qsfp_eeprom_parse(transceiver_index, &parse_value);
            lane_num = QSFP_LANE_NUM;
            break;

        case 0x18: /* QSFP-DD */
        case 0x1e: /* QSFP+ or later with CMIS */
        case 0x1b: /* QSFP+ or later with CMIS */
            qsfp_dd_eeprom_parse(transceiver_index, &parse_value);
            rv = optoe_bin_read_by_index(transceiver_index, &buffer, QSFP_DD_LANE_NUM_OFFSET, 1);
            if(rv < 0)
                return rv;
            lane_num = (buffer >> QSFP_DD_LANE_NUM_MEDIALANE_BIT_OFFSET) & QSFP_DD_LANE_NUM_MEDIALANE_MASK;
            break;

        default:
            return -EPERM;
    }

    if(lane_num){
        ret = sprintf_s(buf, PAGE_SIZE, "temperature :%s%3d.%-2d C \n",((parse_value.temperature < 0) ? "-":""),abs(parse_value.temperature/100),abs(parse_value.temperature%100));

        //printk(KERN_DEBUG "voltage:%d",parse_value.voltage);
        ret = sprintf_s(buf, PAGE_SIZE, "%svoltage :%3d.%-2d V \n",buf,parse_value.voltage/10000,(parse_value.voltage/10)%1000);
        for(i = 0;i < lane_num;i++){
            ret = sprintf_s(buf, PAGE_SIZE, "%stxbias%d :%3d.%-2d mA \n",buf,i+1,parse_value.tx_bias[i]/1000,parse_value.tx_bias[i]%1000);
        }
        for(i = 0;i < lane_num;i++){
           ret = sprintf_s(buf, PAGE_SIZE, "%stx%d_power :%3d.%-2d mW \n",buf,i+1,parse_value.tx_power[i]/10000,parse_value.tx_power[i]%10000);
        }
        for(i = 0;i < lane_num;i++){
           ret = sprintf_s(buf, PAGE_SIZE, "%srx%d_power :%3d.%-2d mW \n",buf,i+1,parse_value.rx_power[i]/10000,parse_value.rx_power[i]%10000);
        }
        return ret;
    }

    ret = sprintf_s(buf, PAGE_SIZE, "N/A\n");
    return ret;
}

static int cpld_read(u8 cpld_addr,u8 device_id ,u8 reg)
{
    if(!reg)
       return -1;
    if(device_id == sys_cpld)
    {
        return lpc_read_cpld( LCP_DEVICE_ADDRESS1, reg);
    }else if(device_id == led_cpld1)
    // {
    //     return lpc_read_cpld( LCP_DEVICE_ADDRESS2, reg);
    // }else
    {
        return _i2c_cpld_read(cpld_addr, device_id, reg);
    }
    return -1;
}

static int cpld_write(u8 cpld_addr,u8 device_id ,u8 reg,u8 value)
{
    if(device_id == sys_cpld)
    {
        return lpc_write_cpld( LCP_DEVICE_ADDRESS1, reg, value);
    }else if(device_id == led_cpld1)
    // {
    //     return lpc_write_cpld( LCP_DEVICE_ADDRESS2, reg, value);
    // }else
    {
        return _i2c_cpld_write(cpld_addr, device_id, reg, value);
    }
    return -1;
}

int present_addr[] = {SFP_PRESENT1,SFP_PRESENT2,QSFP_PRESENT1};
int reset_addr[] = {0,0,QSFP_RST1};
int lpmod_addr[] = {0,0,QSFP_LPWN1};
int interrupt_addr[] = {0,0,QSFP_INTR1};
int rxlos_addr[] = {SFP_RXLOS1,SFP_RXLOS2,0};
int txfault_addr[] = {SFP_FLT1,SFP_FLT2,0};
int disable_addr[] = {SFP_DIS1,SFP_DIS2,0};

int drv_get_eth_value(unsigned int transceiver_index,int *eth_type,unsigned int *bit_mask)
{
    unsigned int cpld_addr = 0, offset = 0,  device_id = 0;
    unsigned int value = 0;
    if(transceiver_index<=12)
    {
        cpld_addr = port_cpld_addr[0];
        device_id = sys_cpld;
        offset = eth_type[0]+(transceiver_index-1)/8;
        *bit_mask = (transceiver_index-1)%8;
    }
    else if(transceiver_index<=48)
    {
        cpld_addr = port_cpld_addr[1];
        device_id = led_cpld1;
        offset = eth_type[1]+(transceiver_index-1-12)/8;
        *bit_mask = (transceiver_index-1-12)%8;
    }
    else if(transceiver_index<=TRANSCEIVER_TOTAL_NUM)
    {
        cpld_addr = port_cpld_addr[1];
        device_id = led_cpld1;
        offset = eth_type[2]+(transceiver_index-1-48)/8;
        *bit_mask = (transceiver_index-1-48)%8;
    }
    value = cpld_read(cpld_addr, device_id, offset);
    TRANSCEIVER_DEBUG("value 0x%x offset 0x%x bitmask %d device_id %d\n",value,offset,*bit_mask,device_id);

    return value;
}

int drv_set_eth_value(unsigned int transceiver_index,int *eth_type,u8 value)
{
    unsigned int cpld_addr = 0, offset = 0, bit_mask = 0, device_id = 0;
    unsigned int ret = 0;
    if(transceiver_index<=12)
    {
        cpld_addr = port_cpld_addr[0];
        device_id = sys_cpld;
        offset = eth_type[0]+(transceiver_index-1)/8;
        bit_mask = (transceiver_index-1)%8;
    }
    else if(transceiver_index<=48)
    {
        cpld_addr = port_cpld_addr[1];
        device_id = led_cpld1;
        offset = eth_type[1]+(transceiver_index-1-12)/8;
        bit_mask = (transceiver_index-1-12)%8;
    }
    else if(transceiver_index<=TRANSCEIVER_TOTAL_NUM)
    {
        cpld_addr = port_cpld_addr[1];
        device_id = led_cpld1;
        offset = eth_type[2]+(transceiver_index-1-48)/8;
        bit_mask = (transceiver_index-1)%8;
    }
    ret = cpld_write(cpld_addr,device_id , offset, value);
    TRANSCEIVER_DEBUG("set value 0x%x offset 0x%x bitmask %d device_id %d\n",value,offset,bit_mask,device_id);

    return ret;
}

int drv_get_eth_power_on(unsigned int transceiver_index)
{
    return 1;
#if 0
    unsigned int power_on_value = 0,bit_mask = 0;
    power_on_value = drv_get_eth_value(transceiver_index,QSFP_POWER_ON1,&bit_mask);
    if(power_on_value < 0){
        return -1;
    }
    power_on_value = (power_on_value >> bit_mask) & 1;
    //reg present bit in reg: 1: enable,   0: disable.
    //return                  1: present,       0: not present
    return !power_on_value ;
#endif
}

int drv_set_eth_power_on(unsigned int transceiver_index, unsigned int pwr_value)
{
    return 0;
#if 0
    int tmp_value = 0,ret = 0;
    unsigned int bit_mask = 0;
    tmp_value = drv_get_eth_value(transceiver_index,QSFP_POWER_ON1,&bit_mask);
    if(tmp_value < 0){
        return -1;
    }
    tmp_value = (tmp_value & ~(1 << bit_mask));
    /*set register*/
    tmp_value = tmp_value | (!pwr_value << bit_mask);
    ret = drv_set_eth_value(transceiver_index,QSFP_POWER_ON1,tmp_value);

    return ret;
#endif
}

int drv_get_eth_reset(unsigned int transceiver_index)
{
    int rst_value = 0;
    unsigned int bit_mask = 0;

    rst_value = drv_get_eth_value(transceiver_index,reset_addr,&bit_mask);
    if(rst_value < 0){
        return -1;
    }
    rst_value = (rst_value >> bit_mask) & 1;
    /*qsfp 0 set reset*/

    return (rst_value == 0) ? 1 : 0;
}

int drv_set_eth_reset(unsigned int transceiver_index, unsigned int rst_value)
{
    int tmp_value = 0,ret = 0;
    unsigned int bit_mask = 0;

    rst_value = rst_value == 0 ? 1 : 0;
    tmp_value = drv_get_eth_value(transceiver_index,reset_addr,&bit_mask);
    if(tmp_value < 0){
        return -1;
    }
    tmp_value = (tmp_value & ~(1 << bit_mask));
    /*set register*/
    tmp_value = tmp_value | (rst_value << bit_mask);
    ret = drv_set_eth_value(transceiver_index,reset_addr,tmp_value);

    return ret;
}

int drv_get_eth_lpmode(unsigned int transceiver_index,unsigned int *lpmode_value)
{

    unsigned char identifier = 0;
    unsigned char power_control = 0;
    unsigned int lp_mode = 0;
    unsigned int bit_mask = 0;
    int tmp_value = 0;
    int rv = 0;

    tmp_value = drv_get_eth_value(transceiver_index,lpmod_addr,&bit_mask);
    if(tmp_value < 0){
        return tmp_value;
    }
    tmp_value = (tmp_value >> bit_mask) & 1;
    *lpmode_value = tmp_value ? 1:0;
    return rv;

    /* Detect the transceiver type */
    rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
    if(rv < 0)
        return rv;

    if((identifier == 0x0) || (identifier >= 0x1f))
    {
        /* If identifier is 0, check the extended identifier */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;
    }

    switch(identifier)
    {
        case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
        case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
            rv = optoe_bin_read_by_index(transceiver_index, &power_control, QSFP_POWER_CONTROL_OFFSET, 1);
            if(rv < 0)
                return rv;

            if(power_control & QSFP_POWER_CONTROL_POWER_OVERRIDE_BIT)
            {
                /* The LP mode is depending on the Power Set value when the override is true. */
                lp_mode = (power_control & QSFP_POWER_CONTROL_POWER_SET_BIT) ? 1 : 0;
            }
            else
            {
                /* The LP mode is on by default */
                lp_mode = 1;
            }
            break;
        case 0x18: /* QSFP-DD */
        case 0x1b: /* DSFP+ or later with CMIS */
        case 0x1e: /* QSFP+ or later with CMIS */
            rv = optoe_bin_read_by_index(transceiver_index, &power_control, QSFP_DD_MODULE_GLOBAL_CONTROL_OFFSET, 1);
            if(rv < 0)
                return rv;

            if(power_control & QSFP_DD_MODULE_GLOBAL_CONTROL_FORCE_LOW_POWER_BIT)
            {
                /* Force low power is enabled */
                lp_mode = 1;
            }
            else
            {
                /* The LP mode is depending on the Low Power value when the Force Low Power is disabled. */
                lp_mode = (power_control & QSFP_DD_MODULE_GLOBAL_CONTROL_LOW_POWER_BIT) ? 1 : 0;
            }
            break;
        default:
            /* Fail to recognize the transceiver type, regard it as default low power. */
            lp_mode = 1;
            break;
    }

    *lpmode_value = (lp_mode ? 1 : 0);
    return rv;
}

int drv_set_eth_lpmode(unsigned int transceiver_index, unsigned int lpmode_value)
{
    unsigned char identifier = 0;
    unsigned char power_control = 0;
    int tmp_value = 0;
    int rv = 0;
    unsigned int bit_mask = 0;

    lpmode_value = lpmode_value == 0 ? 0 : 1;
    tmp_value = drv_get_eth_value(transceiver_index,lpmod_addr,&bit_mask);
    tmp_value = (tmp_value & ~(1 << bit_mask));
    /*set register*/
    tmp_value = tmp_value | (lpmode_value << bit_mask);
    rv = drv_set_eth_value(transceiver_index,lpmod_addr,tmp_value);

    return rv;

    /* Detect the transceiver type */
    rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
    if(rv < 0)
        return rv;

    if((identifier == 0x0) || (identifier >= 0x1f))
    {
        /* If identifier is 0, check the extended identifier */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;
    }

    switch(identifier)
    {
        case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
        case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
            rv = optoe_bin_read_by_index(transceiver_index, &power_control, QSFP_POWER_CONTROL_OFFSET, 1);
            if(rv < 0)
                return rv;

            if(!(power_control & QSFP_POWER_CONTROL_POWER_OVERRIDE_BIT))
            {
                /* Setup the Power Override bit */
                power_control |= QSFP_POWER_CONTROL_POWER_OVERRIDE_BIT;
            }

            if(lpmode_value)
            {
                /* Setup the Power Set bit for the Low power mode */
                if(!(power_control & QSFP_POWER_CONTROL_POWER_SET_BIT))
                {
                    power_control |= QSFP_POWER_CONTROL_POWER_SET_BIT;
                }
            }
            else
            {
                /* Clear the Power Set bit for the High power mode */
                if(power_control & QSFP_POWER_CONTROL_POWER_SET_BIT)
                {
                    power_control &= ~(QSFP_POWER_CONTROL_POWER_SET_BIT);
                }
            }
            rv = optoe_bin_write_by_index(transceiver_index, &power_control, QSFP_POWER_CONTROL_OFFSET, 1);
            if(rv < 0)
                return rv;

            break;

        case 0x18: /* QSFP-DD */
        case 0x1b: /* DSFP+ or later with CMIS */
        case 0x1e: /* QSFP+ or later with CMIS */
            rv = optoe_bin_read_by_index(transceiver_index, &power_control, QSFP_DD_MODULE_GLOBAL_CONTROL_OFFSET, 1);
            TRANSCEIVER_DEBUG("port%d value 0x%x\n",transceiver_index,power_control);
            if(rv < 0)
                return rv;

            if(power_control & QSFP_DD_MODULE_GLOBAL_CONTROL_FORCE_LOW_POWER_BIT)
            {
                /* Clear the Force Low Power bit */
                power_control &= ~QSFP_DD_MODULE_GLOBAL_CONTROL_FORCE_LOW_POWER_BIT;
            }

            if(lpmode_value)
            {
                /* Setup the Low Power bit for the Low power mode */
                if(!(power_control & QSFP_DD_MODULE_GLOBAL_CONTROL_LOW_POWER_BIT))
                {
                    power_control |= QSFP_DD_MODULE_GLOBAL_CONTROL_LOW_POWER_BIT;
                }
            }
            else
            {
                /* Clear the Low Power bit for the High power mode */
                if(power_control & QSFP_DD_MODULE_GLOBAL_CONTROL_LOW_POWER_BIT)
                {
                    power_control &= ~(QSFP_DD_MODULE_GLOBAL_CONTROL_LOW_POWER_BIT);
                }
            }
            rv = optoe_bin_write_by_index(transceiver_index, &power_control, QSFP_DD_MODULE_GLOBAL_CONTROL_OFFSET, 1);
            if(rv < 0)
                return rv;

            break;

        default:
            break;
    }

    return rv;
}

#define S3IP_ETH_PRESENT 1
#define S3IP_ETH_ABSENT 0
#define CPLD_ETH_PRESENT 0
#define CPLD_ETH_ABSENT 1
int drv_get_eth_present(unsigned int transceiver_index)
{
    unsigned int present_value = 0;
    unsigned int bit_mask = 0;
    int ret = 0;
    present_value = drv_get_eth_value(transceiver_index,present_addr,&bit_mask);
    if(present_value < 0){
        return -1;
    }
    present_value = (present_value >> bit_mask) & 1;
    //reg present bit in reg: 1: not present,   0: present.
    //return                  1: present,       0: not present
    if(present_value == CPLD_ETH_PRESENT)
    {
        ret = S3IP_ETH_PRESENT;
    }
    else
    {
        ret = S3IP_ETH_ABSENT;
    }
    return (present_value == 0) ? 1 : 0;
}

int drv_get_eth_i2c_status(unsigned int transceiver_index)
{
    unsigned char identifier = 0;
    int rv = 0;

    /* Detect the transceiver type */
    rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
    if(rv < 0)
        return 1;//abnormal
    else
        return 0;//normal
}

#define S3IP_ETH_INTERRUPT 1
#define S3IP_ETH_NO_INTERRUPT 0
#define CPLD_ETH_INTERRUPT 0
#define CPLD_ETH_NO_INTERRUPT 1
int drv_get_eth_interrupt(unsigned int transceiver_index)
{
    unsigned int interrupt_value = 0;
    unsigned int bit_mask = 0;
    int ret = 0;
    interrupt_value = drv_get_eth_value(transceiver_index,interrupt_addr,&bit_mask);
    if(interrupt_value < 0){
        return -1;
    }
    interrupt_value = (interrupt_value >> bit_mask) & 1;

    if(interrupt_value == CPLD_ETH_NO_INTERRUPT)
    {
        ret = S3IP_ETH_NO_INTERRUPT;
    }
    else
    {
        ret = S3IP_ETH_INTERRUPT;
    }
    return ret;
}

int drv_get_eth_temp(unsigned int transceiver_index, long long *temp)
{
    unsigned char identifier = 0;
    unsigned char temp_buffer[2] = {0};
    short temperature = 0;
    unsigned int offset = 0;
    int rv = 0;

    /* Detect the transceiver type */
    rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
    if(rv < 0)
        return rv;

    if((identifier == 0x0) || (identifier >= 0x1f))
    {
        /* If identifier is 0, check the extended identifier */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;
    }

    switch(identifier)
    {
        case 0x00:
        case 0x03: /* SFP/SFP+/SFP28 */
            offset = SFP_TEMPERATURE_OFFSET;
            break;

        case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
        case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
            offset = QSFP_TEMPERATURE_OFFSET;
            break;

        case 0x18: /* QSFP-DD */
        case 0x1e: /* QSFP+ or later with CMIS */
        case 0x1b: /* QSFP+ or later with CMIS */
            offset = QSFP_DD_TEMPERATURE_OFFSET;
            break;

        default:
            return -EPERM;
    }

    rv = optoe_bin_read_by_index(transceiver_index, temp_buffer, offset, 2);
    if(rv < 0)
        return rv;
    temperature = (temp_buffer[1] + (temp_buffer[0] << 8));
    /* 16-bit signed twos complement value in increments of 1/256 (0.00390625) degrees Celsius */
    *temp = (((long long)temperature >> 8) * 100000000) + ((long long)(temperature & 0xFF) * 390625);

    return rv;
}

#define S3IP_RX_LOS 1
#define S3IP_RX_NOT_LOS 0
#define CPLD_RX_LOS 1
#define CPLD_RX_NOT_LOS 0
int drv_get_eth_rx_los(unsigned int transceiver_index, unsigned char *rx_los)//rx_los[0]=lane_num,rx_los[1]=rx_los_reg
{
    unsigned int rx_los_value = 0;
    unsigned int bit_mask = 0;
    unsigned char identifier = 0;
    unsigned char buffer;
    unsigned int offset = 0;
    unsigned char lane_num = 0;
    unsigned int mask = 0;
    unsigned int bit_offset = 0;
    int rv = 0;

    if(transceiver_index <= SFP_END)
    {
        rv = drv_get_eth_value(transceiver_index, rxlos_addr, &bit_mask);
        if(rv < 0)
            return -1;

        rx_los_value = (rv >> bit_mask) & 1;

        if(rx_los_value == CPLD_RX_LOS)
        {
            rx_los[1] = S3IP_RX_LOS;
        }
        else
        {
            rx_los[1] = S3IP_RX_NOT_LOS;
        }
        return rv;
    }
    else
    {
        /* Detect the transceiver type */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;

        if((identifier == 0x0) || (identifier >= 0x1f))
        {
            /* If identifier is 0, check the extended identifier */
            rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
            if(rv < 0)
                return rv;
        }

        switch(identifier)
        {
            case 0x00:
            case 0x03: /* SFP/SFP+/SFP28 */
                offset = SFP_RX_LOS_OFFSET;
                mask = SFP_RX_LOS_MASK;
                bit_offset = SFP_RX_LOS_BIT_OFFSET;
                lane_num = SFP_LANE_NUM;
                break;

            case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
            case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
                offset = QSFP_RX_LOS_OFFSET;
                mask = QSFP_RX_LOS_MASK;
                bit_offset = QSFP_RX_LOS_BIT_OFFSET;
                lane_num = QSFP_LANE_NUM;
                break;

            case 0x18: /* QSFP-DD */
            case 0x1e: /* QSFP+ or later with CMIS */
                offset = QSFP_DD_RX_LOS_OFFSET;
                mask = QSFP_DD_RX_LOS_MASK;
                bit_offset = QSFP_DD_RX_LOS_BIT_OFFSET;
                rv = optoe_bin_read_by_index(transceiver_index, &buffer, QSFP_DD_LANE_NUM_OFFSET, 1);
                if(rv < 0)
                    return rv;
                lane_num = (buffer >> QSFP_DD_LANE_NUM_MEDIALANE_BIT_OFFSET) & QSFP_DD_LANE_NUM_MEDIALANE_MASK;
                break;

            default:
                return -EPERM;
        }
        rv = optoe_bin_read_by_index(transceiver_index, &buffer, offset, 1);
        if(rv < 0)
            return rv;
        rx_los[0] = lane_num;
        rx_los[1] = (buffer >> bit_offset) & mask;
        return rv;
    }
}

int drv_get_eth_tx_los(unsigned int transceiver_index, unsigned char *tx_los)//tx_los[0]=lane_num,tx_los[1]=tx_los_reg
{
    unsigned char identifier = 0;
    unsigned char buffer;
    unsigned int offset = 0;
    unsigned char lane_num = 0;
    unsigned int mask = 0;
    unsigned int bit_offset = 0;
    int rv = 0;

    /* Detect the transceiver type */
    rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
    if(rv < 0)
        return rv;

    if((identifier == 0x0) || (identifier >= 0x1f))
    {
        /* If identifier is 0, check the extended identifier */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;
    }

    switch(identifier)
    {
        case 0x00:
        case 0x03: /* SFP/SFP+/SFP28 not support */
            return -1;

        case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
        case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
            offset = QSFP_TX_LOS_OFFSET;
            mask = QSFP_TX_LOS_MASK;
            bit_offset = QSFP_TX_LOS_BIT_OFFSET;
            lane_num = QSFP_LANE_NUM;
            break;

        case 0x18: /* QSFP-DD */
        case 0x1e: /* QSFP+ or later with CMIS */
            offset = QSFP_DD_TX_LOS_OFFSET;
            mask = QSFP_DD_TX_LOS_MASK;
            bit_offset = QSFP_DD_TX_LOS_BIT_OFFSET;
            rv = optoe_bin_read_by_index(transceiver_index, &buffer, QSFP_DD_LANE_NUM_OFFSET, 1);
            if(rv < 0)
                return rv;
            lane_num = (buffer >> QSFP_DD_LANE_NUM_MEDIALANE_BIT_OFFSET) & QSFP_DD_LANE_NUM_MEDIALANE_MASK;
            break;

        default:
            return -EPERM;
    }
    rv = optoe_bin_read_by_index(transceiver_index, &buffer, offset, 1);
    if(rv < 0)
        return rv;
    tx_los[0] = lane_num;
    tx_los[1] = (buffer >> bit_offset) & mask;
    return rv;
}

#define S3IP_TX_DISABLE 1
#define S3IP_TX_ENABLE 0
#define CPLD_TX_DISABLE 1
#define CPLD_TX_ENABLE 0
int drv_get_eth_tx_disable(unsigned int transceiver_index, unsigned char *tx_disable)//tx_disable[0]=lane_num,tx_disable[1]=tx_disable_reg
{
    unsigned int tx_disable_value = 0;
    unsigned int bit_mask = 0;
    unsigned char identifier = 0;
    unsigned char buffer;
    unsigned int offset = 0;
    unsigned char lane_num = 0;
    unsigned int mask = 0;
    unsigned int bit_offset = 0;
    int rv = 0;

    if(transceiver_index <= SFP_END)
    {
        rv = drv_get_eth_value(transceiver_index,disable_addr,&bit_mask);
        if(rv < 0)
            return rv;

        tx_disable_value = (rv >> bit_mask) & 1;
        if(tx_disable_value == CPLD_TX_ENABLE)
        {
            tx_disable[1] = S3IP_TX_ENABLE;
        }
        else
        {
            tx_disable[1] = S3IP_TX_DISABLE;
        }
        return rv;
    }
    else
    {
        /* Detect the transceiver type */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;

        if((identifier == 0x0) || (identifier >= 0x1f))
        {
            /* If identifier is 0, check the extended identifier */
            rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
            if(rv < 0)
                return rv;
        }

        switch(identifier)
        {
            case 0x00:
            case 0x03: /* SFP/SFP+/SFP28 */
                offset = SFP_TX_DISABLE_OFFSET;
                mask = SFP_TX_DISABLE_MASK;
                bit_offset = SFP_TX_DISABLE_BIT_OFFSET;
                lane_num = SFP_LANE_NUM;
                break;

            case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
            case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
                offset = QSFP_TX_DISABLE_OFFSET;
                mask = QSFP_TX_DISABLE_MASK;
                bit_offset = QSFP_TX_DISABLE_BIT_OFFSET;
                lane_num = QSFP_LANE_NUM;
                break;

            case 0x18: /* QSFP-DD */
            case 0x1e: /* QSFP+ or later with CMIS */
                offset = QSFP_DD_TX_DISABLE_OFFSET;
                mask = QSFP_DD_TX_DISABLE_MASK;
                bit_offset = QSFP_DD_TX_DISABLE_BIT_OFFSET;
                rv = optoe_bin_read_by_index(transceiver_index, &buffer, QSFP_DD_LANE_NUM_OFFSET, 1);
                if(rv < 0)
                    return rv;
                lane_num = (buffer >> QSFP_DD_LANE_NUM_MEDIALANE_BIT_OFFSET) & QSFP_DD_LANE_NUM_MEDIALANE_MASK;
                break;

            default:
                return -EPERM;
        }
        rv = optoe_bin_read_by_index(transceiver_index, &buffer, offset, 1);
        if(rv < 0)
            return rv;
        tx_disable[0] = lane_num;
        tx_disable[1] = (buffer >> bit_offset) & mask;

        return rv;
    }
}

int drv_get_eth_rx_disable(unsigned int transceiver_index, unsigned char *rx_disable)//rx_disable[0]=lane_num,rx_disable[1]=rx_disable_reg
{
    unsigned char identifier = 0;
    unsigned char buffer;
    unsigned int offset = 0;
    unsigned char lane_num = 0;
    unsigned int mask = 0;
    unsigned int bit_offset = 0;
    int rv = 0;

    /* Detect the transceiver type */
    rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
    if(rv < 0)
        return rv;

    if((identifier == 0x0) || (identifier >= 0x1f))
    {
        /* If identifier is 0, check the extended identifier */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;
    }

    switch(identifier)
    {
        case 0x00:
        case 0x03: /* SFP/SFP+/SFP28 not support */
            return -1;

        case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
        case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
            offset = QSFP_RX_DISABLE_OFFSET;
            mask = QSFP_RX_DISABLE_MASK;
            bit_offset = QSFP_RX_DISABLE_BIT_OFFSET;
            lane_num = QSFP_LANE_NUM;
            break;

        case 0x18: /* QSFP-DD */
        case 0x1e: /* QSFP+ or later with CMIS */
            offset = QSFP_DD_RX_DISABLE_OFFSET;
            mask = QSFP_DD_RX_DISABLE_MASK;
            bit_offset = QSFP_DD_RX_DISABLE_BIT_OFFSET;
            rv = optoe_bin_read_by_index(transceiver_index, &buffer, QSFP_DD_LANE_NUM_OFFSET, 1);
            if(rv < 0)
                return rv;
            lane_num = (buffer >> QSFP_DD_LANE_NUM_MEDIALANE_BIT_OFFSET) & QSFP_DD_LANE_NUM_MEDIALANE_MASK;
            break;

        default:
            return -EPERM;
    }

    rv = optoe_bin_read_by_index(transceiver_index, &buffer, offset, 1);
    rx_disable[0] = lane_num;
    rx_disable[1] = (buffer >> bit_offset) & mask;

    return rv;
}

int drv_set_eth_tx_disable(unsigned int transceiver_index, unsigned int rst_value)
{
    int tmp_value = 0;
    unsigned int tx_disable_value = 0;
    unsigned char tx_disable = 0;
    unsigned int bit_mask = 0;
    unsigned char identifier = 0;
    unsigned char buffer;
    unsigned int offset = 0;
    unsigned char lane_num = 0;
    unsigned int mask = 0;
    unsigned int bit_offset = 0;
    int rv = 0;

    if(transceiver_index <= SFP_END)
    {
        if(rst_value == S3IP_TX_ENABLE)
        {
            rst_value = CPLD_TX_ENABLE;
        }
        else
        {
            rst_value = CPLD_TX_DISABLE;
        }
        tmp_value = drv_get_eth_value(transceiver_index, disable_addr, &bit_mask);
        if(tmp_value < 0){
            return tmp_value;
        }
        tmp_value = (tmp_value & ~(1 << bit_mask));
        /*set register*/
        tmp_value = tmp_value | (rst_value << bit_mask);
        rv = drv_set_eth_value(transceiver_index, disable_addr, tmp_value);

        return rv;
    }
    else
    {
        /* Detect the transceiver type */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;

        if((identifier == 0x0) || (identifier >= 0x1f))
        {
            /* If identifier is 0, check the extended identifier */
            rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
            if(rv < 0)
                return rv;
        }

        switch(identifier)
        {
            case 0x00:
            case 0x03: /* SFP/SFP+/SFP28 */
                offset = SFP_TX_DISABLE_OFFSET;
                mask = SFP_TX_DISABLE_MASK;
                bit_offset = SFP_TX_DISABLE_BIT_OFFSET;
                lane_num = SFP_LANE_NUM;
                break;

            case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
            case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
                offset = QSFP_TX_DISABLE_OFFSET;
                mask = QSFP_TX_DISABLE_MASK;
                bit_offset = QSFP_TX_DISABLE_BIT_OFFSET;
                lane_num = QSFP_LANE_NUM;
                break;

            case 0x18: /* QSFP-DD */
            case 0x1e: /* QSFP+ or later with CMIS */
                offset = QSFP_DD_TX_DISABLE_OFFSET;
                mask = QSFP_DD_TX_DISABLE_MASK;
                bit_offset = QSFP_DD_TX_DISABLE_BIT_OFFSET;
                rv = optoe_bin_read_by_index(transceiver_index, &buffer, QSFP_DD_LANE_NUM_OFFSET, 1);
                if(rv < 0)
                    return rv;
                lane_num = (buffer >> QSFP_DD_LANE_NUM_MEDIALANE_BIT_OFFSET) & QSFP_DD_LANE_NUM_MEDIALANE_MASK;
                break;

            default:
                return -EPERM;
        }

        rv = optoe_bin_read_by_index(transceiver_index, &buffer, offset, 1);
        if(rv < 0)
            return rv;

        tx_disable = ((buffer | (mask<<bit_offset)) & ((rst_value & mask)<<bit_offset))&0xFF;
        rv = optoe_bin_write_by_index(transceiver_index, &tx_disable, offset, 1);
        if(rv < 0)
            return rv;
        return rv;
    }
}

int drv_get_eth_tx_cdr_lol(unsigned int transceiver_index, unsigned char *tx_cdr_lol)//tx_cdr_lol[0]=lane_num,tx_cdr_lol[1]=tx_cdr_lol_reg
{
    unsigned char identifier = 0;
    unsigned char buffer;
    unsigned int offset = 0;
    unsigned char lane_num = 0;
    unsigned int mask = 0;
    unsigned int bit_offset = 0;
    int rv = 0;

    /* Detect the transceiver type */
    rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
    if(rv < 0)
        return rv;

    if((identifier == 0x0) || (identifier >= 0x1f))
    {
        /* If identifier is 0, check the extended identifier */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;
    }

    switch(identifier)
    {
        case 0x00:
        case 0x03: /* SFP/SFP+/SFP28 */
            offset = SFP_TX_CDR_LOL_OFFSET;
            mask = SFP_TX_CDR_LOL_MASK;
            bit_offset = SFP_TX_CDR_LOL_BIT_OFFSET;
            lane_num = SFP_LANE_NUM;
            break;

        case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
        case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
            offset = QSFP_TX_CDR_LOL_OFFSET;
            mask = QSFP_TX_CDR_LOL_MASK;
            bit_offset = QSFP_TX_CDR_LOL_BIT_OFFSET;
            lane_num = QSFP_LANE_NUM;
            break;

        case 0x18: /* QSFP-DD */
        case 0x1e: /* QSFP+ or later with CMIS */
            offset = QSFP_DD_TX_CDR_LOL_OFFSET;
            mask = QSFP_DD_TX_CDR_LOL_MASK;
            bit_offset = QSFP_DD_TX_CDR_LOL_BIT_OFFSET;
            rv = optoe_bin_read_by_index(transceiver_index, &buffer, QSFP_DD_LANE_NUM_OFFSET, 1);
            if(rv < 0)
                return rv;
            lane_num = (buffer >> QSFP_DD_LANE_NUM_MEDIALANE_BIT_OFFSET) & QSFP_DD_LANE_NUM_MEDIALANE_MASK;
            break;

        default:
            return -EPERM;
    }

    rv = optoe_bin_read_by_index(transceiver_index, &buffer, offset, 1);
    if(rv < 0)
        return rv;
    tx_cdr_lol[0] = lane_num;
    tx_cdr_lol[1] = (buffer >> bit_offset) & mask;

    return rv;
}

int drv_get_eth_rx_cdr_lol(unsigned int transceiver_index, unsigned char *rx_cdr_lol)//rx_cdr_lol[0]=lane_num,rx_cdr_lol[1]=rx_cdr_lol_reg
{
    unsigned char identifier = 0;
    unsigned char buffer;
    unsigned int offset = 0;
    unsigned char lane_num = 0;
    unsigned int mask = 0;
    unsigned int bit_offset = 0;
    int rv = 0;

    /* Detect the transceiver type */
    rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
    if(rv < 0)
        return rv;

    if((identifier == 0x0) || (identifier >= 0x1f))
    {
        /* If identifier is 0, check the extended identifier */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;
    }

    switch(identifier)
    {
        case 0x00:
        case 0x03: /* SFP/SFP+/SFP28 */
            offset = SFP_RX_CDR_LOL_OFFSET;
            mask = SFP_RX_CDR_LOL_MASK;
            bit_offset = SFP_RX_CDR_LOL_BIT_OFFSET;
            lane_num = SFP_LANE_NUM;
            break;

        case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
        case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
            offset = QSFP_RX_CDR_LOL_OFFSET;
            mask = QSFP_RX_CDR_LOL_MASK;
            bit_offset = QSFP_RX_CDR_LOL_BIT_OFFSET;
            lane_num = QSFP_LANE_NUM;
            break;

        case 0x18: /* QSFP-DD */
        case 0x1e: /* QSFP+ or later with CMIS */
            offset = QSFP_DD_RX_CDR_LOL_OFFSET;
            mask = QSFP_DD_RX_CDR_LOL_MASK;
            bit_offset = QSFP_DD_RX_CDR_LOL_BIT_OFFSET;
            rv = optoe_bin_read_by_index(transceiver_index, &buffer, QSFP_DD_LANE_NUM_OFFSET, 1);
            if(rv < 0)
                return rv;
            lane_num = (buffer >> QSFP_DD_LANE_NUM_MEDIALANE_BIT_OFFSET) & QSFP_DD_LANE_NUM_MEDIALANE_MASK;
            break;

        default:
            return -EPERM;
    }

    rv = optoe_bin_read_by_index(transceiver_index, &buffer, offset, 1);
    if(rv < 0)
        return rv;
    rx_cdr_lol[0] = lane_num;
    rx_cdr_lol[1] = (buffer >> bit_offset) & mask;

    return rv;
}

#define S3IP_TX_FAULT 1
#define S3IP_TX_NOT_FAULT 0
#define CPLD_TX_FAULT 1
#define CPLD_TX_NOT_FAULT 0
int drv_get_eth_tx_fault(unsigned int transceiver_index, unsigned char *tx_fault)//tx_fault[0]=lane_num,tx_fault[1]=tx_fault_reg
{
    unsigned int tx_fault_value = 0;
    unsigned int bit_mask = 0;
    unsigned char identifier = 0;
    unsigned char buffer;
    unsigned int offset = 0;
    unsigned char lane_num = 0;
    unsigned int mask = 0;
    unsigned int bit_offset = 0;
    int rv = 0;

    if(transceiver_index <= SFP_END)
    {
        rv = drv_get_eth_value(transceiver_index, txfault_addr, &bit_mask);
        if(rv < 0)
            return rv;

        tx_fault_value = (rv >> bit_mask) & 1;
        if(tx_fault_value == CPLD_TX_FAULT)
        {
            tx_fault[1] = S3IP_TX_FAULT;
        }
        else
        {
            tx_fault[1] = S3IP_TX_NOT_FAULT;
        }
        return rv;
    }
    else
    {
        /* Detect the transceiver type */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;

        if((identifier == 0x0) || (identifier >= 0x1f))
        {
            /* If identifier is 0, check the extended identifier */
            rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
            if(rv < 0)
                return rv;
        }

        switch(identifier)
        {
            case 0x00:
            case 0x03: /* SFP/SFP+/SFP28 */
                offset = SFP_TX_FAULT_OFFSET;
                mask = SFP_TX_FAULT_MASK;
                bit_offset = SFP_TX_FAULT_BIT_OFFSET;
                lane_num = SFP_LANE_NUM;
                break;

            case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
            case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
                offset = QSFP_TX_FAULT_OFFSET;
                mask = QSFP_TX_FAULT_MASK;
                bit_offset = QSFP_TX_FAULT_BIT_OFFSET;
                lane_num = QSFP_LANE_NUM;
                break;

            case 0x18: /* QSFP-DD */
            case 0x1e: /* QSFP+ or later with CMIS */
                offset = QSFP_DD_TX_FAULT_OFFSET;
                mask = QSFP_DD_TX_FAULT_MASK;
                bit_offset = QSFP_DD_TX_FAULT_BIT_OFFSET;
                rv = optoe_bin_read_by_index(transceiver_index, &buffer, QSFP_DD_LANE_NUM_OFFSET, 1);
                if(rv < 0)
                    return rv;
                lane_num = (buffer >> QSFP_DD_LANE_NUM_MEDIALANE_BIT_OFFSET) & QSFP_DD_LANE_NUM_MEDIALANE_MASK;
                break;

            default:
                return -EPERM;
        }
        rv = optoe_bin_read_by_index(transceiver_index, &buffer, offset, 1);
        if(rv < 0)
            return rv;
        tx_fault[0] = lane_num;
        tx_fault[1] = (buffer >> bit_offset) & mask;
        return rv;
    }
}

int drv_get_eth_eeprom(unsigned int transceiver_index, char *buf, loff_t off, size_t len)
{
    return optoe_bin_read_by_index(transceiver_index, buf, off, len);
}

int drv_set_eth_eeprom(unsigned int transceiver_index, char *buf, loff_t off, size_t len)
{
    return optoe_bin_write_by_index(transceiver_index, buf, off, len);
}

static int clear_specific_char(unsigned char *buf, int len)
{
    int offset = 0;
    int start_flag = 0;
    TRANSCEIVER_DEBUG("clear char len = %d\n", len);
    for(offset = len; offset > 0; offset--)
    {
        if((buf[offset-1] == 0x20) || (buf[offset-1] == 0xFF))
        {
            TRANSCEIVER_DEBUG("buf[offset-1] = %x \n", buf[offset-1]);
            buf[offset-1] = 0x00;
            start_flag = 1;
        }
        else
        {
            if(start_flag == 1)
            {
                TRANSCEIVER_DEBUG("offset = %d \n", offset);
                break;
            }
        }
    }
    return 0;
}

#define ETH_MAX_SN_NUM (SFP_SN_LEN+1)
int drv_get_eth_sn(unsigned int transceiver_index, char *buf)
{
    unsigned char identifier = 0;
    unsigned char sn_buffer[ETH_MAX_SN_NUM] = {0};
    unsigned int offset = 0;
    unsigned int len = 0;
    int rv = 0;

    /* Detect the transceiver type */
    rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_IDENTIIFIER_OFFSET, 1);
    if(rv < 0)
        return rv;

    if((identifier == 0x0) || (identifier >= 0x1f))
    {
        /* If identifier is 0, check the extended identifier */
        rv = optoe_bin_read_by_index(transceiver_index, &identifier, TRANSCEIVER_EXTENDED_IDENTIIFIER_OFFSET, 1);
        if(rv < 0)
            return rv;
    }

    switch(identifier)
    {
        case 0x00:
        case 0x03: /* SFP/SFP+/SFP28 */
            offset = SFP_SN_OFFSET;
            len = SFP_SN_LEN;
            break;

        case 0x0d: /* QSFP+ or later with SFF8636 or SFF8436 mgmt interface */
        case 0x11: /* QSFP28 or later with SFF8636 mgmt interface */
            offset = QSFP_SN_OFFSET;
            len = QSFP_SN_LEN;
            break;

        case 0x18: /* QSFP-DD */
        case 0x1e: /* QSFP+ or later with CMIS */
        case 0x1b: /* QSFP+ or later with CMIS */
            offset = QSFP_DD_SN_OFFSET;
            len = QSFP_DD_SN_LEN;
            break;

        default:
            return -EPERM;
    }

    rv = optoe_bin_read_by_index(transceiver_index, sn_buffer, offset, len);
    if(rv < 0)
        return rv;
    clear_specific_char(sn_buffer, ETH_MAX_SN_NUM);

    return sprintf_s(buf, PAGE_SIZE, "%s\n", sn_buffer);
}

int drv_get_present_all(long long unsigned int *pvalue)
{
    int i = 0, status = 0;

    u8 values1[2]  = {0};
    u8 values2[6]  = {0};
    // u32 value32 = 0;
    // u8 *tmp_val  = (u8*)&value32;
    u64 value = 0,tmp = 0;
    u8 cpld_id[2] = {sys_cpld,led_cpld1};
    u8 regs1[] = {SFP_PRESENT1, SFP_PRESENT1+1};
    u8 regs2[] = { SFP_PRESENT2,SFP_PRESENT2+1,
        SFP_PRESENT2+2,SFP_PRESENT2+3,SFP_PRESENT2+4 ,QSFP_PRESENT1};

    for (i = 0; i < ARRAY_SIZE(regs1); i++) {
        status = cpld_read(port_cpld_addr[0], cpld_id[0], regs1[i]);

        if (status < 0) {
            return status;
        }
        values1[i] = ~(u8)status;
    }
    for (i = 0; i < ARRAY_SIZE(regs2); i++) {
        status = cpld_read(port_cpld_addr[1], cpld_id[1], regs2[i]);

        if (status < 0) {
            return status;
        }
        values2[i] = ~(u8)status;

    }

    tmp = values1[0];
    value |= tmp ;
    tmp = values1[1]&0xf;
    value |= tmp << 8;
    tmp = values2[0];
    value |= tmp << 12;
    tmp = values2[1];
    value |= tmp << 20;
    tmp = values2[2];
    value |= tmp << 28;
    tmp = values2[3];
    value |= tmp << 36;
    tmp = values2[4]&0xf;
    value |= tmp << 44;
    tmp = values2[5];
    value |= tmp << 48;
    /* For port 1 ~ 56 in order */
    value &= 0xFFFFFFFFFFFFFF;
    *pvalue = value ;

    return status;
}

int drv_get_eth_i2c_clk(unsigned int transceiver_index)
{
    int clk_value = 0;
    unsigned int  offset = 0;

    if(transceiver_index<=TRANSCEIVER_TOTAL_NUM)
    {
        offset = CLK_OFFSET+(transceiver_index-1)*0x20;
    }

    clk_value = fpga_pcie_read(offset);
    if(clk_value < 0){
        return -1;
    }

    return clk_value;

}

int drv_set_eth_i2c_clk(unsigned int transceiver_index, unsigned int rst_value)
{
    int rv = 0;
    unsigned int  offset = 0;

    if(transceiver_index<=TRANSCEIVER_TOTAL_NUM)
    {
        offset = CLK_OFFSET+(transceiver_index-1)*0x20;
    }

    rv = fpga_pcie_write(offset,rst_value);
    if(rv < 0){
        return -1;
    }
    return rv;
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
    return sprintf_s(buf, PAGE_SIZE, "transceiver driver debug message\n");
}

ssize_t drv_debug(const char *buf, int count)
{
    return 0;
}


static struct transceiver_drivers_t pfunc = {
    .get_eth_diagnostic = drv_get_eth_diagnostic,
    .get_eth_power_on = drv_get_eth_power_on,
    .set_eth_power_on = drv_set_eth_power_on,
    .get_eth_reset = drv_get_eth_reset,
    .set_eth_reset = drv_set_eth_reset,
    .get_eth_lpmode = drv_get_eth_lpmode,
    .set_eth_lpmode = drv_set_eth_lpmode,
    .get_eth_present = drv_get_eth_present,
    .get_eth_i2c_status = drv_get_eth_i2c_status,
    .get_eth_interrupt = drv_get_eth_interrupt,
    .get_eth_eeprom = drv_get_eth_eeprom,
    .set_eth_eeprom = drv_set_eth_eeprom,
    .get_eth_temp = drv_get_eth_temp,
    .get_eth_sn = drv_get_eth_sn,
    .get_present_all = drv_get_present_all,
    .get_eth_tx_los = drv_get_eth_tx_los,
    .get_eth_rx_los = drv_get_eth_rx_los,
    .get_eth_tx_disable = drv_get_eth_tx_disable,
    .get_eth_rx_disable = drv_get_eth_rx_disable,
    .set_eth_tx_disable = drv_set_eth_tx_disable,
    .get_eth_tx_cdr_lol = drv_get_eth_tx_cdr_lol,
    .get_eth_rx_cdr_lol = drv_get_eth_rx_cdr_lol,
    .get_eth_tx_fault = drv_get_eth_tx_fault,
    .get_eth_i2c_clk = drv_get_eth_i2c_clk,
    .set_eth_i2c_clk = drv_set_eth_i2c_clk,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
    .debug_help = drv_debug_help,
};


static int drv_transceiver_probe(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    s3ip_transceiver_drivers_register(&pfunc);
    mxsonic_transceiver_drivers_register(&pfunc);
#else
    hisonic_transceiver_drivers_register(&pfunc);
    kssonic_transceiver_drivers_register(&pfunc);
#endif

    return 0;
}

static int drv_transceiver_remove(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    s3ip_transceiver_drivers_unregister();
    mxsonic_transceiver_drivers_unregister();
#else
    hisonic_transceiver_drivers_unregister();
    kssonic_transceiver_drivers_unregister();
#endif

    return 0;
}

static struct platform_driver drv_transceiver_driver = {
    .driver = {
        .name   = DRVNAME,
        .owner = THIS_MODULE,
    },
    .probe      = drv_transceiver_probe,
    .remove     = drv_transceiver_remove,
};

static int __init drv_transceiver_init(void)
{
    int err;
    int retval;

    drv_transceiver_device = platform_device_alloc(DRVNAME, 0);
    if(!drv_transceiver_device)
    {
        TRANSCEIVER_DEBUG("%s(#%d): platform_device_alloc fail\n", __func__, __LINE__);
        return -ENOMEM;
    }

    retval = platform_device_add(drv_transceiver_device);
    if(retval)
    {
        TRANSCEIVER_DEBUG("%s(#%d): platform_device_add failed\n", __func__, __LINE__);
        err = -retval;
        goto dev_add_failed;
    }

    retval = platform_driver_register(&drv_transceiver_driver);
    if(retval)
    {
        TRANSCEIVER_DEBUG("%s(#%d): platform_driver_register failed(%d)\n", __func__, __LINE__, err);
        err = -retval;
        goto dev_reg_failed;
    }

    return 0;

dev_reg_failed:
    platform_device_unregister(drv_transceiver_device);
    return err;
dev_add_failed:
    platform_device_put(drv_transceiver_device);
    return err;
}

static void __exit drv_transceiver_exit(void)
{
    platform_driver_unregister(&drv_transceiver_driver);
    platform_device_unregister(drv_transceiver_device);

    return;
}


MODULE_DESCRIPTION("Huarong Transceiver Driver");
MODULE_VERSION(SWITCH_TRANSCEIVER_DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(drv_transceiver_init);
module_exit(drv_transceiver_exit);
