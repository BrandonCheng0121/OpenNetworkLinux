#!/usr/bin/env python

try:
    import os
    import sys
    import syslog
    import signal
    import time
    import glob

except ImportError as e:
    raise ImportError('%s - required module not found' % str(e))


SLEEP_TIME = 5 # Loop interval in second.

DRIVER_INIT_DONE_FLAG = "/run/sonic_init_driver_done.flag"
PSU_STATUS_PATH = '/sys/switch/psu/psu{}/status'
PSU_NUM = 2
PSU_STATUS_ABSENT = 0x0
PSU_STATUS_OK     = 0x1
PSU_STATUS_NOT_OK = 0x2

RET_OK = 0
RET_ERR = -1

BMC_ENABLE = 1
BMC_DISABLE = 0
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

def SYS_LOG(level, x):
    syslog.syslog(level, x)

def SYS_LOG_ERR(x):
    SYS_LOG(syslog.LOG_ERR, x)

def SYS_LOG_WARN(x):
    SYS_LOG(syslog.LOG_WARNING, x)

def SYS_LOG_INFO(x):
    SYS_LOG(syslog.LOG_INFO, x)

###############################################################################

def psu_get_status(index):
    cmd = 'cat %s' % (PSU_STATUS_PATH)
    try:
        ret, output = getstatusoutput(cmd.format(index))
        if ret:
            SYS_LOG_ERR("get psu status failed! ret:{} output:{}".format(ret, output))
            return RET_ERR, 0
        psu_status = int(output)
    except Exception as _e:
        SYS_LOG_ERR(repr(_e))
        return RET_ERR, 0
    return RET_OK, psu_status

def psu_set_time_task(psu_num):
    for psu_i in range(psu_num):
        psu_index = psu_i+1
        ret, psu_status = psu_get_status(psu_index)
        if(ret != RET_OK):
            SYS_LOG_WARN("psu set time failed! ret:{} output:{}".format(ret, output))
            return RET_ERR
        if psu_status != PSU_STATUS_ABSENT:
            cmd = "dump_psu_box.py psu{} timesync".format(psu_index)
            ret, output = getstatusoutput(cmd)
            if ret:
                SYS_LOG_WARN("psu set time failed! ret:{} output:{}".format(ret, output))
                return RET_ERR
        else:
            SYS_LOG_INFO("psu{} is not present, can not set time".format(psu_index))
    return RET_OK

def get_bmc_enable():
    bmc_is_enable = BMC_DISABLE
    try:
        ret, output = getstatusoutput("cat /sys/switch/bmc/enable")
        if ret:
            SYS_LOG_ERR("fail to read bmc enable status.")
        else:
            if int(output.strip()) == BMC_ENABLE:
                bmc_is_enable = BMC_ENABLE
    except Exception as _e:
        SYS_LOG_ERR(repr(_e))
        SYS_LOG_ERR("get bmc enable ret:{}, output:{}".format(ret, output))

    SYS_LOG_INFO("BMC enable status is {}".format(bmc_is_enable))
    return bmc_is_enable

def main():

    # wait until driver ready
    while not os.path.exists(DRIVER_INIT_DONE_FLAG):
        time.sleep(2)

    # If BMC is present, stop the psu monitor task
    bmc_enable = get_bmc_enable()
    if bmc_enable is BMC_ENABLE:
        SYS_LOG_INFO("BMC is enable, psu timing is executed by bmc")
        exit()

    count = 0
    # Main loop.
    while True:
        count += 1
        if count >= 600/SLEEP_TIME:
            count = 0
            psu_set_time_task(PSU_NUM)

        # Sleep
        time.sleep(SLEEP_TIME)

if __name__ == '__main__':
    main()
