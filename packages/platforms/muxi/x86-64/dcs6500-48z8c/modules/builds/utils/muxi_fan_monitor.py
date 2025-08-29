#!/usr/bin/env python

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

# LED_CTRL_PATH = '/sys/class/leds/dx510_fan/brightness'

# LED_OFF     = 0
# LED_RED     = 10
# LED_GREEN   = 16

###############################################################################

FUNCTION_NAME = 'fan-control'
PRODUCT_NAME  = 'SYSTEM'

THERMAL_CONFIG_FILE = 'thermal_sensor_pid.config'
FAN_CONFIG_FILE = 'fan_sysfs.config'

SLEEP_TIME = 10 # Loop interval in second.
RETRY_COUNT_MAX = 3
TEMP_HISTORY_MAX = RETRY_COUNT_MAX

TEST_MODE = False
DEBUG_MODE = False
SHOW_MODE = False

NONE_VALUE = 'N/A'

###############################################################################
# Thermal sensor.
THERMAL_NUM_MAX = 0

THERMAL_INIT_VAL = None
THERMAL_INVALID_VAL = -10000

THERMAL_VAL_MIN = -40
THERMAL_VAL_MAX = 125
THERMAL_BURST_MAX = 30
THERMAL_HYSTERESIS = 3

THERMAL_FATAL_MAX = 2

THERMAL_CONFIG = {}
###############################################################################
# Fan sensor.
FAN_NUM_MAX = 4

FAN_PWM_MAX = 100
FAN_PWM_MIN = 30
FAN_PWM_DEFAULT = 50

FAN_RPM_LOW = 2500 # 10%
FAN_RPM_OFFSET_MAX = 10

FAN_ABNORMAL_MAX = 2

FAN_SYS_PATH_PREFIX = '/sys/cpld'

FAN_CONFIG = {}

FAN_NOT_PRESENT = 0

###############################################################################
# Alarm.
ALARM_STATE = {}

# Thermal alarm.
THERMAL_ALARM_INACCESS  = 'INACCESS'
THERMAL_ALARM_INVALID   = 'INVALID'
THERMAL_ALARM_BURST     = 'BURST'
THERMAL_ALARM_FATAL     = 'FATAL'
THERMAL_ALARM_MAJOR     = 'MAJOR'

THERMAL_ALARM = [THERMAL_ALARM_INACCESS, THERMAL_ALARM_INVALID, THERMAL_ALARM_BURST, THERMAL_ALARM_FATAL, THERMAL_ALARM_MAJOR]

# Fan alarm.
FAN_ALARM_ABSENT    = 'ABSENT'
FAN_ALARM_LOW       = 'LOW'
FAN_ALARM_OFFSET    = 'OFFSET'

FAN_ALARM = [FAN_ALARM_ABSENT, FAN_ALARM_LOW, FAN_ALARM_OFFSET]

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

def init_thermal_config(file_name):
    # Exist ?
    if not os.path.isfile(file_name):
        SYS_LOG_CRITICAL('Not find the thermal config file "%s", exiting...' %(file_name))
        return -1, None

    # Open the file.
    try:
        file = open(file_name, 'r')
    except ImportError as e:
        SYS_LOG_CRITICAL('Can not read the thermal config file "%s", exiting...' %(file_name))
        return -1, None

    # Read the file data.
    config_data = {}
    line_num = 0
    for line in file.readlines():
        line = line.strip()
        #SYS_LOG_INFO('config_data line: %s' %(line))

        # Empty?
        if len(line) == 0:
            continue
        # Comment?
        if line[0] == '#':
            continue

        # Config.
        line_val_list = line.split()
        line_val = {}
        # name
        name                        = line_val_list[0]
        line_val[ 'name' ] = name
        # sysfs node ?
        sysfs                       = (line_val_list[1] != '0')
        line_val[ 'sysfs' ] = sysfs
        # set-point
        line_val[ 'setpoint' ]      = int(line_val_list[2])
        # MAJOR
        line_val[ 'MAJOR' ]         = int(line_val_list[3])
        # FATAL
        line_val[ 'FATAL' ]         = int(line_val_list[4])
        # Kp
        line_val[ 'kp' ]            = float(line_val_list[5])
        # Ki
        line_val[ 'ki' ]            = float(line_val_list[6])
        # Kd
        line_val[ 'kd' ]            = float(line_val_list[7])
        # sensor path, or command to get the sensor value.
        if sysfs:
            sensor_path             = line_val_list[8]
            line_val['sensor_path'] = sensor_path
            # if TEST_MODE:
            #     line_val['sensor_path'] = './test' + sensor_path
            SYS_LOG_INFO('thermal sensor "%s" sysfs path = "%s".' %(name, line_val[ 'sensor_path' ]))
        else:
            if 0: #TEST_MODE
                line_val['command'] = 'echo 50.0'
            else:
                cmd = line.partition('"')[2]
                line_val['command'] = cmd.strip('"')
            SYS_LOG_INFO('thermal sensor "%s" command = "%s".' %(name, line_val[ 'command' ]))

        # last PWM
        line_val[ 'last_pwm' ]  = FAN_PWM_DEFAULT
        # Temperature history.
        line_val[ 'temp' ] = []
        line_val[ 'history' ] = []
        # Temperature change-point
        line_val[ 'change_point' ] = None

        config_data[line_num] = line_val
        line_num += 1

    file.close()
    return 0, config_data



