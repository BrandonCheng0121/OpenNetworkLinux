#!/usr/bin/env python
#
# Copyright (C) 2018 Accton Technology Corporation
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

# ------------------------------------------------------------------
# HISTORY:
#    mm/dd/yyyy (A.D.)
#    7/2/2018:  Jostar create for as7326-56x
# ------------------------------------------------------------------

try:
    import os
    import sys
    import syslog
    import signal
    import time
    import glob
    import struct
    # from ctypes import create_string_buffer
except ImportError as e:
    raise ImportError('%s - required module not found' % str(e))


###############################################################################
# debug
FUNCTION_NAME = 'led-control'
PRODUCT_NAME  = 'SYSTEM'

SLEEP_TIME = 1 # Loop interval in second.
RETRY_COUNT_MAX = 3
TEMP_HISTORY_MAX = RETRY_COUNT_MAX

TEST_MODE = False
DEBUG_MODE = False
SHOW_MODE = False
###############################################################################
# LOG.
BMC_STATUS = False
DRIVER_INIT_DONE_FLAG = "/run/sonic_init_driver_done.flag"
# priorities (these are ordered)

LOG_EMERG     = 0       #  system is unusable
LOG_ALERT     = 1       #  action must be taken immediately
LOG_CRIT      = 2       #  critical conditions
LOG_ERR       = 3       #  error conditions
LOG_WARNING   = 4       #  warning conditions
LOG_NOTICE    = 5       #  normal but significant condition
LOG_INFO      = 6       #  informational
LOG_DEBUG     = 7       #  debug-level messages

'''
priority_names = {
    "alert":    LOG_ALERT,
    "crit":     LOG_CRIT,
    "critical": LOG_CRIT,
    "debug":    LOG_DEBUG,
    "emerg":    LOG_EMERG,
    "err":      LOG_ERR,
    "error":    LOG_ERR,        #  DEPRECATED
    "info":     LOG_INFO,
    "notice":   LOG_NOTICE,
    "panic":    LOG_EMERG,      #  DEPRECATED
    "warn":     LOG_WARNING,    #  DEPRECATED
    "warning":  LOG_WARNING,
}
'''
priority_name = {
    LOG_CRIT    : "critical",
    LOG_DEBUG   : "debug",
    LOG_WARNING : "warning",
    LOG_INFO    : "info",
}


def SYS_LOG(level, msg):
    if TEST_MODE:
        time_str = time.strftime("%Y-%m-%dT%H:%M:%S", time.localtime())
        print('%s %s: %-12s %s' %(time_str, FUNCTION_NAME, priority_name[level].upper(), msg))
    else:
        syslog.syslog(level, msg)

def DBG_LOG(msg):
    if DEBUG_MODE:
        level = syslog.LOG_DEBUG
        x = PRODUCT_NAME + ' ' + priority_name[level].upper() + ' : ' + msg
        SYS_LOG(level, x)

def SYS_LOG_INFO(msg):
    level = syslog.LOG_INFO
    x = PRODUCT_NAME + ' ' + priority_name[level].upper() + ' : ' + msg
    SYS_LOG(level, x)

def SYS_LOG_WARN(msg):
    level = syslog.LOG_WARNING
    x = PRODUCT_NAME + ' ' + priority_name[level].upper() + ' : ' + msg
    SYS_LOG(level, x)

def SYS_LOG_CRITICAL(msg):
    level = syslog.LOG_CRIT
    x = PRODUCT_NAME + ' ' + priority_name[level].upper() + ' : ' + msg
    SYS_LOG(level, x)

###############################################################################

def getstatusoutput(cmd):
    if sys.version_info.major == 2:
        # python2
        import commands
        return commands.getstatusoutput( cmd )
    else:
        # python3
        import subprocess
        return subprocess.getstatusoutput( cmd )

###############################################################################

