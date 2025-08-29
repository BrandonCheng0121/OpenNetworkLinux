#!/usr/bin/env python
#
# Copyright (C) 2016 Accton Networks, Inc.
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

"""
Usage: %(scriptName)s [options] command object

options:
    -h | --help     : this help message
    -d | --debug    : run with debug mode
    -f | --force    : ignore error during installation or clean
command:
    install     : install drivers and generate related sysfs nodes
    clean       : uninstall drivers and remove related sysfs nodes
    show        : show all systen status
    sff         : dump SFP eeprom
    set         : change board setting with fan|led|sfp
"""
import syslog
import os
import sys, getopt
import logging
import re
import time
from collections import namedtuple

PROJECT_NAME = '6865-48s8cq-w1'
version = '0.1.0'
verbose = False
DEBUG = False
args = []
ALL_DEVICE = {}
DEVICE_NO = {'led':4, 'fan':16,'thermal':4, 'sfp':56}
FORCE = 0
BMC_STATUS = False
DRIVER_INIT_DONE_FLAG = "/run/sonic_init_driver_done.flag"
#logging.basicConfig(filename= PROJECT_NAME+'.log', filemode='w',level=logging.DEBUG)
#logging.basicConfig(level=logging.INFO)

qsfp_start_index = 24
si5395_bus = ['2-006b', '9-006b', '9-0069']
si5395_reg = ['sticky_lol', 'sticky_los', 'sticky_losxaxb', 'sticky_oof']
pg_history_alarm_clr = ['0x38', '0x39', '0x3a', '0x3b', '0x3c', '0x3d', '0x3e', '0x3f']

if DEBUG == True:
    print (sys.argv[0])
    print ('ARGV      :', sys.argv[1:])

def SYS_LOG(level, x):
    syslog.syslog(level, x)

def SYS_LOG_WARN(x):
    SYS_LOG(syslog.LOG_WARNING, x)
def SYS_LOG_INFO(x):
    SYS_LOG(syslog.LOG_INFO, x)

def main():
    global DEBUG
    global args
    global FORCE

    if len(sys.argv)<2:
        show_help()
    adapter_cpu_type()
    options, args = getopt.getopt(sys.argv[1:], 'hdf', ['help',
                                                       'debug',
                                                       'force',
                                                          ])
    if DEBUG == True:
        print (options)
        print (args)
        print (len(sys.argv))

    for opt, arg in options:
        if opt in ('-h', '--help'):
            show_help()
        elif opt in ('-d', '--debug'):
            DEBUG = True
            logging.basicConfig(level=logging.INFO)
        elif opt in ('-f', '--force'):
            FORCE = 1
        else:
            logging.info('no option')
    for arg in args:
        if arg == 'install':
            do_install()
        elif arg == 'clean':
            do_uninstall()
        elif arg == 'show':
            device_traversal()
        elif arg == 'sff':
            if len(args)!=2:
                show_eeprom_help()
            elif int(args[1]) ==0 or int(args[1]) > DEVICE_NO['sfp']:
                show_eeprom_help()
            else:
                show_eeprom(args[1])
            return
        elif arg == 'set':
            if len(args)<3:
                show_set_help()
            else:
                set_device(args[1:])
            return
        else:
            show_help()

    return 0

def show_help():
    print (__doc__ % {'scriptName' : sys.argv[0].split("/")[-1]})
    sys.exit(0)

def  show_set_help():
    cmd =  sys.argv[0].split("/")[-1]+ " "  + args[0]
    print  (cmd +" [led|sfp|fan]")
    print  ("    use \""+ cmd + " led 0-1 \"  to set led color")
    print  ("    use \""+ cmd + " fan 0-100\" to set fan duty percetage")
    print  ("    use \""+ cmd + " sfp 1-20 {0|1}\" to set sfp# tx_disable")
    sys.exit(0)

def  show_eeprom_help():
    cmd =  sys.argv[0].split("/")[-1]+ " "  + args[0]
    print  ("    use \""+ cmd + " 1-20 \" to dump sfp# eeprom")
    sys.exit(0)

def my_log(txt):
    if DEBUG == True:
        print ("[ROY]"+txt)
    return