def init_fan_config(file_name):
    # Exist ?
    if not os.path.isfile(file_name):
        SYS_LOG_CRITICAL('Not find the fan config file "%s", exiting...' %(file_name))
        return -1, None

    # Open the file.
    try:
        file = open(file_name, 'r')
    except ImportError as e:
        SYS_LOG_CRITICAL('Can not read the fan config file "%s", exiting...' %(file_name))
        return -1, None


    fan_config = {}
    prefix = ''
    # if TEST_MODE:
    #     prefix = './test' + prefix


    # Read the file data.
    config_data = {}
    line_num = 0
    for line in file.readlines():
        line = line.strip()
        #SYS_LOG_INFO('config_data line: %s' %(line))

        # Empty?
        if len(line) == 0:
            continue
        # Comment?
        if line[0] == '#':
            continue

        # Config.
        line_val_list = line.split()
        line_val = {}
        # name
        line_val[ 'name' ]                          =               line_val_list[0]
        # sysfs node
        line_val[ 'sysfs_path' ]                    =  prefix     + line_val_list[1]
        sysfs_path = line_val[ 'sysfs_path' ]
        # present
        line_val[ 'sysfs_path_present' ]            =  sysfs_path + line_val_list[2] if line_val_list[2] != NONE_VALUE else None
        # status
        line_val[ 'sysfs_status' ]                  =  sysfs_path + line_val_list[3]
        # pwm
        line_val[ 'sysfs_path_pwm_0' ]              =  sysfs_path + line_val_list[4]
        line_val[ 'sysfs_path_pwm_1' ]              = (sysfs_path + line_val_list[5]) if line_val_list[5] != NONE_VALUE else None
        # speed
        line_val[ 'sysfs_path_speed_0' ]            =  sysfs_path + line_val_list[6]
        line_val[ 'sysfs_path_speed_1' ]            =  sysfs_path + line_val_list[7]
        # speed target
        line_val[ 'sysfs_path_speed_target_0' ]     =  sysfs_path + line_val_list[8]
        line_val[ 'sysfs_path_speed_target_1' ]     =  sysfs_path + line_val_list[9]
        # speed tolerance
        line_val[ 'sysfs_path_speed_tolerance_0' ]  =  sysfs_path + line_val_list[10]
        line_val[ 'sysfs_path_speed_tolerance_1' ]  =  sysfs_path + line_val_list[11]

        line_val[ 'present' ]                   = True
        line_val[ 'rpm_0' ]                     = 0
        line_val[ 'rpm_1' ]                     = 0

        config_data[line_num] = line_val
        line_num += 1

    config_data['present_count'] = 0
    config_data['abnormal_count'] = 0
    config_data['abnormal_list'] = []

    file.close()
    return 0, config_data



'''
def init_fan_config():
    fan_config = {}
    prefix = FAN_SYS_PATH_PREFIX
    if TEST_MODE:
        prefix = './test' + prefix
    for index in range(FAN_NUM_MAX):
        fan_data = {}
        fan_data[ 'name' ]                  = 'FAN%d' %(index + 1)
        fan_data[ 'pwm' ]                   = 0
        fan_data[ 'rpm_front' ]             = 0
        fan_data[ 'rpm_rear' ]              = 0
        fan_data[ 'present' ]               = True
        fan_data[ 'i2c_path_present' ]      = prefix + '/fan%d_present' %(index + 1)
        fan_data[ 'i2c_path_pwm' ]          = prefix + '/fan%d_pwm' %(index + 1)
        fan_data[ 'i2c_path_rpm_front' ]    = prefix + '/fan%d_input' %(index + 1)
        fan_data[ 'i2c_path_rpm_rear' ]     = prefix + '/fan%d_input' %(index + 1 + FAN_NUM_MAX)
        fan_config[ index ] = fan_data
    fan_config['present_count'] = 0
    fan_config['abnormal_count'] = 0
    fan_config['abnormal_list'] = []
    return 0, fan_config
'''


def init_data():
    global THERMAL_NUM_MAX
    global THERMAL_CONFIG, FAN_CONFIG

    prefix = '/lib/platform-config/current/onl/bin/'
    if TEST_MODE:
        prefix = './'

    # Init thermal config.
    thermal_config_file = prefix + THERMAL_CONFIG_FILE
    ret, THERMAL_CONFIG = init_thermal_config(thermal_config_file)
    if ret != 0:
        return ret
    THERMAL_NUM_MAX = len(THERMAL_CONFIG)
    SYS_LOG_INFO('THERMAL_CONFIG:')
    for index in range(THERMAL_NUM_MAX):
        SYS_LOG_INFO('%s' %(THERMAL_CONFIG[index]))

    fan_config_file = prefix + FAN_CONFIG_FILE
    ret, FAN_CONFIG = init_fan_config(fan_config_file)
    if ret != 0:
        return ret
    SYS_LOG_INFO('FAN_CONFIG:')
    SYS_LOG_INFO('%s' %(FAN_CONFIG))

    return 0


def raising_alarm(sensor, alarm, value = None):
    sensor_alarm = {}
    if ALARM_STATE.get(sensor) == None:
        ALARM_STATE[sensor] = { alarm : { 'raise' : 0, 'clear' : 0, 'value' : value }}
    else:
        if ALARM_STATE[sensor].get(alarm) == None:
            ALARM_STATE[sensor][alarm] = { 'raise' : 0, 'clear': 0, 'value' : value }
    ALARM_STATE[sensor][alarm]['raise'] += 1
    ALARM_STATE[sensor][alarm]['value']  = value
    ALARM_STATE[sensor][alarm]['clear']  = 0

    # clear other alarm.
    alarm_raise_count = ALARM_STATE[sensor][alarm]['raise']
    if alarm_raise_count >= RETRY_COUNT_MAX:
        for every_alarm in ALARM_STATE[sensor].keys():
            if every_alarm != alarm:
                ALARM_STATE[sensor].pop(every_alarm)

    return alarm_raise_count