# Define
MP2973_AVS_PATH = '/sys/bus/i2c/devices/4-0020/hwmon/hwmon*/avs_node'
AVS_EEPROM_PATH = "/sys/bus/i2c/devices/0-0050/eeprom"
MP2973_OUT1_PATH = "/sys/switch/sensor/in6/in_input"

EEPROM_WP_EN = 1
EEPROM_WP_UEN = 0
AVS_EEPROM_OFFSET = 513

MP2973_AVS_STEP = 0.01 # V
MP2973_AVS_DEFAULT = 0x28 # 0.8875v
MP2973_AVS_MAX = 0x33 # 0.9975v
MP2973_AVS_MIN = 0x1F # 0.7975v

EEPROM_AVS_STEP = 0.00625 # V
EEPROM_AVS_DEFAULT = 0x74 # 0.8875v
EEPROM_AVS_MAX = 0x82 # 0.800v
EEPROM_AVS_MIN = 0x62 # 1.000v

def eepromAVS_to_mp2973AVS(avs_eeprom):
    if(avs_eeprom > EEPROM_AVS_MAX) or (avs_eeprom < EEPROM_AVS_MIN):
        print("eeprom avs val is error,please check it!")
        return MP2973_AVS_DEFAULT
    mp2973_avs_float = (EEPROM_AVS_MAX - float(avs_eeprom))/(EEPROM_AVS_MAX - EEPROM_AVS_MIN) * (MP2973_AVS_MAX - MP2973_AVS_MIN) + MP2973_AVS_MIN
    print(mp2973_avs_float)
    mp2973_avs = int(mp2973_avs_float)
    return mp2973_avs


def mp2973_avs_get_from_bmc():
    cmd = "ipmitool raw 0x36 0xa5 0x1 0x00"
    ret, output = getstatusoutput(cmd)
    if(ret):
        print(output)
        return ret
    return int(output.split()[3],16)

def mp2973_avs_set_to_bmc(data):
    avs_path_cmd = "ipmitool raw 0x36 0xa5 0x0 0x{:x}".format(data)
    ret, output = getstatusoutput(avs_path_cmd)
    if(ret):
        print(output)
        return ret
    return 0

def mp2973_avs_get():
    cmd = "cat "+MP2973_AVS_PATH
    ret, output = getstatusoutput(cmd)
    if(ret):
        print(output)
        return ret
    return int(output,16)

def mp2973_avs_get_from_node():
    cmd = "cat "+MP2973_OUT1_PATH
    ret, output = getstatusoutput(cmd)
    if(ret or "NA" in output):
        print(output)
        return ret
    return float(output)

def mp2973_avs_set(data):
    avs_path_cmd = "ls {:s}".format(MP2973_AVS_PATH)
    ret, output = getstatusoutput(avs_path_cmd)
    if(ret):
        print(output)
        return ret
    avs_path_str = output
    cmd = "echo 0x{:x} > {:s}".format(data, avs_path_str)
    print(cmd)
    ret, output = getstatusoutput(cmd)
    if(ret):
        print(output)
        return ret
    return 0

def avs_eeprom_get():
    try:
        file_eeprom = open(AVS_EEPROM_PATH, mode="rb", buffering=0)
    except Exception:
        print("open eeprom failed!")
        return -1
    try:
        file_eeprom.seek(AVS_EEPROM_OFFSET)
        eeprom_raw = file_eeprom.read(1)
    except Exception:
        print(AVS_EEPROM_PATH+" read failed!" )
    finally:
        val = struct.unpack('B', eeprom_raw)
        print("eeprom val = 0x%x " % val[0])
        if file_eeprom:
            file_eeprom.close()
    return val[0]


def avs_eeprom_set(data):
    try:
        file_eeprom = open(AVS_EEPROM_PATH, mode="wb", buffering=0)
    except Exception:
        print("open eeprom failed!")
        return -1
    try:
        file_eeprom.seek(AVS_EEPROM_OFFSET)
        content = struct.pack("B",data)
        eeprom_wp(EEPROM_WP_UEN)
        file_eeprom.write(content)
        eeprom_wp(EEPROM_WP_EN)
        print('content',content)
    except Exception:
        print(AVS_EEPROM_PATH+" read failed!" )
    finally:
        if file_eeprom:
            file_eeprom.close()

