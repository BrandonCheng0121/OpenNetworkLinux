#!/usr/bin/python3
import json
import os
import subprocess

class PSU_TEST():
    PSU_COUNT_NODE = "/sys/switch/psu/num"
    PSU_IN_VOL = "/sys/switch/psu/psu{}/in_vol"
    PSU_IN_CURR = "/sys/switch/psu/psu{}/in_curr"
    PSU_IN_POWER = "/sys/switch/psu/psu{}/in_power"
    PSU_MAX_IN_VOL = "/sys/switch/psu/psu{}/alarm_threshold_vol"
    PSU_MAX_IN_CURR = "/sys/switch/psu/psu{}/alarm_threshold_curr"
    PSU_MAX_IN_POWER =  "/sys/switch/psu/psu{}/max_output_power"
    PSU_OUT_VOL = "/sys/switch/psu/psu{}/out_vol"
    PSU_OUT_CURR = "/sys/switch/psu/psu{}/out_curr"
    PSU_OUT_POWER = "/sys/switch/psu/psu{}/out_power"
    # PSU_MAX_OUT_VOL = "/sys/switch/psu/psu{}/in_curr"
    # PSU_MAX_OUT_CURR = "/sys/switch/psu/psu{}/in_curr"
    # PSU_MAX_OUT_POWER = "/sys/switch/psu/psu{}/in_curr"

    def __init__(self):
        with open('/usr/local/bin/self_detect/kis_platform_table.json', 'r') as f:
            kis_json_str = json.load(f)
        enmu_def_keys = kis_json_str.keys()
        enmu_def_dict = kis_json_str
        self.file_psu_count = int(enmu_def_dict['psu_count']['val'])
        self.file_psu_start_count = int(enmu_def_dict['psu_count']['start_num'])

    def call_retcode(self, cmd):
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, shell=True)
        outs = p.communicate()
        return outs[0].decode()+outs[1].decode()

    def previous_setting(self):
        pass
    # restore psu auto mode
    def restore_default_setting(self):
        pass

    def read_sysfs(self,file):
        ret_str = self.call_retcode("cat {}".format(file))
        if 'No such' in ret_str:
            return False
        else:
            return ret_str
        return False

    def get_psu_count(self):
        try:
            return int(self.read_sysfs(self.PSU_COUNT_NODE),10)
        except :
            print("get psu count failed")
            return False

    def test_psu_info(self):
        self.psu_count = self.get_psu_count()
        if(self.psu_count != False):
            return True
        return False

    def test_psu_status(self):
        ret = True
        if self.psu_count != self.file_psu_count:
            print("psu count fail.please check json file !")
            return False

        for psu_id in range(self.file_psu_start_count, self.psu_count+self.file_psu_start_count):
            PSU_COUNT_NODE = "/sys/switch/psu/num"
            PSU_IN_VOL = "/sys/switch/psu/psu{}/in_vol"
            PSU_IN_CURR = "/sys/switch/psu/psu{}/in_curr"
            PSU_IN_POWER = "/sys/switch/psu/psu{}/in_power"
            PSU_MAX_IN_VOL = "/sys/switch/psu/psu{}/alarm_threshold_vol"
            PSU_MAX_IN_CURR = "/sys/switch/psu/psu{}/alarm_threshold_curr"
            PSU_MAX_IN_POWER =  "/sys/switch/psu/psu{}/max_output_power"
            PSU_OUT_VOL = "/sys/switch/psu/psu{}/out_vol"
            PSU_OUT_CURR = "/sys/switch/psu/psu{}/out_curr"
            PSU_OUT_POWER = "/sys/switch/psu/psu{}/out_power"
            in_vol = float(self.read_sysfs(self.PSU_IN_VOL.format(psu_id)))
            in_curr = float(self.read_sysfs(self.PSU_IN_CURR.format(psu_id)))
            in_power = float(self.read_sysfs(self.PSU_IN_POWER.format(psu_id)))
            max_in_vol = float(self.read_sysfs(self.PSU_MAX_IN_VOL.format(psu_id)))
            max_in_curr = float(self.read_sysfs(self.PSU_MAX_IN_CURR.format(psu_id)))
            max_in_power = float(self.read_sysfs(self.PSU_MAX_IN_POWER.format(psu_id)))
            out_vol = float(self.read_sysfs(self.PSU_OUT_VOL.format(psu_id)))
            out_curr = float(self.read_sysfs(self.PSU_OUT_CURR.format(psu_id)))
            out_power = float(self.read_sysfs(self.PSU_OUT_POWER.format(psu_id)))
            print("psu{}  in_vol:{}  in_curr:{}  in_power:{}  out_vol:{}  out_curr:{}  out_power:{}".format \
                  (psu_id, in_vol, in_curr, in_power, out_vol, out_curr, out_power))
            # running_state = self.read_sysfs(f'/sys/s3ip/psu/psu{n}/motor{i}/speed')
            if in_vol > max_in_vol or in_vol < 200:
                print("test_psu_status psu{} in vol fail.".format(psu_id))
                ret = False
            if in_curr > max_in_curr:
                print("test_psu_status psu{} in curr fail.".format(psu_id))
                ret = False
            if in_power > max_in_power:
                print("test_psu_status psu{} in power fail.".format(psu_id))
                ret = False
            if out_vol < 10.8 or out_vol > 13.2:
                print("test_psu_status psu{} out vol fail.".format(psu_id))
                ret = False
        if ret == True:
            print("test_psu_status PASS.")
        else:
            print("test_psu_status fail.")
        return ret

    def run_test(self):
        ret = self.test_psu_info()
        if ret != True :
            return ret
        ret = self.test_psu_status()
        if ret != True :
            return ret
        # ret = self.test_psu_control()
        # if ret != True :
        #     return ret
        return True
