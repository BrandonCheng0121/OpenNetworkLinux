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

###############################################################################

PRODUCT_NAME  = ''
FUNCTION_NAME = 'bmc_log_watchdog'

DEBUG_MODE = False

###############################################################################

WDT_TIMEOUT = 180           # Timeout value, in second.
WDT_KICK_DOG_INTERVAL = 5   # Kick watchdog interval, in second.

###############################################################################
# LOG.

# priorities (these are ordered)

LOG_EMERG     = 0       #  system is unusable
LOG_ALERT     = 1       #  action must be taken immediately
LOG_CRIT      = 2       #  critical conditions
LOG_ERR       = 3       #  error conditions
LOG_WARNING   = 4       #  warning conditions
LOG_NOTICE    = 5       #  normal but significant condition
LOG_INFO      = 6       #  informational
LOG_DEBUG     = 7       #  debug-level messages

priority_name = {
    LOG_CRIT    : "critical",
    LOG_DEBUG   : "debug",
    LOG_WARNING : "warning",
    LOG_INFO    : "info",
}


def SYS_LOG(level, msg):
    '''
    if TEST_MODE:
        print('%s: %-12s %s' %(FUNCTION_NAME, priority_name[level].upper(), msg))
    else:
        syslog.syslog(level, msg)
    '''
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

def check_debug_flag():
    path_prefix = '/run/'
    return os.path.isfile(path_prefix + "bmc-wdt-control.debug")

###############################################################################

def set_timeout(wdt_timeout):
    time_l = (wdt_timeout*10)&0xff
    time_h = ((wdt_timeout*10)&0xff00) >> 8
    cmd = 'ipmitool raw 0x06 0x24 0x03 0x00 0x00 0x00 0x{:x} 0x{:x}'.format(time_l, time_h)
    ret, output = getstatusoutput(cmd)
    if ret != 0:
        SYS_LOG_WARN('Failed to configure watchdog, {}'.format(output))
        return False
    time.sleep(1)
    cmd = 'ipmitool mc watchdog get'
    ret, output = getstatusoutput(cmd)
    if ret != 0:
        SYS_LOG_WARN('Failed to get watchdog configure {}'.format(output))
        return False

    cmd = 'ipmitool mc watchdog get |grep "Initial Countdown"'
    ret, output = getstatusoutput(cmd)
    if ret != 0:
        SYS_LOG_WARN('Failed to get watchdog configure {}'.format(output))
        return False
    set_time = int(output.split()[2])
    if(set_time == wdt_timeout):
        SYS_LOG_INFO('Set watchdog timeout {}s, success'.format(set_time))
    else:
        SYS_LOG_INFO('Failed to set watchdog timeout {}'.format(output))
        return False

    return True

def feed_cpld_wdt():
    # Watchdog is used to trigger BMC logging
    cmd = 'ipmitool mc watchdog reset'
    ret, output = getstatusoutput(cmd)
    if ret != 0:
        SYS_LOG_WARN('Failed to feed cpld watchdog, {}'.format(output))
        return False
    return True

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
    global DEBUG_MODE

    SYS_LOG_INFO('This watchdog is used to trigger BMC logging when the Sonic crashes')
    SYS_LOG_INFO('Starting bmc log watchdog program...')
    # If BMC is present, stop the set time task
    bmc_present = get_bmc_present_status()
    if bmc_present is False:
        SYS_LOG_INFO('BMC not present. exit bmc log watchdog program.')
        exit()

    while True:
        if not set_timeout(WDT_TIMEOUT):
            SYS_LOG_WARN("Failed to set WDT timeout")
            time.sleep(5)
        else:
            break
    # Main loop.
    while True:
        DEBUG_MODE = check_debug_flag()
        if DEBUG_MODE:
            ret, date_str = getstatusoutput('date')
            DBG_LOG('Running WDT, date = [{}]'.format(date_str))

        feed_cpld_wdt()
        # Sleep
        time.sleep(WDT_KICK_DOG_INTERVAL)


if __name__ == '__main__':
    main(sys.argv)


