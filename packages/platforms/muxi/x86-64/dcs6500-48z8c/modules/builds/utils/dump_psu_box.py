#!/usr/bin/python3

import os
import sys
import time
import math
from datetime import datetime, timedelta

STATUS_WORD_L_tbl =  [
    # 0:ALARM bit 1:ALARM name
    [0x7,   "UNIT WAS BUSY",],
    [0x6,   "UNIT IS OFF",],
    [0x5,   "VOUT_OV_FAULT",],
    [0x4,   "IOUT_OC_FAULT",],
    [0x3,   "VIN_UV_FAULT",],
    [0x2,   "TEMPERATURE FAULT OR WARNING",],
    [0x1,   "COMM, MEMORY, LOGIC EVENT",],
    [0x0,   "NONE OF THE ABOVE",],
]

STATUS_WORD_H_tbl =  [
    # 0:ALARM bit 1:ALARM name
    [0x7,   "VOUT FAULT OR WARNING",],
    [0x6,   "IOUT/POUT FAULT OR WARNING",],
    [0x5,   "INPUT FAULT OR WARNING",],
    [0x4,   "MFR_SPECIFIC2",],
    [0x3,   "POWER_GOOD Negated",],
    [0x2,   "FAN FAULT OR WARNING",],
    [0x1,   "OTHER",],
    [0x0,   "UNKNOWN FAULT OR WARNING",],
]

STATUS_IOUT_tbl =  [
    # 0:ALARM bit 1:ALARM name
    [0x7,   "IOUT OC Fault",],
    [0x6,   "IOUT OC Fault w/ LV Shutdown",],
    [0x5,   "IOUT OC Warning",],
    [0x4,   "IOUT UC Fault",],
    [0x3,   "Current Share Fault",],
    [0x2,   "In Power Limiting Mode",],
    [0x1,   "POUT OP Fault",],
    [0x0,   "POUT OP Warning",],
]

STATUS_INPUT_tbl =  [
    # 0:ALARM bit 1:ALARM name
    [0x7,   "VIN OV Fault",],
    [0x6,   "VIN OV Warning",],
    [0x5,   "VIN UV Warning",],
    [0x4,   "VIN UV Fault",],
    [0x3,   "Unit Off For Low Input Voltage",],
    [0x2,   "IIN OC Fault",],
    [0x1,   "IIN OC Warning",],
    [0x0,   "PIN OP Warning",],
]

STATUS_TEMP_tbl =  [
    # 0:ALARM bit 1:ALARM name
    [0x7,   "OT Fault",],
    [0x6,   "OT Warning",],
    [0x5,   "UT Warning",],
    [0x4,   "UT Fault",],
    [0x3,   "Reserved",],
    [0x2,   "Reserved",],
    [0x1,   "Reserved",],
    [0x0,   "Reserved",],
]

STATUS_FAN_tbl =  [
    # 0:ALARM bit 1:ALARM name
    [0x7,   "Fan 1 Fault",],
    [0x6,   "Fan 2 Fault",],
    [0x5,   "Fan 1 Warning",],
    [0x4,   "Fan 2 Warning",],
    [0x3,   "Fan 1 Speed Override",],
    [0x2,   "Fan 2 Speed Override",],
    [0x1,   "Air Flow Fault",],
    [0x0,   "Air Flow Warning",],
]

def dump_help():
    print("Usage: %s PSU-INDEX [CMD]" % (sys.argv[0]))
    print("PSU-INDEX: psu1 / psu2")
    print("CMD: timesync")

def call_retcode(cmd):
    if sys.version_info.major == 2:
        # python2
        import commands
        return commands.getstatusoutput( cmd )
    else:
        # python3
        import subprocess
        return subprocess.getstatusoutput( cmd )

def twos_comp(val, bits):
    """compute the 2's complement of int value val"""
    if (val & (1 << (bits - 1))) != 0: # if sign bit is set e.g., 8bit: 128-255
        val = val - (1 << bits)        # compute negative value
    return val                         # return positive value as is

def linear11(h_byte, l_byte):
    mantissa = twos_comp((l_byte + (h_byte<<8)) & 0x7FF, 11)
    exponent = twos_comp((h_byte >> 3) & 0x1F, 5)
    return mantissa * math.pow(2, exponent)

def parse_time(psu_time_str):
    data = psu_time_str.split()
    numbase = 16
    time_int = (int(data[3], numbase)<<24) + (int(data[2], numbase)<<16) + (int(data[1], numbase)<<8) + (int(data[0], numbase))
    base = datetime(1970, 1, 1)
    log_time = base + timedelta(seconds=time_int)
    return log_time

def reg_parse(val, tbl):
    for alarm in tbl:
        if val & (0x1<<alarm[0]):
            print ("    {}".format(alarm[1]))