def log_os_system(cmd, show):
    logging.info('Run :'+cmd)
    if sys.version_info.major == 2:
        # python2
        import commands
        status, output = commands.getstatusoutput(cmd)
    else:
        # python3
        import subprocess
        status, output = subprocess.getstatusoutput(cmd)
    my_log (cmd +"with result:" + str(status))
    my_log ("      output:"+output)
    if status:
        logging.info('Failed :'+cmd)
        if show:
            print('Failed :'+cmd)
    return  status, output

def print_cmd(cmd,show):
    print (cmd)
    return 0,0

def driver_check():
    global kos

    ret, lsmod = log_os_system("cat /proc/modules", 0)
    SYS_LOG_INFO('mods %d: ' %(ret) + lsmod)
    if ret :
        return False

    lost_driver_list = []
    for driver_line in kos:
        kernel_driver = driver_line.split()[1]
        kernel_driver = kernel_driver.replace("-", "_")
        if not kernel_driver in lsmod:
            lost_driver_list.append(kernel_driver)
    if len(lost_driver_list) > 0:
        SYS_LOG_INFO('Need install driver: %s' %(lost_driver_list))
        return False

    return True

def check_bmc_status():
    global BMC_STATUS

    status, output = log_os_system("cat /sys/switch/bmc/status", 1)
    # check if BMC OK
    if(status == 0) and (output == "1"):
        BMC_STATUS_1 = True
        print("BMC present and work.")
    else:
        BMC_STATUS_1 = False
        print("BMC not exist.")

    status, output = log_os_system("cat /sys/switch/bmc/enable", 1)
    # check if BMC enable
    if(status == 0) and (output == "1"):
        BMC_STATUS_2 = True
        print("BMC enable.")
    else:
        BMC_STATUS_2 = False
        print("BMC disable.")
    if(BMC_STATUS_1 == True) and (BMC_STATUS_2 == True):
        BMC_STATUS = True
    else:
        BMC_STATUS = False

    return BMC_STATUS

# kos_bmc =  [
#     'modprobe ipmi_devintf',
#     'modprobe ipmi_ssif',
#     'modprobe ipmi_msghandler',
#     'modprobe ipmi_si',
# ]
kos = [
# 'modprobe ipmi_devintf',
'modprobe i2c_dev',
'modprobe i2c_mux_pca954x force_deselect_on_exit=1',
'modprobe fruid_eeprom_parse',
'modprobe cpld_lpc_driver',
'modprobe fpga_driver',
'modprobe cpld_i2c_driver',
'modprobe switch_at24',
# 'modprobe at24',
'modprobe switch_lm75',
'modprobe switch_pmbus_core',
'modprobe max34440',
'modprobe mp29xx',
'modprobe lm25066',
'modprobe psu_g1156',
# 'modprobe mtd',
# 'modprobe spi-nor',
# 'modprobe i2c-imc',
# 'modprobe switch_intel-spi',
# 'modprobe intel-spi-platform',
'modprobe switch_optoe',
# 'modprobe switch_coretemp',

'modprobe switch_mxsonic_switch',
'modprobe switch_mxsonic_fan',
'modprobe switch_mxsonic_psu',
'modprobe switch_mxsonic_sensor',
'modprobe switch_mxsonic_cpld',
'modprobe switch_mxsonic_fpga',
'modprobe switch_mxsonic_transceiver',
'modprobe switch_mxsonic_watchdog',
'modprobe switch_mxsonic_cpu',
'modprobe switch_mxsonic_sysled',
'modprobe switch_mxsonic_bmc',
'modprobe switch_mxsonic_avs',
'modprobe switch_mxsonic_pcie',

'modprobe switch_s3ip_switch',

'modprobe switch_s3ip_watchdog',
'modprobe switch_wdt_driver',

'modprobe switch_s3ip_cpld',
'modprobe switch_cpld_driver',

'modprobe switch_s3ip_fpga',
'modprobe switch_fpga_driver',

'modprobe switch_s3ip_sensor',
'modprobe switch_sensor_driver',

'modprobe switch_s3ip_psu',
'modprobe switch_psu_driver',

'modprobe switch_s3ip_fan',
'modprobe switch_fan_driver',

'modprobe switch_s3ip_sysled',
'modprobe switch_led_driver',

'modprobe switch_s3ip_syseeprom',
'modprobe switch_mb_driver',

'modprobe switch_s3ip_transceiver',
'modprobe switch_transceiver_driver',

'modprobe switch_bmc_driver',
'modprobe switch_avs_driver',

'modprobe switch_cpu_driver',
'modprobe switch_pcie_driver',
'modprobe dying_record',
]

