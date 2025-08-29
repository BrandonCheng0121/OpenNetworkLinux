#!/usr/bin/python3
import json
import os
import subprocess

class FAN_TEST():
    SYSCTL_STOP_FAN_SERVICE = "systemctl stop muxi-platform-fan-monitor.service"
    SYSCTL_START_FAN_SERVICE = "systemctl start muxi-platform-fan-monitor.service"

    FAN_COUNT = "/sys/switch/fan/num"
    MOTOR_COUNT_NODE = "/sys/switch/fan/fan{}/num_motors"
    MOTOR_SPEED = "/sys/switch/fan/fan{}/motor{}/speed"
    MOTOR_RATIO = "/sys/switch/fan/fan{}/motor{}/ratio"
    MOTOR_SPEED_TOLERANCE = "/sys/switch/fan/fan{}/motor{}/speed_tolerance"
    MOTOR_SPEED_TARGET = "/sys/switch/fan/fan{}/motor{}/speed_target"

    def __init__(self):
        with open('/usr/local/bin/self_detect/kis_platform_table.json', 'r') as f:
            kis_json_str = json.load(f)
        enmu_def_keys = kis_json_str.keys()
        enmu_def_dict = kis_json_str
        self.file_fan_count = int(enmu_def_dict['fan_count']['val'])
        self.file_fan_start_count = int(enmu_def_dict['fan_count']['start_num'])
        self.file_motor_count = int(enmu_def_dict['motor_count']['val'])
        self.file_motor_start_count = int(enmu_def_dict['motor_count']['start_num'])

    def call_retcode(self, cmd):
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, shell=True)
        outs = p.communicate()
        return outs[0].decode()+outs[1].decode()

    def previous_setting(self):
        self.call_retcode(self.SYSCTL_STOP_FAN_SERVICE)
    # restore fan auto mode
    def restore_default_setting(self):
        self.call_retcode(self.SYSCTL_START_FAN_SERVICE)

    def read_sysfs(self,file):
        ret_str = self.call_retcode("cat {}".format(file))
        if 'No such file' in ret_str:
            return False
        else:
            return ret_str
        return False

    def get_fan_count(self):
        try:
            return int(self.read_sysfs(self.FAN_COUNT),10)
        except :
            print("get fan count failed")
            return False

    def get_motor_count(self,fanid):
        try:
            return int(self.read_sysfs(self.MOTOR_COUNT_NODE.format(fanid)),10)
        except Exception as e:
            print("get motor count failed")
            return False

    def test_fan_info(self):
        self.fan_count = self.get_fan_count()
        if(self.fan_count != False):
            return True

    def test_fan_status(self):
        if self.fan_count != self.file_fan_count:
            print("fan count fail.please check json file !")
            return False

        for fan_id in range(self.file_fan_start_count, self.fan_count+self.file_fan_start_count):
            motor_count = self.get_motor_count(fan_id)
            if motor_count != self.file_motor_count:
                print("motor count fail.please check json file !")
                return False
            for motor_id in range(self.file_motor_start_count, motor_count+self.file_motor_start_count):
                ratio = int(self.read_sysfs(self.MOTOR_RATIO.format(fan_id, motor_id)))
                speed = float(self.read_sysfs(self.MOTOR_SPEED.format(fan_id, motor_id)))
                speed_tolerance = float(self.read_sysfs(self.MOTOR_SPEED_TOLERANCE.format(fan_id, motor_id)))
                speed_target = float(self.read_sysfs(self.MOTOR_SPEED_TARGET.format(fan_id, motor_id)))
                speed_max = speed_target + speed_tolerance
                speed_min = speed_target - speed_tolerance
                # running_state = self.read_sysfs(f'/sys/s3ip/fan/fan{n}/motor{i}/speed')
                print("fan{}.motor{}  speed:{}  speed_max:{}  speed_min:{}  ratio:{}%".format(fan_id, motor_id, speed, speed_max, speed_min, ratio))
                if speed > speed_max or speed < speed_min:
                    print("test_fan_status fan{}.motor{} fail.".format(fan_id, motor_id))
                    return False
        print("test_fan_status PASS.")
        return True

    def run_test(self):
        ret = self.test_fan_info()
        if ret != True :
            return ret
        ret = self.test_fan_status()
        if ret != True :
            return ret
        # ret = self.test_fan_control()
        # if ret != True :
        #     return ret
        return True

