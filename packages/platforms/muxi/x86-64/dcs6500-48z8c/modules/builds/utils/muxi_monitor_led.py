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
    #import logging
    #import logging.config
except ImportError as e:
    raise ImportError('%s - required module not found' % str(e))


###############################################################################
# debug
FUNCTION_NAME = 'led-control'
PRODUCT_NAME  = 'SYSTEM'

SLEEP_TIME = 1 # Loop interval in second.
FAN_WARN_COUNT_MAX = 15
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

def check_debug_flag():
    path_prefix = '/run/'
    if TEST_MODE:
        path_prefix = './test/'
    return os.path.isfile(path_prefix + "fan-control.debug")

def check_show_flag():
    path_prefix = '/run/'
    if TEST_MODE:
        path_prefix = './test/'
    return os.path.isfile(path_prefix + "fan-control.show")

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
LED_DARK = 0
LED_GREEN_ON = 1
LED_YELLOW_ON = 2
LED_RED_ON = 3
LED_GREEN_FLASH_SLOW = 4
LED_YELLOW_FLASH_SLOW = 5
LED_RED_FLASH_SLOW = 6
LED_BLUE_ON = 7
LED_BLUE_FLASH_SLOW = 8
LED_GREEN_FLASH_FAST = 4
LED_YELLOW_FLASH_FAST = 5
LED_RED_FLASH_FAST = 6
LED_BLUE_FLASH_FAST = 8

SYS_LED_PATH = '/sys/switch/sysled/sys_led_status'
ACT_LED_PATH = '/sys/switch/sysled/act_led_status'
BMC_LED_PATH = '/sys/switch/sysled/bmc_led_status'

USB_DATA_FLOW_PATH = '/sys/bus/usb/devices/1-1/urbnum' #number of URBs submitted for the whole device
C3508_USB_DATA_FLOW_PATH = '/sys/bus/usb/devices/1-1/urbnum' #number of URBs submitted for the whole device
D1627_USB_DATA_FLOW_PATH = '/sys/bus/usb/devices/1-1.1/urbnum' #number of URBs submitted for the whole device

cur_usb_data_num = 0
old_usb_data_num = 0

PSU1_PRESENCE_PATH = '/sys/switch/psu/psu1/status'
PSU2_PRESENCE_PATH = '/sys/switch/psu/psu2/status'
PSU1_STATUS_PATH = '/sys/switch/psu/psu1/status'
PSU2_STATUS_PATH = '/sys/switch/psu/psu2/status'
PSU_ABSENT = 0
PSU_PRESENT = 1
PSU_PRESENT_NOT_OK = 2
PSU_OUT_NOT_OK = 2
PSU_OUT_OK = 1
FAN_ABSENT = 0
FAN_PRESENT = 1
FAN_PRESENT_NOT_OK = 2

FAN_NUM_MAX = 4
FAN_STATUS_PATH = "/sys/switch/fan/fan{}/status"
COMe_TYPE = 'C3508'

def init_data():
    global COMe_TYPE
    global USB_DATA_FLOW_PATH
    cmd = 'lscpu | grep "Model name"'
    ret, output = getstatusoutput(cmd)
    if("D-1627" in output):
        COMe_TYPE = 'D1627'
        USB_DATA_FLOW_PATH = D1627_USB_DATA_FLOW_PATH
    else:
        COMe_TYPE = 'C3508'
        USB_DATA_FLOW_PATH = C3508_USB_DATA_FLOW_PATH
    SYS_LOG_INFO('Starting led monitor for '+COMe_TYPE)
    return 0