def driver_install():
    global FORCE

    status, output = log_os_system("depmod", 1)
    status, output = log_os_system("modprobe -r i2c_ismt", 1)
    status, output = log_os_system("modprobe -r i2c_i801", 1)
    status, output = log_os_system("modprobe -r i2c_smbus", 1)
    # status, output = log_os_system("modprobe switch_i2c_smbus", 1)
    status, output = log_os_system("modprobe msr", 1)
    status, output = log_os_system("modprobe i2c-i801 disable_features=0x10", 1)
    '''
    status, output = log_os_system("modprobe -r intel-spi-platform", 1)
    status, output = log_os_system("modprobe -r intel_spi", 1)
    status, output = log_os_system("modprobe -r optoe", 1)
    # status, output = log_os_system("modprobe intel-spi-platform", 1)
    '''
    for i in range(0,len(kos)):
        status, output = log_os_system(kos[i], 1)
        #status, output = print_cmd(kos[i],1)
        if status:
            if FORCE == 0:
                return status

    return 0

def driver_uninstall():
    global FORCE

    for i in range(0,len(kos)):
        #remove parameter if any
        rm = kos[-(i+1)]
        lst = rm.split(" ")
        if len(lst) > 2:
            del(lst[2:])
        rm = " ".join(lst)

        #Change to removing commands
        rm = rm.replace("modprobe", "modprobe -rq")
        rm = rm.replace("insmod", "rmmod")
        status, output = log_os_system(rm, 1)

        #status, output = print_cmd(rm, 1)
        if status:
            if FORCE == 0:
                return status

    return 0

led_prefix ='/sys/class/leds/'+PROJECT_NAME+'_'
hwmon_types = {'led': ['dx510_fan','dx510_alarm','dx510_fpga_id','dx510_pwr']}
hwmon_nodes = {'led': ['brightness'] }
hwmon_prefix ={'led': led_prefix}

i2c_prefix = '/sys/bus/i2c/devices/'
i2c_cpld = '/sys/cpld/'
i2c_bus = {'fan': ['6-0066'] ,
           'thermal': ['3-004b','3-004c', '10-0048','10-0049'],
           'sfp': ['-0050']}
i2c_nodes = {'fan': ['input'],
           'thermal': ['hwmon/hwmon*/temp1_input'],
           'sfp': ['present']}

sfp_map =  [101,102,103,104,
            105,106,107,108,
            109,110,111,112,
            113,114,115,116,
            117,118,119,120,
            121,122,123,124,
            125,126,127,128,
            129,130,131,132,
            133,134,135,136,
            137,138,139,140,
            141,142,143,144,
            145,146,147,148,
            149,150,151,152,
            153,154,155,156
            ]

mknod =[
'echo pca9548 0x70 > /sys/bus/i2c/devices/i2c-0/new_device',

'echo switch_lm75 0x4C > /sys/bus/i2c/devices/i2c-7/new_device',
'echo switch_lm75 0x4B > /sys/bus/i2c/devices/i2c-7/new_device',
'echo switch_lm75 0x4A > /sys/bus/i2c/devices/i2c-7/new_device',

'echo max34461 0x74 > /sys/bus/i2c/devices/i2c-8/new_device',
'echo lm25066 0x40 > /sys/bus/i2c/devices/i2c-4/new_device',
'echo mp2976 0x5f > /sys/bus/i2c/devices/i2c-4/new_device',
'echo mp2973 0x20 > /sys/bus/i2c/devices/i2c-4/new_device',

'echo psu_g1156_0 0x5a > /sys/bus/i2c/devices/i2c-1/new_device',
'echo psu_g1156_1 0x59 > /sys/bus/i2c/devices/i2c-2/new_device',

'echo switch_24c64 0x50 > /sys/bus/i2c/devices/i2c-0/new_device',
'echo switch_24c64 0x51 > /sys/bus/i2c/devices/i2c-6/new_device',
'echo switch_24c64 0x55 > /sys/bus/i2c/devices/i2c-6/new_device',

'echo muxi_sys_cpld 0x62 > /sys/bus/i2c/devices/i2c-157/new_device',
'echo muxi_led_cpld1 0x64 > /sys/bus/i2c/devices/i2c-158/new_device',
]

