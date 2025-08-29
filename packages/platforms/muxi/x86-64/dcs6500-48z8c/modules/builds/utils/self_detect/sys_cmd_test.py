#!/usr/bin/python3
import json
import os
import time
import subprocess

class SYS_CMD_TEST():

    def __init__(self):
        pass

    def call_retcode(self, cmd):
        p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE, shell=True)
        outs = p.communicate()
        return outs[0].decode()+outs[1].decode()

    def previous_setting(self):
        pass
    # restore fan auto mode
    def restore_default_setting(self):
        pass

    def read_sysfs(self,file):
        ret_str = self.call_retcode("cat {}".format(file))
        if 'No such file' in ret_str:
            return False
        else:
            return ret_str
        return False

    def test_cpu_status(self):

        cmd = 'lscpu | egrep -e "^CPU\(s\):"'
        cpu_number_str = self.call_retcode(cmd).strip()
        if cpu_number_str != '':
            cpu_number = int(cpu_number_str.split()[1])
        print("CFreq check CPU number: %d" % (cpu_number))

        freq_check = 0
        CPU_CFreq_threshold = 750
        for index in range(cpu_number):
            CPU_CFreq_path = '/sys/devices/system/cpu/cpu%d/cpufreq/scaling_cur_freq' %(index)
            CPU_CFreq_value = int(self.call_retcode("timeout 3s cat %s" %(CPU_CFreq_path)).splitlines()[0])
            CPU_CFreq_value /= 1000

            if (CPU_CFreq_value < CPU_CFreq_threshold):
                freq_check = 1
                print("CFreq check error, CPU%d: %d" % (index, CPU_CFreq_value))
            else:
                print("CFreq check OK, CPU%d: %d" % (index, CPU_CFreq_value))
            time.sleep(0.1)

        time.sleep(2)

        ret = self.call_retcode("dmesg | grep 'Package temperature above threshold, cpu clock throttled'")

        if (ret == 0) or (freq_check == 1):
            # Package temperature above threshold
            return False
        else:
            return True

    def test_memfree_status(self):
        cmd = 'cat /proc/meminfo | grep MemTotal'
        MemTotal_str = self.call_retcode(cmd)
        print(MemTotal_str)
        cmd = 'cat /proc/meminfo | grep MemFree'
        MemFree_str = self.call_retcode(cmd)
        print(MemFree_str)
        if(float(MemTotal_str.split()[1])/1000/1000 < 16):
            print("MemTotal is less then 16GB")
            return False
        return True

    def test_ssd_status(self):
        SSDTotal_err = 0
        SSDHealthy_err = 0
        cmd ='smartctl -i /dev/sda | grep \'User Capacity\''
        SSDTotal_str = self.call_retcode(cmd)
        print (SSDTotal_str)
        SSDTotal = int(SSDTotal_str.split()[4].strip('['))
        if(SSDTotal < 230):
            SSDTotal_err = 1
            print("SSD Total capacity abnormal less than [230 GB]")

        cmd ='smartctl -H /dev/sda'
        SSDHealthy_str = self.call_retcode(cmd)
        if('PASSED' not in SSDHealthy_str):
            SSDHealthy_err = 1

        if(SSDHealthy_err == 1 or SSDTotal_err == 1):
            return False
        return True

    def test_show_status(self):
        cmd_tbl = [['sensors'],
        ['lspci -vv'],
        ['show platform fan'],
        ['show platform firmware status'],
        ['show platform firmware update'],
        ['show platform psustatus'],
        ['show platform syseeprom'],
        ['show platform temperature'],
        ['history'],
        ['free'],
        ['df'],
        ['date'], 
        ['uptime'], 
        ['show reboot-cause'], 
        ['show reboot-cause history']]
        for cmd in cmd_tbl:
            try:
                ret_str = self.call_retcode(cmd)
                if 'TypeError' in ret_str:
                    print('')
                else:
                    print(cmd)
                    print(ret_str)
            except:
                pass
        return True

    def test_sys_cmd_status(self):
        ret1 = self.test_show_status()
        ret2 = self.test_cpu_status()
        ret3 = self.test_memfree_status()
        ret4 = self.test_ssd_status()
        if(ret1 == False or ret2 == False or ret3 == False or ret4 == False):
            print("test_sys_cmd_status fail.")
            return False
        print("test_sys_cmd_status PASS.")
        return True

    def run_test(self):
        ret = self.test_sys_cmd_status()
        if ret != True :
            return ret
        return True

