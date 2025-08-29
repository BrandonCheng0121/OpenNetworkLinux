/************************************************************
 * <bsn.cl fy=2014 v=onl>
 *
 *           Copyright 2014 Big Switch Networks, Inc.
 *           Copyright 2013 Muxi Technology Corporation.
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
#include <onlp/platformi/sfpi.h>
#include <onlplib/i2c.h>
#include <onlplib/file.h>
#include "platform_lib.h"
#include "x86_64_muxi_dcs6500_48z8c_int.h"
#include "x86_64_muxi_dcs6500_48z8c_log.h"

#define PORT_BUS_INDEX(port) (port+100)
#define PORT_EEPROM_FORMAT              "/sys/bus/i2c/devices/%d-0050/eeprom"

/************************************************************
 *
 * SFPI Entry Points
 *
 ***********************************************************/
/**
 * @brief Initialize the SFPI subsystem.
 */
int onlp_sfpi_init(void)
{
    /* Called at initialization time */
    return ONLP_STATUS_OK;
}

/**
 * @brief Get the bitmap of SFP-capable port numbers.
 * @param bmap [out] Receives the bitmap.
 */
int onlp_sfpi_bitmap_get(onlp_sfp_bitmap_t* bmap)
{
    // return ONLP_STATUS_E_UNSUPPORTED;
    /*
     * Ports {1, 64}
     */
    int p;

    for(p = 1; p <= CHASSIS_XSFP_COUNT; p++) {
        AIM_BITMAP_SET(bmap, p);
    }

    return ONLP_STATUS_OK;
}

/**
 * @brief Determine if an SFP is present.
 * @param port The port number.
 * @returns 1 if present
 * @returns 0 if absent
 * @returns An error condition.
 */