mknod_bmc =[
'echo switch_24c64 0x50 > /sys/bus/i2c/devices/i2c-0/new_device',
'echo muxi_sys_cpld 0x62 > /sys/bus/i2c/devices/i2c-157/new_device',
'echo muxi_led_cpld1 0x64 > /sys/bus/i2c/devices/i2c-158/new_device',
]

def device_install():
    global FORCE

    # Check the BMC status
    check_bmc_status()

    if BMC_STATUS is False:
        nodes = mknod
    else:
        nodes = mknod_bmc
    # log_os_system("busybox devmem 0xfc801025 8 0x00", 1) #change iic channal to COMe
    for i in range(0,len(nodes)):
        #for pca954x need times to built new i2c buses
        if nodes[i].find('pca954') != -1:
           time.sleep(1)

        status, output = log_os_system(nodes[i], 1)
        #status, output = print_cmd(nodes[i],1)
        if status:
            print (output)
            if FORCE == 0:
                return status

    # FPGA IIC
    for i in range(0,len(sfp_map)):
        path = "/sys/bus/i2c/devices/i2c-"+str(sfp_map[i])+"/new_device"
        if i < 48:
            status, output =log_os_system("echo optoe2 0x50 > " + path, 1)
        else:
            status, output =log_os_system("echo optoe1 0x50 > " + path, 1)
        #status, output = print_cmd("echo optoe3 0x50 > " + path, 1)

        #Add port name for ks sonic optoe driver
        port_name_path = "/sys/bus/i2c/devices/"+str(sfp_map[i])+"-0050/port_name"
        status, output = log_os_system("echo eth"+str(i+1)+" > "+ port_name_path, 1)

        if status:
            print (output)
            if FORCE == 0:
                return status

    return

def device_uninstall():
    global FORCE

    if BMC_STATUS is False:
        nodelist = mknod
    else:
        nodelist = mknod_bmc

    for i in range(0,len(sfp_map)):
        target = "/sys/bus/i2c/devices/i2c-"+str(sfp_map[i])+"/delete_device"
        status, output = log_os_system("echo 0x50 > "+ target, 1)
        #status, output = print_cmd("echo 0x50 > "+ target, 1)
        if status:
            print (output)
            if FORCE == 0:
                return status

    # FPGA IIC
    for i in range(len(nodelist)):
        target = nodelist[-(i+1)]
        temp = target.split()
        del temp[1]
        temp[-1] = temp[-1].replace('new_device', 'delete_device')
        status, output = log_os_system(" ".join(temp), 1)
        #status, output = print_cmd(" ".join(temp), 1)
        if status:
            print (output)
            if FORCE == 0:
                return status

    return

# CFG.TCK_SETUP=0
# CFG.TCK_HOLD=0
# Detect 2 devices
# #0 MachXO3LF-6900C (0x612bd043)
# #1 MachXO3LF-2100C-BG324 (0xe12bc043)
# def cpld_jtag_check():
#     cmd = 'cpldupd_sec -s MX7327'
#     status, output = log_os_system(cmd, 1)
#     if status:
#         print("error:CPLD JTAG :{}".format(output))
#         return
#     if ("MachXO3LF-6900C" not in output) or ("MachXO3LF-2100C-BG324" not in output):
#         print("error:CPLD JTAG :{}".format(output))
#         return
#     print("CPLD JTAG check OK")