def parse_black_box(psu_log_str):
    data = psu_log_str.split()
    ff_count = 0
    for val in data:
        if val == "ff" or val == "0xff":
            ff_count+=1
    if ff_count >= 20:
        print("No PSU black box data.")
        return

    numbase = 16
    time_int = (int(data[3], numbase)<<24) + (int(data[2], numbase)<<16) + (int(data[1], numbase)<<8) + (int(data[0], numbase))
    base = datetime(1970, 1, 1)
    log_time = base + timedelta(seconds=time_int)

    status_word_l = int(data[4], numbase)
    status_word_h = int(data[5], numbase)
    status_iout = int(data[6], numbase)
    status_input = int(data[7], numbase)
    status_temp = int(data[8], numbase)
    status_fans = int(data[9], numbase)
    vin = linear11(int(data[11], numbase), int(data[10], numbase))
    iin = linear11(int(data[13], numbase), int(data[12], numbase))
    iout = linear11(int(data[15], numbase), int(data[14], numbase))
    temp_t1 = linear11(int(data[17], numbase), int(data[16], numbase))
    temp_t2 = linear11(int(data[19], numbase), int(data[18], numbase))
    fan_speed = linear11(int(data[21], numbase), int(data[20], numbase))
    pin = linear11(int(data[23], numbase), int(data[22], numbase))
    vout = ((int(data[25], numbase) << 8)+ int(data[24], numbase)) / 500

    # print('%19s %13s %13s %13s %13s %13s %13s %13s %13s %13s %13s %13s %13s %13s %13s' % \
    #     ("TIME", "STATUS_WORD_L", "STATUS_WORD_H", "STATUS_IOUT", \
    #     "STATUS_INPUT", "STATUS_TEMP", "STATUS_FAN", \
    #     "VIN", "IIN", "IOUT", "TEMP_1", "TEMP_2", \
    #     "FAN_SPEED", "PIN", "VOUT"))
    # print(log_time, \
    #     '         0x%02x          0x%02x          0x%02x' % (status_word_l, status_word_h, status_iout), \
    #     '         0x%02x          0x%02x          0x%02x' % (status_input, status_temp, status_fans), \
    #     " {:>12.3f}".format(vin), " {:>12.3f}".format(iin), " {:>12.3f}".format(iout), \
    #     " {:>12.3f}".format(temp_t1), " {:>12.3f}".format(temp_t2), " {:>12.3f}".format(fan_speed), \
    #     " {:>12.3f}".format(pin), " {:>12.3f}".format(vout)
    #     )
    print("TIME           : {}".format(log_time) )
    print("STATUS_WORD_L  : 0x{:02x}".format(status_word_l) )
    reg_parse(status_word_l, STATUS_WORD_L_tbl)
    print("STATUS_WORD_H  : 0x{:02x}".format(status_word_h) )
    reg_parse(status_word_h, STATUS_WORD_H_tbl)
    print("STATUS_IOUT    : 0x{:02x}".format(status_iout) )
    reg_parse(status_iout, STATUS_IOUT_tbl)
    print("STATUS_INPUT   : 0x{:02x}".format(status_input) )
    reg_parse(status_input, STATUS_INPUT_tbl)
    print("STATUS_TEMP    : 0x{:02x}".format(status_temp) )
    reg_parse(status_temp, STATUS_TEMP_tbl)
    print("STATUS_FAN     : 0x{:02x}".format(status_fans) )
    reg_parse(status_fans, STATUS_FAN_tbl)
    print("PIN            : {:.3f} W".format(pin) )
    print("VIN            : {:.3f} V".format(vin) )
    print("IIN            : {:.3f} A".format(iin) )
    print("VOUT           : {:.3f} V".format(vout) )
    print("IOUT           : {:.3f} A".format(iout) )
    print("TEMP_1         : {:.3f} C".format(temp_t1) )
    print("TEMP_2         : {:.3f} C".format(temp_t2) )
    print("FAN_SPEED      : {:.3f} rpm".format(fan_speed) )

def __main__():
    if ((len(sys.argv) != 2) and (len(sys.argv) != 3)):
        dump_help()
        return
    elif (sys.argv[1] == "psu1"):
        channel = 1
        addr = 0x5a
        ipmi_psu_index = 1
    elif (sys.argv[1] == "psu2"):
        channel = 2
        addr = 0x59
        ipmi_psu_index = 2
    else:
        dump_help()
        return

    ret, output = call_retcode("cat /sys/switch/bmc/status")
    bmc_is_ok = output.strip()
    ret, output = call_retcode("cat /sys/switch/bmc/enable")
    bmc_enable = output.strip()
    if (len(sys.argv) == 3):
        if (sys.argv[2] == "timesync"):
            if (bmc_enable == "0"):
                ret, output = call_retcode("date +%s")
                system_timestamp = int(output)
                call_retcode("i2cset -f -y {} {} 0xdd {} {} {} {} s".format(channel, addr, \
                                                                        (system_timestamp & 0x000000ff),
                                                                        (system_timestamp & 0x0000ff00)>>8,
                                                                        (system_timestamp & 0x00ff0000)>>16,
                                                                        (system_timestamp & 0xff000000)>>24))
                ret, ret_str = call_retcode("psu_i2cget -f -y {} {} 0xdd s".format(channel, addr))
                print("PSU time:{}".format(parse_time(ret_str)))
        else:
            dump_help()
            return
    else:

        if (bmc_is_ok == "1" and bmc_enable == "1"):
            read_log_cmd = "ipmitool raw 0x36 0xa7 {} {}".format(ipmi_psu_index, 1)
        else:
            read_log_cmd = "psu_i2cget -f -y {} {} 0xdc s".format(channel, addr)
        ret, ret_str = call_retcode(read_log_cmd)
        if ret:
            print("No PSU black box data.")
            return
        parse_black_box(ret_str)

__main__()