def get_psu_status():
    try:
        psu1_cmd = 'cat '+PSU1_PRESENCE_PATH
        psu2_cmd = 'cat '+PSU2_PRESENCE_PATH
        ret, output = getstatusoutput(psu1_cmd)
        psu1_present = int(output)
        ret, output = getstatusoutput(psu2_cmd)
        psu2_present = int(output)
        if(psu1_present != PSU_PRESENT) or (psu1_present != PSU_PRESENT):
            return -1
        psu1_sta_cmd = 'cat '+PSU1_STATUS_PATH
        psu2_sta_cmd = 'cat '+PSU2_STATUS_PATH
        ret, output = getstatusoutput(psu1_sta_cmd)
        psu1_sta = int(output)
        ret, output = getstatusoutput(psu2_sta_cmd)
        psu2_sta = int(output)
        if(psu1_sta != PSU_OUT_OK) or (psu2_sta != PSU_OUT_OK):
            return -1
    except:
        return -1
    return 0

def get_fan_status():
    try:
        for index in range(1, FAN_NUM_MAX+1):
            cmd = 'cat '+FAN_STATUS_PATH.format(index)
            ret, output = getstatusoutput(cmd)
            if(ret):
                SYS_LOG_WARN(output)
            fan_status = int(output)
            if(fan_status != FAN_PRESENT):
                return -1
    except:
        return -1
    return 0

def get_fan_all_present_status():
    try:
        for index in range(1, FAN_NUM_MAX+1):
            cmd = 'cat '+FAN_STATUS_PATH.format(index)
            ret, output = getstatusoutput(cmd)
            if(ret):
                SYS_LOG_WARN(output)
            fan_status = int(output)
            if(fan_status == FAN_ABSENT):
                return -1
    except:
        return -1
    return 0

def get_sys_led():
    cmd = "cat "+SYS_LED_PATH
    ret, output = getstatusoutput(cmd)
    return int(output)

def set_sys_led(led):
    cmd = "echo {:d} > {}".format(led, SYS_LED_PATH)
    ret, output = getstatusoutput(cmd)
    return ret

def clear_sys_led_wdt():
    ret = -1
    try:
        get_cmd = "busybox devmem 0xfc801085 8"
        set_cmd = "busybox devmem 0xfc801085 8 0x{:x}"
        ret, output = getstatusoutput(get_cmd)
        if(ret):
            SYS_LOG_WARN("get sys led wdt failed!! %s" % output)
            return ret
        reg_val = int(output, 16) | 0x08
        ret, output = getstatusoutput(set_cmd.format(reg_val))
        if(ret):
            SYS_LOG_WARN("clear sys led wdt failed!! %s" % output)
            return ret
    except:
        SYS_LOG_WARN("clear sys led wdt failed!!")
    return ret

def get_act_led():
    cmd = "cat "+ACT_LED_PATH
    ret, output = getstatusoutput(cmd)
    return int(output)

def set_act_led(led):
    cmd = "echo {:d} > {}".format(led, ACT_LED_PATH)
    ret, output = getstatusoutput(cmd)
    return ret

def get_usb_data_num():
    cmd = "cat "+USB_DATA_FLOW_PATH
    ret, output = getstatusoutput(cmd)
    if("No such file" in output):
        return 0
    return int(output)

fan_warn_count = 0
def sys_led_ctrl():
    global fan_warn_count
    clear_sys_led_wdt()
    psu_status = get_psu_status()
    fan_status = get_fan_status()
    fan_present = get_fan_all_present_status()
    cur_led = get_sys_led()
    # SYS_LOG_INFO(psu_status)
    # SYS_LOG_INFO(fan_status)
    # SYS_LOG_INFO(cur_led)
    if(fan_present):
        fan_warn_count = FAN_WARN_COUNT_MAX
    if(fan_status) :
        if (fan_warn_count < FAN_WARN_COUNT_MAX):
            fan_warn_count = fan_warn_count + 1
    else:
        fan_warn_count = 0
    if(psu_status == 0) and (fan_warn_count < FAN_WARN_COUNT_MAX):
        if(cur_led != LED_GREEN_ON):
            set_sys_led(LED_GREEN_ON)
            SYS_LOG_INFO("set sys led green")
    else:
        if(cur_led != LED_RED_ON):
            set_sys_led(LED_RED_ON)
            SYS_LOG_INFO("set sys led red on")
    return 0