def clearing_alarm(sensor, alarm, value = None):
    if ALARM_STATE.get(sensor) == None:
        return 0
    if ALARM_STATE[sensor].get(alarm) == None:
        return 0
    if ALARM_STATE[sensor][alarm]['raise'] <= 0:
        return 0

    ALARM_STATE[sensor][alarm]['clear'] += 1
    ALARM_STATE[sensor][alarm]['value']  = None
    if ALARM_STATE[sensor][alarm]['raise'] < RETRY_COUNT_MAX:
        ALARM_STATE[sensor][alarm]['raise'] = 0
    else:
        if ALARM_STATE[sensor][alarm]['clear'] >= RETRY_COUNT_MAX:
            ALARM_STATE[sensor][alarm]['raise'] = 0
    return ALARM_STATE[sensor][alarm]['clear']


def get_alarm_count(sensor, alarm):
    if ALARM_STATE.get(sensor) == None:
        return 0
    if ALARM_STATE[sensor].get(alarm) == None:
        return 0
    return ALARM_STATE[sensor][alarm]['raise']

def get_sensor_alarm(sensor):
    alarm_list = []
    if ALARM_STATE.get(sensor) != None:
        for alarm in ALARM_STATE[sensor].keys():
            if ALARM_STATE[sensor][alarm]['raise'] >= RETRY_COUNT_MAX:
                alarm_list.append(alarm)
    return alarm_list

def check_thermal_sensor_alarm(index, temp, alarm, raising, RETRY_LIMIT = RETRY_COUNT_MAX, PWM_PROPOSAL = FAN_PWM_MAX):
    pwm = None
    old_pwm = FAN_PWM_MIN

    sensor = THERMAL_CONFIG[index]
    sensor_name = sensor['name']
    # Old PWM.
    last_pwm = sensor['last_pwm']

    if raising:
        count = raising_alarm(sensor_name, alarm, temp)
        if count > 0 and count <= RETRY_LIMIT:
            SYS_LOG_INFO('sensor "%s" is going to raise "%s"(%s), retrying count %d' %(sensor_name, alarm, str(temp), count))
            # Need more retry, so not to change PWM.
            pwm = last_pwm
        if count >= RETRY_LIMIT:
            if count == RETRY_LIMIT:
                '''
                # Customer log specification.
                if alarm == THERMAL_ALARM_FATAL:
                    SYS_LOG_CRITICAL('TempRisingCriticalAlarm Report:The temperature of [%s] exceeded the critical limit.(currentValue(%s), thresholdValue(%s))' \
                            %(sensor_name, str(temp), str(sensor[alarm])))
                elif alarm == THERMAL_ALARM_MAJOR:
                    SYS_LOG_WARN('TempRisingWarningAlarm Report:The temperature of [%s] exceeded the upper limit.(currentValue(%s), thresholdValue(%s))'
                            %(sensor_name, str(temp), str(sensor[alarm])))
                '''
                SYS_LOG_WARN('sensor "%s" raise alarm "%s"' %(sensor_name, alarm))
                if PWM_PROPOSAL == FAN_PWM_MAX:
                    SYS_LOG_CRITICAL('propose to set MAX PWM')
            pwm = PWM_PROPOSAL
    else:
        count = clearing_alarm(sensor_name, alarm, temp)
        if count > 0 and count <= RETRY_LIMIT:
            pwm = last_pwm
            SYS_LOG_INFO('sensor "%s" is going to clear "%s"(%s), retrying count %d' %(sensor_name, alarm, str(temp), count))
        if count >= RETRY_LIMIT:
            if count == RETRY_LIMIT:
                '''
                # Customer log specification.
                if alarm == THERMAL_ALARM_FATAL:
                    SYS_LOG_WARN('TempRisingCriticalAlarm Cancel:The temperature of [%s] exceeded the critical limit.(currentValue(%s), thresholdValue(%s))' \
                            %(sensor_name, str(temp), str(sensor[alarm])))
                elif alarm == THERMAL_ALARM_MAJOR:
                    SYS_LOG_WARN('TempRisingWarningAlarm Cancel:The temperature of [%s] exceeded the upper limit.(currentValue(%s), thresholdValue(%s))'
                            %(sensor_name, str(temp), str(sensor[alarm])))
                '''
                SYS_LOG_WARN('sensor "%s" clear alarm "%s"(%s)' %(sensor_name, alarm, str(temp)))
            pwm = None
    return pwm


