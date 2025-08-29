#!/usr/bin/env python

import syslog
import os
import sys
import collections


def log_os_system(cmd):
    if sys.version_info.major == 2:
        # python2
        import commands
        status, output = commands.getstatusoutput(cmd)
    else:
        # python3
        import subprocess
        status, output = subprocess.getstatusoutput(cmd)

    return  status, output


def recode_temp_info():
    ret, output = log_os_system("cat /sys/switch/sensor/num_temp_sensors")
    if(ret) or ("NA" in output):
        syslog.syslog(syslog.LOG_WARNING, "can not get num_temp_sensors ret:{} output:{}".format(ret, output))
        return ret
    num_temp = int(output)

    for temp_index in range(0,num_temp):
        temp_alias_cmd = "cat /sys/switch/sensor/temp{}/temp_alias".format(temp_index)
        temp_input_cmd = "cat /sys/switch/sensor/temp{}/temp_input".format(temp_index)
        ret, output = log_os_system(temp_alias_cmd)
        if(ret) or ("NA" in output):
            syslog.syslog(syslog.LOG_WARNING, "can not get temp_alias ret:{} output:{}".format(ret, output))
            return ret
        temp_alias = output

        ret, output = log_os_system(temp_input_cmd)
        if(ret) or ("NA" in output):
            syslog.syslog(syslog.LOG_WARNING, "can not get temp_input ret:{} output:{}".format(ret, output))
            return ret
        temp_input = output

        syslog.syslog(syslog.LOG_NOTICE, "Temperature alias:{} val:{}C".format(temp_alias, temp_input))


def recode_barcode_info():
    # syseeprom info
    ret, output = log_os_system("decode-syseeprom")
    if(ret) or ("NA" in output):
        syslog.syslog(syslog.LOG_WARNING, "can not get decode-syseeprom ret:{} output:{}".format(ret, output))
        return ret
    for output_info in output.splitlines():
        syslog.syslog(syslog.LOG_NOTICE, "{}".format(output_info))

    # fan eeprom
    fan_info_dict = {
        "serial_number":"cat /sys/switch/fan/fan{}/serial_number",
        "model_name":"cat /sys/switch/fan/fan{}/model_name",
        "part_number":"cat /sys/switch/fan/fan{}/part_number",
        "vendor":"cat /sys/switch/fan/fan{}/vendor",
        "hardware_version":"cat /sys/switch/fan/fan{}/hardware_version"
    }

    ret, output = log_os_system("cat /sys/switch/fan/num")
    if(ret) or ("NA" in output):
        syslog.syslog(syslog.LOG_WARNING, "can not get fan num ret:{} output:{}".format(ret, output))
        return ret
    num_fan = int(output)

    for fan_index in range(1, num_fan+1):
        syslog.syslog(syslog.LOG_NOTICE, "Fan{} info:".format(fan_index))
        for  fan_info_name, fan_info_cmd in fan_info_dict.items():
            ret, output = log_os_system(fan_info_cmd.format(fan_index))
            if(ret) or ("NA" in output):
                syslog.syslog(syslog.LOG_WARNING, "can not get decode-syseeprom ret:{} output:{}".format(ret, output))
                return ret
            info_str = output
            syslog.syslog(syslog.LOG_NOTICE, "    {}:{}".format(fan_info_name, info_str))

def recode_fan_run_info():
    # fan run info
    ret, output = log_os_system("show platform fan")
    if ret:
        syslog.syslog(syslog.LOG_WARNING, "can not get show platform fan ret:{} output:{}".format(ret, output))
        return ret
    for output_info in output.splitlines():
        syslog.syslog(syslog.LOG_NOTICE, "{}".format(output_info))

def main():
    syslog.syslog(syslog.LOG_NOTICE, "Record hardware information start")
    recode_temp_info()
    recode_barcode_info()
    recode_fan_run_info()


if __name__ == "__main__":
    main()
    syslog.syslog(syslog.LOG_NOTICE, "Record hardware information end")