def eeprom_wp(enable):
    if(enable):
        cmd = "busybox devmem 0xfc801012 8 0xB0"
        ret, output = getstatusoutput(cmd)
    else:
        cmd = "busybox devmem 0xfc801012 8 0x90"
        ret, output = getstatusoutput(cmd)

def check_bmc_status():
    global BMC_STATUS

    status, output = getstatusoutput("cat /sys/switch/bmc/status")
    # check if BMC OK
    if(status == 0) and (output == "1"):
        BMC_STATUS_1 = True
        print("BMC present and work.")
    else:
        BMC_STATUS_1 = False
        print("BMC not exist.")

    status, output = getstatusoutput("cat /sys/switch/bmc/enable")
    # check if BMC enable
    if(status == 0) and (output == "1"):
        BMC_STATUS_2 = True
        print("BMC enable.")
    else:
        BMC_STATUS_2 = False
        print("BMC disable.")
    if(BMC_STATUS_1 == True) and (BMC_STATUS_2 == True):
        BMC_STATUS = True
    else:
        BMC_STATUS = False

    return BMC_STATUS

def main(argv):
    avs_eeprom = avs_eeprom_get()
    avs_eeprom_vol = 1 - EEPROM_AVS_STEP*(avs_eeprom-EEPROM_AVS_MIN)
    print ("get eeprom_avs val = 0x{:x} equivalent:{:.03}V".format(avs_eeprom, avs_eeprom_vol) )
    mp2973_avs = eepromAVS_to_mp2973AVS(avs_eeprom)
    mp2973_avs_vol = MP2973_AVS_STEP*(mp2973_avs-MP2973_AVS_MIN)+0.8
    print ("need to set mp2973_avs = 0x{:x} equivalent:{:.03}V".format(mp2973_avs, mp2973_avs_vol))
    check_bmc_status()
    if BMC_STATUS == True:
        print("mp2973 avs val set as bmc")
        print("get mp2973_avs val = 0x%x" % mp2973_avs_get_from_bmc())
        mp2973_avs_set_to_bmc(mp2973_avs)
        mp2973_avs_new = mp2973_avs_get_from_bmc()
        mp2973_avs_new_vol = MP2973_AVS_STEP*(mp2973_avs_new-MP2973_AVS_MIN)+0.8
        print("get mp2973_avs val = 0x{:x}  equivalent:{:.03}V".format(mp2973_avs_new, mp2973_avs_new_vol))
        if(mp2973_avs == mp2973_avs_new):
            print("current mp2973 avs vol:{:.03}V".format(mp2973_avs_get_from_node()))
            print("mp2973 avs val set success")
        else:
            print("mp2973 avs val set failed! expect = 0x{:x}, current = 0x{:x}".format(mp2973_avs, mp2973_avs_new))
        # todo send date to BMC set avs vol
    else:
        print("get mp2973_avs val = 0x%x" % mp2973_avs_get())
        mp2973_avs_set(mp2973_avs)
        mp2973_avs_new = mp2973_avs_get()
        mp2973_avs_new_vol = MP2973_AVS_STEP*(mp2973_avs_new-MP2973_AVS_MIN)+0.8
        print("get mp2973_avs val = 0x{:x}  equivalent:{:.03}V".format(mp2973_avs_new, mp2973_avs_new_vol))
        if(mp2973_avs == mp2973_avs_new):
            print("current mp2973 avs vol:{:.03}V".format(mp2973_avs_get_from_node()))
            print("mp2973 avs val set success")
        else:
            print("mp2973 avs val set failed! expect = 0x{:x}, current = 0x{:x}".format(mp2973_avs, mp2973_avs_new))
        # avs_eeprom_set(0x75)

if __name__ == '__main__':
    main(sys.argv[1:])
