#!/usr/bin/env python

try:
    import os
    import sys
    import time

except ImportError as e:
    raise ImportError('%s - required module not found' % str(e))

###############################################################################

SLEEP_TIME = 60 # Loop interval in second.
DRIVER_INIT_DONE_FLAG = "/run/sonic_init_driver_done.flag"
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

def set_bmc_time_task():
    cmd = 'date -u \"+%m/%d/%Y %T\"'
    ret, sys_time = getstatusoutput(cmd)
    cmd = 'ipmitool sel time set \"%s\"' % (sys_time)
    ret, output = getstatusoutput(cmd)
    if ret != 0 or len(output) == 0:
        print('cmd: %s failed' % (cmd))

def get_bmc_present_status():
    bmc_is_present = False
    ret, output = getstatusoutput('cat /sys/switch/bmc/status')
    print('Get BMC status ret: %d, output: "%s"' %(ret, output))

    try:
        if ret != 0 or len(output) == 0:
            print('fail to read bmc present status.')
        else:
            if (int(output, 0) == 1):
                bmc_is_present = True

    except Exception as e:
        print(repr(e))

    print('BMC present status is %d' %(1 if bmc_is_present else 0))
    return bmc_is_present

def main(argv):
    # wait until driver ready
    while not os.path.isfile(DRIVER_INIT_DONE_FLAG):
        time.sleep(10)

    # If BMC is present, stop the set time task
    bmc_present = get_bmc_present_status()
    if bmc_present is False:
        exit()

    # Main loop.
    while True:
        set_bmc_time_task()

        # Sleep
        time.sleep(SLEEP_TIME)


if __name__ == '__main__':
    main(sys.argv)