def check_thermal_sensor_value(index, temp):
    sensor = THERMAL_CONFIG[index]
    sensor_name = sensor['name']

    pwm = None
    pwm_all = []

    # INACCESS ?
    alarm = THERMAL_ALARM_INACCESS
    raise_alarm = (temp == None)
    if raise_alarm and sensor_name == 'SFP':
        # SFP may be inaccessible when system is booting up.
        return FAN_PWM_MIN
    pwm = check_thermal_sensor_alarm(index, temp, alarm, raise_alarm)
    if raise_alarm:
        DBG_LOG('sensor "%s" hit alarm "%s", value is "%s", propose PWM "%s"' %(sensor_name, alarm, str(temp), str(pwm)))
        return pwm
    if pwm != None:
        pwm_all.append(pwm)

    # INVALID ?
    alarm = THERMAL_ALARM_INVALID
    raise_alarm = (temp < THERMAL_VAL_MIN or temp > THERMAL_VAL_MAX)
    pwm = check_thermal_sensor_alarm(index, temp, alarm, raise_alarm)
    if raise_alarm:
        DBG_LOG('sensor "%s" hit alarm "%s", value is "%s", propose PWM "%s"' %(sensor_name, alarm, str(temp), str(pwm)))
        return pwm
    if pwm != None:
        pwm_all.append(pwm)

    # BURST ?
    alarm = THERMAL_ALARM_BURST
    history = sensor['temp']
    #DBG_LOG('sensor "%s" history value %s' %(sensor_name, history))
    if len(history) >= TEMP_HISTORY_MAX:
        old_temp_max = max(history)
        old_temp_min = min(history)
        raise_alarm = (old_temp_max > old_temp_min + THERMAL_BURST_MAX)
        pwm = check_thermal_sensor_alarm(index, temp, alarm, raise_alarm)
        if raise_alarm:
            DBG_LOG('sensor "%s" hit alarm "%s", value is "%s", propose PWM "%s"' %(sensor_name, alarm, str(temp), str(pwm)))
            return pwm
        if pwm != None:
            pwm_all.append(pwm)

    # FATAL
    alarm_fatal = THERMAL_ALARM_FATAL
    raise_alarm = (temp >= sensor[alarm_fatal])
    check_thermal_sensor_alarm(index, temp, alarm_fatal, raise_alarm, PWM_PROPOSAL = None)
    if raise_alarm:
        DBG_LOG('sensor "%s" hit alarm "%s", value is "%s"' %(sensor_name, alarm_fatal, str(temp)))
    else:
        # MAJOR
        alarm_major = THERMAL_ALARM_MAJOR
        raise_alarm = (temp >= sensor[alarm_major] and temp < sensor[alarm_fatal])
        check_thermal_sensor_alarm(index, temp, alarm_major, raise_alarm, PWM_PROPOSAL = None)
        if raise_alarm:
            DBG_LOG('sensor "%s" hit alarm "%s", value is "%s"' %(sensor_name, alarm_major, str(temp)))

    # Return the MAX PWM proposal.
    if len(pwm_all) > 0:
        pwm = max(pwm_all)
    else:
        pwm = None

    return pwm


def check_thermal_sensor_fatal():
    fatal_thermal = []
    alarm = 'FATAL'
    for index in range(THERMAL_NUM_MAX):
        thermal = THERMAL_CONFIG[index]
        thermal_name = thermal['name']
        count = get_alarm_count(thermal_name, alarm)
        if count >= RETRY_COUNT_MAX:
            fatal_thermal.append(thermal_name)
            DBG_LOG('sensor "%s" hit alarm "%s" count %d' %(thermal_name, alarm, count))
    if len(fatal_thermal) >= THERMAL_FATAL_MAX:
        while True:
            SYS_LOG_CRITICAL('resetting board since thermal sensor %s hit "%s" alarm!' %(fatal_thermal, alarm))
            if TEST_MODE:
                break
            time.sleep(3)
            # reset board...
            ret, output = getstatusoutput('sudo reboot')
            SYS_LOG_CRITICAL('%d, "%s"' %(ret, output))
            time.sleep(300)


def get_thermal_sensor_value(index):
    temp = None
    name = THERMAL_CONFIG[index]['name']

    # Get the thermal sensor.
    cmd = 'timeout 3s '
    if THERMAL_CONFIG[index]['sysfs']:
        sys_path = THERMAL_CONFIG[index]['sensor_path']
        cmd = cmd + 'cat %s' %(sys_path)
    else:
        cmd = cmd + THERMAL_CONFIG[index]['command']

    ret, output = getstatusoutput(cmd)
    DBG_LOG('read sensor "%s" output: "%s".' %(name, output))
    temp = None
    if ret != 0 or len(output) == 0:
        DBG_LOG('fail to read sensor "%s".' %(name))
    else:
        try:
            temp = int(float(output))
        except:
            temp = 0

        if name == 'COMe':
            temp /= 1000

        # History, but may be invalid.
        THERMAL_CONFIG[index][ 'temp' ].insert(0, temp)
        if len(THERMAL_CONFIG[index][ 'temp' ]) > TEMP_HISTORY_MAX:
            old_temp = THERMAL_CONFIG[index][ 'temp' ].pop()
        DBG_LOG('sensor "%s" history value = %s' %(name, THERMAL_CONFIG[index][ 'temp' ]))
    return temp


CUSTOMER_FAN_ALARM_LOG = {
    FAN_ALARM_ABSENT :      ['0x10002001', 'Report:The fan module was pluged in/out.(Status=0 0-out 1-in.)',
                                           'Cancel:The fan module was pluged in/out.(Status=1 0-out 1-in.)'],
    FAN_ALARM_LOW    :      ['0x10002002', 'Report:The fan module failed.(ErrCode=1, Reason=The Fan module status abnormal.)',
                                           'Cancel:The fan module failed.(ErrCode=0, Reason=The Fan module status abnormal.)'],
    FAN_ALARM_OFFSET :      ['0x10002002', 'Report:The fan module failed.(ErrCode=2, Reason=The Fan module status abnormal.)',
                                           'Cancel:The fan module failed.(ErrCode=0, Reason=The Fan module status abnormal.)'],
    'FAN_ABNORMAL_LIST' :   ['Report:The fan module failed(Name={}, Reason= Two or more fans are faulty.)',
                                           'Cancel:The fan module failed(Name={}, Reason= Two or more fans are faulty.)' ],
}