def adapter_cpu_type():
    cmd = 'lscpu | grep "Model name"'
    status, output = log_os_system(cmd, 1)
    cfg_path = "/lib/platform-config/x86-64-muxi-dcs6500-48z8c-r0/onl/components_fw"
    json_path = "/lib/platform-config/x86-64-muxi-dcs6500-48z8c-r0/onl/device"
    if("D-1627" in output):
        print("CPU is D-1627")
        cpu_cmd1 = "cp {}/muxi_fw_version_info_D1627.cfg {}/muxi_fw_version_info.cfg".format(cfg_path, cfg_path)
        cpu_cmd2 = "cp {}/platform_components_D1627.json {}/platform_components.json".format(json_path, json_path)
    else:
        print("CPU is C3508")
        cpu_cmd1 = "cp {}/muxi_fw_version_info_C3508.cfg {}/muxi_fw_version_info.cfg".format(cfg_path, cfg_path)
        cpu_cmd2 = "cp {}/platform_components_C3508.json {}/platform_components.json".format(json_path, json_path)
    status, output = log_os_system(cpu_cmd1, 1)
    status, output = log_os_system(cpu_cmd2, 1)

def init_avs():
    bin_path = "/lib/platform-config/x86-64-muxi-dcs6500-48z8c-r0/onl/bin"
    cmd = '/usr/bin/python -u {}/muxi_avs.py'.format(bin_path)
    status, output = log_os_system(cmd, 1)
    print(output)

def init_fan_eeprom():
    cmd = 'cat /sys/switch/fan/fan1/part_number'
    status, output = log_os_system(cmd, 1)
    cmd = 'cat /sys/switch/fan/fan2/part_number'
    status, output = log_os_system(cmd, 1)
    cmd = 'cat /sys/switch/fan/fan3/part_number'
    status, output = log_os_system(cmd, 1)
    cmd = 'cat /sys/switch/fan/fan4/part_number'
    status, output = log_os_system(cmd, 1)

def system_ready():
    if driver_check() == False:
        return False
    if not device_exist():
        return False
    return True

def recode_reboot_count():
    #Update /usr/local/bin/reboot-count.txt for statistics number
    status, output = log_os_system("cat /sys/switch/cpld/reboot_cause", 1)
    if output.strip() == "1":# reboot cause power loss clear count
        if os.path.exists("/usr/local/bin/reboot-count.txt"):
            log_os_system("rm -rf /usr/local/bin/reboot-count.txt", 1)

    if os.path.exists("/usr/local/bin/reboot-count.txt"):
        ret, output = log_os_system("cat /usr/local/bin/reboot-count.txt", 1)
        if ret == 0:
            output = int(output) + 1
            cmd = "echo %d > /usr/local/bin/reboot-count.txt" % (output)
            log_os_system(cmd, 1)
        else:
            print("Update reboot-count.txt for statistics number failed")
    else:
        log_os_system("echo 1 > /usr/local/bin/reboot-count.txt", 1)

def do_install():
    if os.path.exists(DRIVER_INIT_DONE_FLAG):
        print("Drive installed, No need for repeated installation")
        return
    print ("Checking system....")
    if driver_check() == False:
        print ("Driver not installed, Now installing....")
        status = driver_install()
        time.sleep(2)
        if status:
            if FORCE == 0:
                return status
    else:
        print (PROJECT_NAME.upper()+" drivers detected....")

    if not device_exist():
        print ("Device not installed, Now installing....")
        status = device_install()
        if status:
            if FORCE == 0:
                return  status
    else:
        print (PROJECT_NAME.upper()+" devices detected....")

    #Add /dev/ipmi0 permissions
    if os.path.exists("/dev/ipmi0"):
        log_os_system("chmod 777 /dev/ipmi0", 1)

    #Create a link /sys/switch to /sys_switch
    if os.path.isdir("/sys_switch"):
        if not os.path.islink("/sys_switch"):
            log_os_system("rm -rf /sys_switch", 1)
            log_os_system("ln -s /sys/switch /sys_switch", 1)
    else:
        log_os_system("ln -s /sys/switch /sys_switch", 1)

    log_os_system("busybox devmem 0xfbb00e00 32 0x00078426", 1) # set i210 led 83ms
    if not os.path.exists(DRIVER_INIT_DONE_FLAG):
        os.mknod(DRIVER_INIT_DONE_FLAG)
    init_avs()
    init_fan_eeprom()
    # cpld_jtag_check()
    recode_reboot_count()
    # log_os_system("busybox devmem 0xfc801021 8 1", 0) # Eliminate invalid interrupts
    log_os_system("busybox devmem 0xfc80106f 8 0xff", 0) # Eliminate invalid interrupts
    log_os_system("i2cset -y -f 158 0x64 0x37 0x07", 0) # Eliminate invalid interrupts
    return