def act_led_ctrl():
    global cur_usb_data_num
    global old_usb_data_num
    global COMe_TYPE
    cur_led = get_act_led()
    cur_usb_data_num = get_usb_data_num()
    if(cur_usb_data_num == 0):
        if(cur_led != LED_DARK):
            set_act_led(LED_DARK)
            SYS_LOG_INFO("set act led dark")
        return 0
    if(cur_usb_data_num != old_usb_data_num):
        diff_data_num = cur_usb_data_num - old_usb_data_num
        old_usb_data_num = cur_usb_data_num
        if(diff_data_num > 4):
            if(cur_led != LED_GREEN_FLASH_FAST):
                set_act_led(LED_GREEN_FLASH_FAST)
                SYS_LOG_INFO("set act led green flash fast")
        else:
            if(cur_led != LED_GREEN_ON):
                set_act_led(LED_GREEN_ON)
                SYS_LOG_INFO("set act led green on")
    return 0

def bmc_led_ctrl():
    global cur_usb_data_num
    global old_usb_data_num
    global COMe_TYPE
    cur_led = get_act_led()
    cur_usb_data_num = get_usb_data_num()
    if(cur_usb_data_num == 0):
        if(cur_led != LED_DARK):
            set_act_led(LED_DARK)
            SYS_LOG_INFO("set act led dark")
        return 0
    if(cur_usb_data_num != old_usb_data_num):
        diff_data_num = cur_usb_data_num - old_usb_data_num
        old_usb_data_num = cur_usb_data_num
        if(diff_data_num > 4):
            if(cur_led != LED_GREEN_FLASH_FAST):
                set_act_led(LED_GREEN_FLASH_FAST)
                SYS_LOG_INFO("set act led green flash fast")
        else:
            if(cur_led != LED_GREEN_ON):
                set_act_led(LED_GREEN_ON)
                SYS_LOG_INFO("set act led green on")
    return 0

def check_bmc_status():
    global BMC_STATUS

    status, output = getstatusoutput("cat /sys/switch/bmc/status")
    # check if BMC OK
    if(status == 0) and (output == "1"):
        BMC_STATUS_1 = True
        # SYS_LOG_INFO("BMC present and work.")
    else:
        BMC_STATUS_1 = False
        # SYS_LOG_INFO("BMC not exist.")

    status, output = getstatusoutput("cat /sys/switch/bmc/enable")
    # check if BMC enable
    if(status == 0) and (output == "1"):
        BMC_STATUS_2 = True
        # SYS_LOG_INFO("BMC enable.")
    else:
        BMC_STATUS_2 = False
        # SYS_LOG_INFO("BMC disable.")
    if(BMC_STATUS_1 == True) and (BMC_STATUS_2 == True):
        BMC_STATUS = True
    else:
        BMC_STATUS = False

    return BMC_STATUS

def main(argv):
    global TEST_MODE, DEBUG_MODE, SHOW_MODE
    # Check test mode flag.
    if len(argv) > 1 :
        if argv[1] == 'test':
            TEST_MODE = True

    # Wait 60 seconds for system to finish the initialization.
    if not TEST_MODE:
        SYS_LOG_INFO('Waiting for system finish init...')
        while not os.path.exists(DRIVER_INIT_DONE_FLAG):
            time.sleep(2)
        SYS_LOG_INFO('Waiting for system finish init...done.')
    # Init
    ret = init_data()
    if ret != 0:
        SYS_LOG_CRITICAL('Fail to init program data, %d, exiting...' %(ret))
        exit(ret)
    # Loop forever, doing something useful hopefully:
    while True:
        check_bmc_status()
        if BMC_STATUS == True:
            get_fan_status() # for switch fan driver status refresh
            pass
        else:
            sys_led_ctrl()
        act_led_ctrl()
        time.sleep(SLEEP_TIME)

if __name__ == '__main__':
    main(sys.argv[1:])