def check_fan_sensor_alarm(index, alarm, raising):
    sensor = FAN_CONFIG[index]
    sensor_name = sensor['name']

    if raising:
        count = raising_alarm(sensor_name, alarm)
        if count <= RETRY_COUNT_MAX:
            SYS_LOG_INFO('sensor "%s" is going to raise "%s", retrying count %d' %(sensor_name, alarm, count))
        if count >= RETRY_COUNT_MAX:
            if count == RETRY_COUNT_MAX:
                SYS_LOG_WARN('sensor "%s" raise alarm "%s"' %(sensor_name, alarm))

                '''
                # Customer log specification.
                SYS_LOG_WARN('%s|%s %s' %(CUSTOMER_FAN_ALARM_LOG[alarm][0], sensor_name, CUSTOMER_FAN_ALARM_LOG[alarm][1]))
                '''
    else:
        count = clearing_alarm(sensor_name, alarm)
        if count > 0 and count <= RETRY_COUNT_MAX:
            SYS_LOG_INFO('sensor "%s" is going to clear "%s", retrying count %d' %(sensor_name, alarm, count))
        if count >= RETRY_COUNT_MAX:
            if count == RETRY_COUNT_MAX:
                SYS_LOG_WARN('sensor "%s" clear alarm "%s"' %(sensor_name, alarm))

                '''
                # Customer log specification.
                SYS_LOG_WARN('%s|%s %s' %(CUSTOMER_FAN_ALARM_LOG[alarm][0], sensor_name, CUSTOMER_FAN_ALARM_LOG[alarm][2]))
                '''
    raised = (get_alarm_count(sensor_name, alarm) >= RETRY_COUNT_MAX)
    return raised


def check_fan_state(index, alarm , raising):
    abnormal = False
    if raising:
        if check_fan_sensor_alarm(index, alarm, True):
            # alarm raised.
            abnormal = True
    else:
        abnormal = check_fan_sensor_alarm(index, alarm, False)
    return abnormal


def check_all_fan_state():
    abnormal_fan = []
    for index in range(FAN_NUM_MAX):
        name = FAN_CONFIG[index]['name']
        present = FAN_CONFIG[index]['present']
        rpm_0 = FAN_CONFIG[index]['rpm_0']
        rpm_1 = FAN_CONFIG[index]['rpm_1']
        #rpm_min = min(rpm_0, rpm_rear)
        #rpm_max = max(rpm_1, rpm_rear)
        pwm = get_fan_pwm(index)

        rpm_0_target = get_fan_config_sysfs_value(index, 'sysfs_path_speed_target_0')
        rpm_1_target = get_fan_config_sysfs_value(index, 'sysfs_path_speed_target_1')
        rpm_0_tolenrance = get_fan_config_sysfs_value(index, 'sysfs_path_speed_tolerance_0')
        rpm_1_tolenrance = get_fan_config_sysfs_value(index, 'sysfs_path_speed_tolerance_1')

        raise_alarm = False
        abnormal = False
        # ABSENT
        if not raise_alarm:
            alarm = FAN_ALARM_ABSENT
            raise_alarm = not present
            if check_fan_state(index, alarm, raise_alarm):
                abnormal = True

        '''
        # LOW
        if not raise_alarm:
            alarm = FAN_ALARM_LOW
            raise_alarm = (present and rpm_min < FAN_RPM_LOW)
            if check_fan_state(index, alarm, raise_alarm):
                abnormal = True
        '''

        # OFFSET
        if not raise_alarm:
            alarm = FAN_ALARM_OFFSET
            raise_alarm_0 = ((rpm_0 + rpm_0_tolenrance) < rpm_0_target) or (rpm_0 > (rpm_0_tolenrance+ rpm_0_target))
            raise_alarm_1 = ((rpm_1 + rpm_1_tolenrance) < rpm_1_target) or (rpm_1 > (rpm_1_tolenrance+ rpm_1_target))
            raise_alarm = (present and (raise_alarm_0 or raise_alarm_1))
            if check_fan_state(index, alarm, raise_alarm):
                abnormal = True

        if abnormal:
            abnormal_fan.append(name)

    return abnormal_fan


def get_fan_config_sysfs_value(index, config, format = 10):
    val = None
    name = FAN_CONFIG[index]['name']
    sys_path = FAN_CONFIG[index][config]

    cmd = 'cat %s' %(sys_path)

    # read the i2c sys file twice due to CPLD issues.
    ret, output = getstatusoutput(cmd)
    ret, output = getstatusoutput(cmd)

    #DBG_LOG('read "%s" config "%s" cmd: "%s", output = "%s".' %(name, config, cmd, output))
    if (ret != 0) or (len(output) == 0) or ('NA' in output):
        val = 0
        DBG_LOG('fail to read sensor "%s" conifg "%s".' %(name, config))
    else:
        val = int(output, format)
        #DBG_LOG('sensor "%s" config "%s" value = "%d".' %(name, config, val))
    return val


def get_fan_alarm_count(alarm):
    total_count = 0
    for index in range(FAN_NUM_MAX):
        name = FAN_CONFIG[index]['name']
        if get_alarm_count(name, alarm) >= RETRY_COUNT_MAX:
            total_count += 1
    return total_count


def get_fan_pwm(index):
    return get_fan_config_sysfs_value(index, 'sysfs_path_pwm_0')


def set_fan_pwm(index, pwm):
    name = FAN_CONFIG[index]['name']
    sys_path = FAN_CONFIG[index]['sysfs_path_pwm_0']

    cmd = 'echo %d > %s' %(pwm, sys_path)
    #SYS_LOG_INFO('setting %s PWM to %d by command "%s".' %(name, pwm, cmd))
    ret, output = getstatusoutput(cmd)
    if ret != 0:
        SYS_LOG_WARN('fail to set %s PWM to %d, %s.' %(name, pwm, output))
    return ret


def check_fan_pwm(pwm_expected, PWM_OFFSET = 2):
    match = True
    for index in range(FAN_NUM_MAX):
        pwm = get_fan_pwm(index)
        if pwm > (pwm_expected + PWM_OFFSET) or (pwm + PWM_OFFSET) < pwm_expected:
            DBG_LOG("FAN%d PWM(%d) is beyond reasonable offset from the expected PWM(%d)" %(index + 1, pwm, pwm_expected))
            match = False
    return match