int onlp_sfpi_is_present(int port)
{
    int present = 0;
    int ret = 0;
    ret = s3ip_int_path_get(port, "/sys/s3ip_switch/transceiver/eth", "present", &present);
    if (ret) {
        AIM_LOG_ERROR("Unable to read eth(%d) node(present)\r\n", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    return present;
}

/**
 * @brief Return the presence bitmap for all SFP ports.
 * @param dst Receives the presence bitmap.
 */
int onlp_sfpi_presence_bitmap_get(onlp_sfp_bitmap_t* dst)
{
    unsigned long long presence_all = 0;
    int port_num = 0;
    int present = 0;
    int ret = 0;
    for (port_num = 1; port_num <= CHASSIS_XSFP_COUNT; port_num++) {
        ret = s3ip_int_path_get(port_num, "/sys/s3ip_switch/transceiver/eth", "present", &present);
        if(ret)
        {
            AIM_LOG_ERROR("Unable to read eth(%d) node(present)\r\n", port_num);
            return ONLP_STATUS_E_INTERNAL;
        }
        presence_all |= (present << (port_num-1));
    }

    /* Populate bitmap */
    for(port_num = 1; presence_all; port_num++) {
        AIM_BITMAP_MOD(dst, port_num-1, (presence_all & 0x1));
        presence_all >>= 1;
    }

    return ONLP_STATUS_OK;
}

/**
 * @brief Return the RX_LOS bitmap for all SFP ports.
 * @param dst Receives the RX_LOS bitmap.
 */
int onlp_sfpi_rx_los_bitmap_get(onlp_sfp_bitmap_t* dst)
{
    unsigned long long rxlos_all = 0;
    int port_num = 0;
    int rxlos = 0;
    int ret = 0;
    int present = 0;
    for (port_num = 1; port_num <= CHASSIS_XSFP_COUNT; port_num++) {
        present = onlp_sfpi_is_present(port_num);
        if(present != 1)
        {
            continue;
        }
        ret = s3ip_int_path_get(port_num, "/sys/s3ip_switch/transceiver/eth", "rx_los", &rxlos);
        if(ret)
        {
            AIM_LOG_ERROR("Unable to read eth(%d) node(rx_los)\r\n", port_num);
            return ONLP_STATUS_E_INTERNAL;
        }
        rxlos_all |= (rxlos << (port_num-1));
    }

    /* Populate bitmap */
    for(port_num = 1; rxlos_all; port_num++) {
        AIM_BITMAP_MOD(dst, port_num-1, (rxlos_all & 0x1));
        rxlos_all >>= 1;
    }

    return ONLP_STATUS_OK;
}

/**
 * @brief Read the SFP EEPROM.
 * @param port The port number.
 * @param data Receives the SFP data.
 */
int onlp_sfpi_eeprom_read(int port, uint8_t data[256])
{
    int size = 0;
    int present = 0;
    memset(data, 0, 256);
    present = onlp_sfpi_is_present(port);
    if(present != 1)
    {
        return ONLP_STATUS_E_MISSING;
    }
	if(onlp_file_read(data, 256, &size, PORT_EEPROM_FORMAT, PORT_BUS_INDEX(port)) != ONLP_STATUS_OK) {
        AIM_LOG_ERROR("Unable to read eeprom from port(%d)\r\n", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    if (size != 256) {
        AIM_LOG_ERROR("Unable to read eeprom from port(%d), size is different!\r\n", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    return ONLP_STATUS_OK;
}

/**
 * @brief Read a byte from an address on the given SFP port's bus.
 * @param port The port number.
 * @param devaddr The device address.
 * @param addr The address.
 */
int onlp_sfpi_dev_readb(int port, uint8_t devaddr, uint8_t addr)
{
    int bus = PORT_BUS_INDEX(port);
    int present = 0;
    present = onlp_sfpi_is_present(port);
    if(present != 1)
    {
        return ONLP_STATUS_E_MISSING;
    }
    return onlp_i2c_readb(bus, devaddr, addr, ONLP_I2C_F_FORCE);
}

/**
 * @brief Write a byte to an address on the given SFP port's bus.
 */
int onlp_sfpi_dev_writeb(int port, uint8_t devaddr, uint8_t addr, uint8_t value)
{
    int bus = PORT_BUS_INDEX(port);
    int present = 0;
    present = onlp_sfpi_is_present(port);
    if(present != 1)
    {
        return ONLP_STATUS_E_MISSING;
    }
    return onlp_i2c_writeb(bus, devaddr, addr, value, ONLP_I2C_F_FORCE);
}

/**
 * @brief Read a word from an address on the given SFP port's bus.
 * @param port The port number.
 * @param devaddr The device address.
 * @param addr The address.
 * @returns The word if successful, error otherwise.
 */
int onlp_sfpi_dev_readw(int port, uint8_t devaddr, uint8_t addr)
{
    int bus = PORT_BUS_INDEX(port);
    int present = 0;
    present = onlp_sfpi_is_present(port);
    if(present != 1)
    {
        return ONLP_STATUS_E_MISSING;
    }
    return onlp_i2c_readw(bus, devaddr, addr, ONLP_I2C_F_FORCE);
}

/**
 * @brief Write a word to an address on the given SFP port's bus.
 */
int onlp_sfpi_dev_writew(int port, uint8_t devaddr, uint8_t addr, uint16_t value)
{
    int bus = PORT_BUS_INDEX(port);
    int present = 0;
    present = onlp_sfpi_is_present(port);
    if(present != 1)
    {
        return ONLP_STATUS_E_MISSING;
    }
    return onlp_i2c_writew(bus, devaddr, addr, value, ONLP_I2C_F_FORCE);
}

/**
 * @brief Read from an address on the given SFP port's bus.
 * @param port The port number.
 * @param devaddr The device address.
 * @param addr The address.
 * @returns The data if successful, error otherwise.
 */
int onlp_sfpi_dev_read(int port, uint8_t devaddr, uint8_t addr, uint8_t* rdata, int size)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/**
 * @brief Write to an address on the given SFP port's bus.
 */
int onlp_sfpi_dev_write(int port, uint8_t devaddr, uint8_t addr, uint8_t* data, int size)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}


/**
 * @brief Read the SFP DOM EEPROM.
 * @param port The port number.
 * @param data Receives the SFP data.
 */
int onlp_sfpi_dom_read(int port, uint8_t data[256])
{
    FILE* fp;
    char file[64] = {0};
    int present = 0;
    present = onlp_sfpi_is_present(port);
    if(present != 1)
    {
        return ONLP_STATUS_E_MISSING;
    }
    sprintf(file, PORT_EEPROM_FORMAT, PORT_BUS_INDEX(port));
    fp = fopen(file, "r");
    if(fp == NULL) {
        AIM_LOG_ERROR("Unable to open the eeprom device file of port(%d)", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    if (fseek(fp, 256, SEEK_CUR) != 0) {
        fclose(fp);
        AIM_LOG_ERROR("Unable to set the file position indicator of port(%d)", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    int ret = fread(data, 1, 256, fp);
    fclose(fp);
    if (ret != 256) {
        AIM_LOG_ERROR("Unable to read the module_eeprom device file of port(%d)", port);
        return ONLP_STATUS_E_INTERNAL;
    }

    return ONLP_STATUS_OK;
}
/**
 * @brief Perform any actions required after an SFP is inserted.
 * @param port The port number.
 * @param info The SFF Module information structure.
 * @notes Optional
 */
int onlp_sfpi_post_insert(int port, sff_info_t* info)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/**
 * @brief Returns whether or not the given control is suppport on the given port.
 * @param port The port number.
 * @param control The control.
 * @param rv [out] Receives 1 if supported, 0 if not supported.
 * @note This provided for convenience and is optional.
 * If you implement this function your control_set and control_get APIs
 * will not be called on unsupported ports.
 */
int onlp_sfpi_control_supported(int port, onlp_sfp_control_t control, int* rv)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/**
 * @brief Set an SFP control.
 * @param port The port.
 * @param control The control.
 * @param value The value.
 */
int onlp_sfpi_control_set(int port, onlp_sfp_control_t control, int value)
{
    int ret = ONLP_STATUS_OK;
    int present = 0;

    if (port < 1 || port > CHASSIS_XSFP_COUNT) {
        return ONLP_STATUS_E_UNSUPPORTED;
    }
    present = onlp_sfpi_is_present(port);
    if(present != 1)
    {
        return ONLP_STATUS_E_MISSING;
    }

    switch(control)
        {
        case ONLP_SFP_CONTROL_TX_DISABLE:
            present = onlp_sfpi_is_present(port);
            value = value&0x1;
            ret = s3ip_int_path_set(port, "/sys/s3ip_switch/transceiver/eth", "tx_disable", value);
            if(ret) {
                AIM_LOG_ERROR("Unable to set tx_disable status to port(%d)\r\n", port);
                return ONLP_STATUS_E_INTERNAL;
            }
            break;
        default:
            return ONLP_STATUS_E_UNSUPPORTED;
            break;
        }

    return ret;
}

/**
 * @brief Get an SFP control.
 * @param port The port.
 * @param control The control
 * @param [out] value Receives the current value.
 */
int onlp_sfpi_control_get(int port, onlp_sfp_control_t control, int* value)
{
    int ret = ONLP_STATUS_OK;
    int present = 0;

    if (port < 1 || port > CHASSIS_XSFP_COUNT) {
        return ONLP_STATUS_E_UNSUPPORTED;
    }
    present = onlp_sfpi_is_present(port);
    if(present != 1)
    {
        return ONLP_STATUS_E_MISSING;
    }
    switch(control)
        {
        case ONLP_SFP_CONTROL_RX_LOS:
            ret = s3ip_int_path_get(port, "/sys/s3ip_switch/transceiver/eth", "rx_los", value);
            if(ret)
            {
                AIM_LOG_ERROR("Unable to read eth(%d) node(rx_los)\r\n", port);
                return ONLP_STATUS_E_INTERNAL;
            }
            break;
        case ONLP_SFP_CONTROL_TX_FAULT:
            ret = s3ip_int_path_get(port, "/sys/s3ip_switch/transceiver/eth", "tx_fault", value);
            if(ret)
            {
                AIM_LOG_ERROR("Unable to read eth(%d) node(tx_fault)\r\n", port);
                return ONLP_STATUS_E_INTERNAL;
            }
            break;

        case ONLP_SFP_CONTROL_TX_DISABLE:
            ret = s3ip_int_path_get(port, "/sys/s3ip_switch/transceiver/eth", "tx_disable", value);
            if(ret)
            {
                AIM_LOG_ERROR("Unable to read eth(%d) node(tx_disable)\r\n", port);
                return ONLP_STATUS_E_INTERNAL;
            }
            break;
        default:
            ret = ONLP_STATUS_E_UNSUPPORTED;
        }

    return ret;
}


/**
 * @brief Remap SFP user SFP port numbers before calling the SFPI interface.
 * @param port The user SFP port number.
 * @param [out] rport Receives the new port.
 * @note This function will be called to remap the user SFP port number
 * to the number returned in rport before the SFPI functions are called.
 * This is an optional convenience for platforms with dynamic or
 * variant physical SFP numbering.
 */
int onlp_sfpi_port_map(int port, int* rport)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

/**
 * @brief Deinitialize the SFP driver.
 */
int onlp_sfpi_denit(void)
{
    return ONLP_STATUS_OK;
}

/**
 * @brief Generic debug status information.
 * @param port The port number.
 * @param pvs The output pvs.
 * @notes The purpose of this vector is to allow reporting of internal debug
 * status and information from the platform driver that might be used to debug
 * SFP runtime issues.
 * For example, internal equalizer settings, tuning status information, status
 * of additional signals useful for system debug but not exposed in this interface.
 *
 * @notes This is function is optional.
 */
void onlp_sfpi_debug(int port, aim_pvs_t* pvs)
{
    ;
}

/**
 * @brief Generic ioctl
 * @param port The port number
 * @param The variable argument list of parameters.
 *
 * @notes This generic ioctl interface can be used
 * for platform-specific or driver specific features
 * that cannot or have not yet been defined in this
 * interface. It is intended as a future feature expansion
 * support mechanism.
 *
 * @notes Optional
 */
int onlp_sfpi_ioctl(int port, va_list vargs)
{
    return ONLP_STATUS_E_UNSUPPORTED;
}