def do_uninstall():
    print ("Checking system....")
    if not device_exist():
        print (PROJECT_NAME.upper() +" has no device installed....")
    else:
        print ("Removing device....")
        status = device_uninstall()
        if status:
            if FORCE == 0:
                return  status

    if driver_check()== False :
        print (PROJECT_NAME.upper() +" has no driver installed....")
    else:
        print ("Removing installed driver....")
        status = driver_uninstall()
        if os.path.exists(DRIVER_INIT_DONE_FLAG):
            os.remove(DRIVER_INIT_DONE_FLAG)
        if status:
            if FORCE == 0:
                return status

    return

def devices_info():
    global DEVICE_NO
    global ALL_DEVICE
    global i2c_bus, hwmon_types
    for key in DEVICE_NO:
        ALL_DEVICE[key]= {}
        for i in range(0,DEVICE_NO[key]):
            ALL_DEVICE[key][key+str(i+1)] = []

    for key in i2c_bus:
        buses = i2c_bus[key]
        nodes = i2c_nodes[key]
        for i in range(0,len(buses)):
            for j in range(0,len(nodes)):
                if  'fan' == key:
                    for k in range(0,DEVICE_NO[key]):
                        node = key+str(k+1)
                        path = i2c_cpld+"fan"+str(k+1)+"_"+ nodes[j]
                        my_log(node+": "+ path)
                        ALL_DEVICE[key][node].append(path)
                elif  'sfp' == key:
                    for k in range(0,DEVICE_NO[key]):
                        node = key+str(k+1)
                        if k >= qsfp_start_index:
                            fmt = i2c_prefix+"7-0064/{0}_{1}"
                            path =  fmt.format(nodes[j], (k%qsfp_start_index)+2)
                        else:
                            fmt = i2c_prefix+"8-0063/{0}_{1}"
                            path =  fmt.format(nodes[j], (k%qsfp_start_index)+1)

                        my_log(node+": "+ path)
                        ALL_DEVICE[key][node].append(path)
                else:
                    node = key+str(i+1)
                    path = i2c_prefix+ buses[i]+"/"+ nodes[j]
                    my_log(node+": "+ path)
                    ALL_DEVICE[key][node].append(path)

    for key in hwmon_types:
        itypes = hwmon_types[key]
        nodes = hwmon_nodes[key]
        for i in range(0,len(itypes)):
            for j in range(0,len(nodes)):
                node = key+"_"+itypes[i]
                path = hwmon_prefix[key]+ itypes[i]+"/"+ nodes[j]
                my_log(node+": "+ path)
                ALL_DEVICE[key][ key+str(i+1)].append(path)

    #show dict all in the order
    if DEBUG == True:
        for i in sorted(ALL_DEVICE.keys()):
            print(i+": ")
            for j in sorted(ALL_DEVICE[i].keys()):
                print("   "+j)
                for k in (ALL_DEVICE[i][j]):
                    print("   "+"   "+k)
    return

def show_eeprom(index):
    if system_ready()==False:
        print("System's not ready.")
        print("Please install first!")
        return

    i = int(index)-1
    node = i2c_prefix+ str(sfp_map[i])+ i2c_bus['sfp'][0]+"/"+ 'eeprom'
    # check if got hexdump command in current environment
    ret, log = log_os_system("which hexdump", 0)
    ret, log2 = log_os_system("which busybox hexdump", 0)
    if len(log):
        hex_cmd = 'hexdump'
    elif len(log2):
        hex_cmd = ' busybox hexdump'
    else:
        log = 'Failed : no hexdump cmd!!'
        logging.info(log)
        print (log)
        return 1

    print (node + ":")
    ret, log = log_os_system(hex_cmd +" -C "+node, 1)
    #ret, log = print_cmd(hex_cmd +" -C "+node, 1)
    if ret==0:
        print (log)
    else:
        print ("**********device no found**********")
    return