def get_fan_present(index):
    present = True
    fan = FAN_CONFIG[index]

    if fan['sysfs_path_present'] != None:
        fan_status = get_fan_config_sysfs_value(index, 'sysfs_path_present')
        DBG_LOG("FAN{} present {}".format(index + 1, fan_status))
    else:
        # Check the status
        fan_status = get_fan_config_sysfs_value(index, 'sysfs_status')
        DBG_LOG("FAN{} status {}".format(index + 1, fan_status))

    if fan_status == FAN_NOT_PRESENT:
        present = False

    if present == False:
        # Not present.
        fan['present'] = False
        DBG_LOG('FAN%d is absent' %(index + 1))

    return present


def check_fan_sensor():
    global FAN_CONFIG
    pwm_proposal = FAN_PWM_MIN
    present_count = 0

    for index in range(FAN_NUM_MAX):
        fan = FAN_CONFIG[index]
        # present ?
        present = get_fan_present(index)
        if present == False:
            # Not present.
            fan['present'] = False
            DBG_LOG('FAN%d is absent' %(index + 1))
            continue
        # present.
        fan['present'] = True
        present_count += 1

        rpm_front = get_fan_config_sysfs_value(index, 'sysfs_path_speed_0')
        fan['rpm_0'] = rpm_front
        rpm_rear = get_fan_config_sysfs_value(index, 'sysfs_path_speed_1')
        fan['rpm_1']  = rpm_rear
        DBG_LOG('%s rpm = [%d %d]' %(fan['name'], rpm_front, rpm_rear))
        max_rpm = max(rpm_front, rpm_rear)
        min_rpm = min(rpm_front, rpm_rear)

    FAN_CONFIG['present_count'] = present_count
    DBG_LOG('FAN present count = [%d]' %(present_count))

    abnormal_fan_list = check_all_fan_state()
    abnormal_count = len(abnormal_fan_list)
    DBG_LOG('FAN abnormal count = [%d]' %(abnormal_count))

    '''
    if abnormal_count >= FAN_ABNORMAL_MAX:
        if FAN_CONFIG['abnormal_list'] !=  abnormal_fan_list:
            SYS_LOG_WARN('found %d FAN modules abnormal, propose to set MAX PWM' %(abnormal_count))

            # Customer log specification.
            customer_log = 'TwoOrMoreFanFaulty ' + CUSTOMER_FAN_ALARM_LOG['FAN_ABNORMAL_LIST'][0].format(abnormal_fan_list)
            SYS_LOG_WARN(customer_log)

        pwm_proposal = FAN_PWM_MAX
    else:
        if FAN_CONFIG['abnormal_count'] >= FAN_ABNORMAL_MAX:
            SYS_LOG_WARN('found %d FAN modules abnormal, propose to set normal PWM' %(abnormal_count))

            # Customer log specification.
            customer_log = 'TwoOrMoreFanFaulty ' + CUSTOMER_FAN_ALARM_LOG['FAN_ABNORMAL_LIST'][1].format(abnormal_fan_list)
            SYS_LOG_WARN(customer_log)
    '''

    FAN_CONFIG['abnormal_list']  = abnormal_fan_list
    FAN_CONFIG['abnormal_count'] = abnormal_count

    return pwm_proposal


def calc_pid_pwm(thermal_index, temp):
    global THERMAL_CONFIG
    sensor = THERMAL_CONFIG[thermal_index]
    name = sensor['name']

    Kp          = sensor['kp']
    Ki          = sensor['ki']
    Kd          = sensor['kd']
    setpoint    = sensor['setpoint']
    last_pwm    = sensor['last_pwm']
    change_point_temp = sensor['change_point']

    # Record real valid temperature history.
    THERMAL_CONFIG[thermal_index][ 'history' ].insert(0, temp)
    if len(THERMAL_CONFIG[thermal_index][ 'history' ]) > TEMP_HISTORY_MAX:
        old_temp = THERMAL_CONFIG[thermal_index][ 'history' ].pop()
    DBG_LOG('sensor "%s" valid history value = %s' %(name, THERMAL_CONFIG[thermal_index][ 'history' ]))

    last_temp1  = temp
    if len(sensor['history']) > 1:
        last_temp1  = sensor['history'][1]
    last_temp2  = temp
    if len(sensor['history']) > 2:
        last_temp2  = sensor['history'][2]

    P = Kp * (temp - last_temp1)
    I = Ki * (temp - setpoint)
    D = Kd * (temp + last_temp2 - 2 * last_temp1)
    pwm = int(last_pwm + P + I + D)

    if temp < setpoint:
        # Force the thermal sensor not to affect PWM as long as its temperature is below "setpoint".
        pwm = FAN_PWM_MIN

    # Hysteresis operation.
    hyst_action = 'hyst'
    if change_point_temp == None:
        # init state.
        change_point_temp = temp + THERMAL_HYSTERESIS + 1
    if (pwm <= last_pwm) and ((change_point_temp - THERMAL_HYSTERESIS) <= temp <= change_point_temp):
        # Hyst state
        DBG_LOG('%s: pwm = [%d] last_pwm = [%d] change_point = [%d] P = [%d] I = [%d] D = [%d]' \
                %(hyst_action, pwm, last_pwm, change_point_temp, P, I, D))
        # Not change PWM under Hyst state.
        pwm = last_pwm
    else:
        # Non-Hyst state
        hyst_action = 'calc'
        THERMAL_CONFIG[thermal_index]['change_point'] = temp
        DBG_LOG('%s: pwm = [%d] last_pwm = [%d] change_point = [%d] P = [%d] I = [%d] D = [%d]' \
                %(hyst_action, pwm, last_pwm, change_point_temp, P, I, D))

    pwm = min(pwm, FAN_PWM_MAX)
    pwm = max(pwm, FAN_PWM_MIN)
    THERMAL_CONFIG[thermal_index]['last_pwm'] = pwm
    return pwm


