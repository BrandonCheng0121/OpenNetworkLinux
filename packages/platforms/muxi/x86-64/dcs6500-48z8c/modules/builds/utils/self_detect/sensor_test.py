#!/usr/bin/python3
import json
import os
import subprocess

class SENSOR_TEST():
    SENSOR_VOL_COUNT = "/sys/switch/sensor/num_in_sensors"
    SENSOR_VOL = "/sys/switch/sensor/in{}/in_input"
    SENSOR_MAX_VOL = "/sys/switch/sensor/in{}/in_max"
    SENSOR_MIN_VOL = "/sys/switch/sensor/in{}/in_min"
    SENSOR_VOL_NAME = "/sys/switch/sensor/in{}/in_alias"

    SENSOR_CURR_COUNT = "/sys/switch/sensor/num_curr_sensors"
    SENSOR_CURR = "/sys/switch/sensor/curr{}/curr_input"
    SENSOR_MAX_CURR = "/sys/switch/sensor/curr{}/curr_max"
    SENSOR_CURR_NAME = "/sys/switch/sensor/curr{}/curr_alias"

    SENSOR_TEMP_COUNT = "/sys/switch/sensor/num_temp_sensors"
    SENSOR_TEMP = "/sys/switch/sensor/temp{}/temp_input"
    SENSOR_MAX_TEMP = "/sys/switch/sensor/temp{}/temp_max"
    SENSOR_MIN_TEMP = "/sys/switch/sensor/temp{}/temp_min"
    SENSOR_TEMP_NAME = "/sys/switch/sensor/temp{}/temp_alias"

    def __init__(self):
        with open('/usr/local/bin/self_detect/kis_platform_table.json', 'r') as f:
            kis_json_str = json.load(f)
        enmu_def_dict = kis_json_str
        self.file_sensor_vol_count = int(enmu_def_dict['sensor_in_count']['val'])
        self.file_sensor_vol_start_count = int(enmu_def_dict['sensor_in_count']['start_num'])
        self.file_sensor_curr_count = int(enmu_def_dict['sensor_curr_count']['val'])
        self.file_sensor_curr_start_count = int(enmu_def_dict['sensor_curr_count']['start_num'])
        self.file_sensor_temp_count = int(enmu_def_dict['sensor_temp_count']['val'])
        self.file_sensor_temp_start_count = int(enmu_def_dict['sensor_temp_count']['start_num'])

    def call_retcode(self, cmd):
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, shell=True)
        outs = p.communicate()
        return outs[0].decode() + outs[1].decode()

    def read_sysfs(self, file):
        ret_str = self.call_retcode("cat {}".format(file))
        if 'No such' in ret_str or 'Operation not permitted' in ret_str:
            return False
        else:
            return ret_str
        return False

    def get_vol_sensor_count(self):
        try:
            return int(self.read_sysfs(self.SENSOR_VOL_COUNT),10)
        except :
            print("get vol sensor count failed")
            return False

    def get_curr_sensor_count(self):
        try:
            return int(self.read_sysfs(self.SENSOR_CURR_COUNT),10)
        except :
            print("get curr sensor count failed")
            return False

    def get_temp_sensor_count(self):
        try:
            return int(self.read_sysfs(self.SENSOR_TEMP_COUNT),10)
        except :
            print("get temp sensor count failed")
            return False

    def test_sensor_info(self):
        self.sensor_vol_count = self.get_vol_sensor_count()
        if(self.sensor_vol_count == False):
            return False
        self.sensor_curr_count = self.get_curr_sensor_count()
        if(self.sensor_curr_count == False):
            return False
        self.sensor_temp_count = self.get_temp_sensor_count()
        if(self.sensor_temp_count == False):
            return False
        return True

    def test_sensor_status(self):
        ret = True
        if self.sensor_vol_count != self.file_sensor_vol_count:
            print("voltage sensor count fail.please check json file !")
            return False
        if self.sensor_curr_count != self.file_sensor_curr_count:
            print("current sensor count fail.please check json file !")
            return False
        if self.sensor_temp_count != self.file_sensor_temp_count:
            print("temperature sensor count fail.please check json file !")
            return False
        for vol_id in range(self.file_sensor_vol_start_count, self.sensor_vol_count+self.file_sensor_vol_start_count):
            vol = float(self.read_sysfs(self.SENSOR_VOL.format(vol_id)))
            max_vol = float(self.read_sysfs(self.SENSOR_MAX_VOL.format(vol_id)))
            min_vol = float(self.read_sysfs(self.SENSOR_MIN_VOL.format(vol_id)))
            vol_name = self.read_sysfs(self.SENSOR_VOL_NAME.format(vol_id)).strip('\n')
            print("sensor {}: in{}:{}  max:{}  min:{}  ".format(vol_name, vol_id, vol, max_vol, min_vol))
            if vol > max_vol or vol < min_vol:
                print("test_sensor_status {} in{} voltage fail.".format(vol_name, vol_id))
                ret = False
        for curr_id in range(self.file_sensor_curr_start_count, self.sensor_curr_count+self.file_sensor_curr_start_count):
            curr = float(self.read_sysfs(self.SENSOR_CURR.format(curr_id)))
            max_curr = float(self.read_sysfs(self.SENSOR_MAX_CURR.format(curr_id)))
            curr_name = self.read_sysfs(self.SENSOR_CURR_NAME.format(curr_id)).strip('\n')
            print("sensor {}: curr{}:{}  max:{}  ".format(curr_name, curr_id, curr, max_curr))
            if curr > max_curr :
                print("test_sensor_status {} curr{} currtage fail.".format(curr_name, curr_id))
                ret = False
        for temp_id in range(self.file_sensor_temp_start_count, self.sensor_temp_count+self.file_sensor_temp_start_count):
            temp = float(self.read_sysfs(self.SENSOR_TEMP.format(temp_id)))
            max_temp = float(self.read_sysfs(self.SENSOR_MAX_TEMP.format(temp_id)))
            min_temp = float(self.read_sysfs(self.SENSOR_MIN_TEMP.format(temp_id)))
            temp_name = self.read_sysfs(self.SENSOR_TEMP_NAME.format(temp_id)).strip('\n')
            print("sensor {}: temp{}:{}  max:{}  min:{}  ".format(temp_name, temp_id, temp, max_temp, min_temp))
            if temp > max_temp or temp < min_temp:
                print("test_sensor_status {} temp{} temperature fail.".format(temp_name, temp_id))
                ret = False
            # running_state = self.read_sysfs(f'/sys/s3ip/sensor/sensor{n}/motor{i}/speed')
        
        if ret == True:
            print("test_sensor_status PASS.")
        else:
            print("test_sensor_status fail.")
        return ret

    def run_test(self):
        ret = self.test_sensor_info()
        if ret != True :
            return ret
        ret = self.test_sensor_status()
        if ret != True :
            return ret
        # ret = self.test_sensor_control()
        # if ret != True :
        #     return ret
        return True