def set_device(args):
    global DEVICE_NO
    global ALL_DEVICE
    if system_ready()==False:
        print("System's not ready.")
        print("Please install first!")
        return

    if len(ALL_DEVICE)==0:
        devices_info()

    if args[0]=='led':
        if int(args[1])>4:
            show_set_help()
            return
        #print  ALL_DEVICE['led']
        for i in range(0,len(ALL_DEVICE['led'])):
            for k in (ALL_DEVICE['led']['led'+str(i+1)]):
                ret, log = log_os_system("echo "+args[1]+" >"+k, 1)
                #ret, log = print_cmd("echo "+args[1]+" >"+k, 1)
                if ret:
                    return ret
    elif args[0]=='fan':
        if int(args[1])>100:
            show_set_help()
            return
        #print  ALL_DEVICE['fan']
        #fan1~8 is all fine, all fan share same setting
        node = ALL_DEVICE['fan'] ['fan1'][0]
        node = node.replace(node.split("/")[-1], 'fan1_pwm')
        ret, log = log_os_system("cat "+ node, 1)
        #ret, log = print_cmd("cat "+ node, 1)
        if ret==0:
            print ("Previous fan duty: " + log.strip() +"%")
            #print ("Previous fan duty: " +"%")
        ret, log = log_os_system("echo "+args[1]+" >"+node, 1)
        #ret, log = print_cmd("echo "+args[1]+" >"+node, 1)
        if ret==0:
            print ("Current fan duty: " + args[1] +"%")
        return ret
    elif args[0]=='sfp':
        if int(args[1])> DEVICE_NO[args[0]] or int(args[1])==0:
            show_set_help()
            return
        if len(args)<2:
            show_set_help()
            return

        if int(args[2])>1:
            show_set_help()
            return

        #print  ALL_DEVICE[args[0]]
        for i in range(0,len(ALL_DEVICE[args[0]])):
            for j in ALL_DEVICE[args[0]][args[0]+str(args[1])]:
                if j.find('tx_disable')!= -1:
                    ret, log = log_os_system("echo "+args[2]+" >"+ j, 1)
                    #ret, log = print_cmd("echo "+args[2]+" >"+ j, 1)
                    if ret:
                        return ret

    return

#get digits inside a string.
#Ex: 31 for "sfp31"
def get_value(input):
    digit = re.findall('\d+', input)
    return int(digit[0])

def device_traversal():
    if system_ready()==False:
        print("System's not ready.")
        print("Please install first!")
        return

    if len(ALL_DEVICE)==0:
        devices_info()
    for i in sorted(ALL_DEVICE.keys()):
        print("============================================")
        print(i.upper()+": ")
        print("============================================")

        for j in sorted(ALL_DEVICE[i].keys(), key=get_value):
            print ("   "+j+":"),
            for k in (ALL_DEVICE[i][j]):
                ret, log = log_os_system("cat "+k, 0)
                #ret, log = print_cmd("cat "+k, 0)
                func = k.split("/")[-1].strip()
                func = re.sub(j+'_','',func,1)
                func = re.sub(i.lower()+'_','',func,1)
                if func == "input":
                    if ret ==0:
                        print ("RPM"+"="+log+" "),
                    func = "present"
                    if int(log) < 1:
                        log = '0'
                    else:
                        log = '1'
                if ret==0:
                    print (func+"="+log+" "),
                    #print func+"="+" ",
                else:
                    print (func+"="+"X"+" "),
            print
            print("----------------------------------------------------------------")


        print
    return

def device_exist():
    check_path = [i2c_prefix+"101-0050", i2c_prefix+"0-0070"]
    if not os.path.exists(check_path[0]) or not os.path.exists(check_path[1]):
        return False
    return True

if __name__ == "__main__":
    main()