def set_all_fan_pwm(pwm, sensor):
    if check_fan_pwm(pwm):
        # No need to set the same PWM again.
        return
    SYS_LOG_INFO('setting ALL FAN PWM to %d, thermal sensor %s.' %(pwm, sensor))
    for index in range(FAN_NUM_MAX):
        set_fan_pwm(index, pwm)

'''
def set_fmea_fan_status(index, alarm , raising):
    FMEA_FAN_OTT_PATH = '/sys/ott_fan'
    FMEA_FAN_ALARM_PROCESS = {
        # ALARM : ("Value to write when clearing", "Value to write when rasing", "File to write")
        FAN_ALARM_ABSENT    : (1, 0, '/sys/cpld/fan{}_pret_ctrl'),
        FAN_ALARM_LOW       : (0, 1, '%s/{}/status' %(FMEA_FAN_OTT_PATH)),
        FAN_ALARM_OFFSET    : (0, 2, '%s/{}/status' %(FMEA_FAN_OTT_PATH)),
    }

    if FMEA_FAN_ALARM_PROCESS.get(alarm) == None:
        return

    action = 'clear'
    val = FMEA_FAN_ALARM_PROCESS[alarm][0]
    if raising:
        action = 'raise'
        val = FMEA_FAN_ALARM_PROCESS[alarm][1]
    sys_path = FMEA_FAN_ALARM_PROCESS[alarm][2].format(index + 1)

    # For test purpose only.
    if TEST_MODE:
        prefix = './test'
        sys_path = prefix + sys_path
        dir_name  = os.path.dirname(sys_path)
        file_name = os.path.basename(sys_path)
        if alarm == FAN_ALARM_ABSENT:
            status_path = '%s/%d' %(prefix + FMEA_FAN_OTT_PATH, index + 1)

            if raising:
                cmd = 'rm -rf %s' %(status_path)
                DBG_LOG('FMEA: %s alarm "%s" for FAN%d, cmd "%s".' %(action, alarm, index+1, cmd))
                ret, output = getstatusoutput(cmd)
            else:
                # ctrl path
                cmd = 'mkdir -p %s' %(dir_name)
                ret, output = getstatusoutput(cmd)
                cmd = 'touch %s' %(sys_path)
                ret, output = getstatusoutput(cmd)
                # status path
                cmd = 'mkdir -p %s' %(status_path)
                ret, output = getstatusoutput(cmd)
                status_file = status_path + '/status'
                cmd = 'touch %s' %(status_file)
                ret, output = getstatusoutput(cmd)

    if not os.path.exists(sys_path):
        SYS_LOG_WARN('FMEA: Fail to %s alarm "%s" for FAN%d, sys path "%s" not exist.' %(action, alarm, index+1, sys_path))
    else:
        cmd = 'echo %d > %s' %(val, sys_path)
        ret, output = getstatusoutput(cmd)
        DBG_LOG('FMEA: %s alarm "%s" for FAN%d, cmd "%s".' %(action, alarm, index+1, cmd))
        if ret != 0:
            SYS_LOG_WARN('FMEA: Fail to %s alarm "%s" for FAN%d, command "%s", %s' %(action, alarm, index+1, cmd, output))
'''

'''
def set_all_fmea_fan_status():
    for index in range(FAN_NUM_MAX):
        fan = 'FAN%d' %(index + 1)
        if get_alarm_count(fan, FAN_ALARM_ABSENT) > 0:
            set_fmea_fan_status(index, FAN_ALARM_ABSENT, True)
            # Nothing more need to do for this FAN.
            continue
        alarm_list = get_sensor_alarm(fan)
        for alarm in FAN_ALARM:
            set_fmea_fan_status(index, alarm , (alarm in alarm_list))
'''

'''
def set_led_color(color):
    cmd = 'echo %d > %s' %(color, LED_CTRL_PATH)
    ret, output = getstatusoutput(cmd)

def set_led_off():
    DBG_LOG('setting FAN LED to OFF.')
    set_led_color(LED_OFF)

def set_led_green():
    DBG_LOG('setting FAN LED to GREEN.')
    set_led_color(LED_GREEN)

def set_led_red():
    DBG_LOG('setting FAN LED to RED!')
    set_led_color(LED_RED)

def control_led_thread(thread_name, delay_time):
    if delay_time != None:
        time.sleep(delay_time)

    loop_count = 0
    while True:
        alarm_total_count = 0
        for alarm in FAN_ALARM:
            alarm_count = get_fan_alarm_count(alarm)
            DBG_LOG('control_led_thread: checking alarm [%s], count %d.' %(alarm, alarm_count))
            alarm_total_count += alarm_count
        DBG_LOG('control_led_thread: alarm total count %d.' %(alarm_total_count))
        if alarm_total_count >= FAN_ABNORMAL_MAX:
            set_led_red()
        else:
            set_led_green()
        loop_count += 1
        time.sleep(2)
'''

def show_raised_alarm():
    if not SHOW_MODE:
        return

    SYS_LOG_INFO( "----------------------------------------------------------------------" )
    SYS_LOG_INFO("")
    SYS_LOG_INFO( "%-16s %-16s %-24s %-12s" %('Raised alarm:', 'MODULE', 'ALARM', 'VALUE') )

    print_sensor = None
    for sensor in ALARM_STATE.keys():
        for alarm in ALARM_STATE[sensor].keys():
            raised_count = ALARM_STATE[sensor][alarm]['raise']
            if raised_count < RETRY_COUNT_MAX:
                continue
            if( print_sensor != sensor ):
                SYS_LOG_INFO("%-16s %-16s %-24s [%s]" %('', sensor, alarm, raised_count))
            else:
                SYS_LOG_INFO("%-16s %-16s %-24s [%s]" %('', '', alarm, raised_count))
            print_sensor = sensor
    SYS_LOG_INFO("")
    SYS_LOG_INFO( "----------------------------------------------------------------------" )


def thermal_pid_fan_policy():
    pwm_sensor = []
    max_thermal_pwm = FAN_PWM_MIN
    for index in range(THERMAL_NUM_MAX):
        # Read the thermal sensor value.
        temp = get_thermal_sensor_value(index)

        # Check thermal sensor value.
        thermal_pwm = check_thermal_sensor_value(index, temp)
        # Check FATAL thermal sensor.

        pwm = FAN_PWM_MIN
        if thermal_pwm != None:
            # No need to calculate the PID value.
            pwm = thermal_pwm
        else:
            # PID
            pwm = calc_pid_pwm(index, temp)

        # Record the sensor with the max PWM.
        if pwm > max_thermal_pwm:
            max_thermal_pwm = pwm
            pwm_sensor = [THERMAL_CONFIG[index]['name'], str(temp)]
        elif pwm == max_thermal_pwm:
            pwm_sensor.append([THERMAL_CONFIG[index]['name'], str(temp)])

    # Reset the board once two(or more) thermal sensors hit alarm 'FATAL'.
    check_thermal_sensor_fatal()

    # Check the FAN sensor
    fan_sensor_pwm = check_fan_sensor()

    # Set FMEA status.
    #set_all_fmea_fan_status()

    # Choose the highest value.
    pwm_proposal = max(max_thermal_pwm, fan_sensor_pwm)
    DBG_LOG('propose NEW PWM = [%d]' %(pwm_proposal))

    pwm_proposal = min(FAN_PWM_MAX, pwm_proposal)
    pwm_proposal = max(FAN_PWM_MIN, pwm_proposal)

    # Set PWM.
    set_all_fan_pwm(pwm_proposal, pwm_sensor)

    # Show all alarms by now.
    show_raised_alarm()

def check_bmc_status():
    global BMC_STATUS

    status, output = getstatusoutput("cat /sys/switch/bmc/status")
    # check if BMC OK
    if(status == 0) and (output == "1"):
        BMC_STATUS_1 = True
        # print("BMC present and work.")
    else:
        BMC_STATUS_1 = False
        # print("BMC not exist.")

    status, output = getstatusoutput("cat /sys/switch/bmc/enable")
    # check if BMC enable
    if(status == 0) and (output == "1"):
        BMC_STATUS_2 = True
        print("BMC enable.")
    else:
        BMC_STATUS_2 = False
        # print("BMC disable.")
    if(BMC_STATUS_1 == True) and (BMC_STATUS_2 == True):
        BMC_STATUS = True
    else:
        BMC_STATUS = False

    return BMC_STATUS

# time_out 0-42 sec
def set_fan_timeout(time_out):
    if(time_out > 42):
        time_out = 42
    timeout_count = int(float(time_out)/0.67) & 0x3f
    clear_cmd = "busybox devmem 0xfc80102f 8 0x{:x}".format(timeout_count)
    status, output = getstatusoutput(clear_cmd)
    if status:
        print(output)

def clear_fan_conut():
    clear_cmd = "busybox devmem 0xfc80102e 8 0x03"
    status, output = getstatusoutput(clear_cmd)
    if status:
        print(output)

def send_sfp_temp_to_bmc(temp):
    cmd = 'ipmitool raw 0x36 0x9F %d %d' % (temp if (temp is not None) else 0, 1 if (temp is not None) else 0)
    ret, output = getstatusoutput(cmd)
    DBG_LOG('%s, ret:%d, "%s"' %(cmd, ret, output))
    if ret != 0:
        DBG_LOG('fail to send SFP temperature to BMC.')

def power_off():
    status, output = getstatusoutput("power.sh off")
    if status:
        print(output)

def power_down_strategy():
    present_count = 0
    retry_time = 3

    while retry_time:
        present_count = 0
        for index in range(FAN_NUM_MAX):
            present = get_fan_present(index)
            if(present == False):
                present_count += 1
        if present_count >= FAN_NUM_MAX:
            SYS_LOG_INFO("The fans are all pull out, check again, the check count{}".format(3-retry_time))
            retry_time -= 1
        else:
            break
    if retry_time == 0:
        SYS_LOG_INFO("The fans are all pull out, system will power cycle")
        power_off()

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

    '''
    # FAN LED control thread.
    SYS_LOG_INFO('Starting LED control thread...')
    thread.start_new_thread(control_led_thread, ('fan-led-control', SLEEP_TIME))
    '''
    print_onec = 0
    # Main loop.
    set_fan_timeout(15)
    while True:
        # Check debug flag.
        DEBUG_MODE = check_debug_flag()
        SHOW_MODE = check_show_flag()

        check_bmc_status()
        while BMC_STATUS:
            # If BMC is present, periodically update the max SFP temp to BMC only.
            # Get SFP temperature
            for index in range(THERMAL_NUM_MAX):
                if THERMAL_CONFIG[index]['name'] == 'SFP':
                    sfp_temp = get_thermal_sensor_value(index)
                    send_sfp_temp_to_bmc(None if (sfp_temp is None) else (sfp_temp%256))
                    break
            # Sleep
            if(print_onec == 0):
                SYS_LOG_WARN("fan monitor by BMC!")
                print_onec = 1
            time.sleep(SLEEP_TIME)

        thermal_pid_fan_policy()
        clear_fan_conut()
        power_down_strategy()
        # Sleep
        DBG_LOG("fan monitor alive")
        time.sleep(SLEEP_TIME)


if __name__ == '__main__':
    main(sys.argv)


