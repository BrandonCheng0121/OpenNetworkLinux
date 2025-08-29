#!/usr/bin/python3
import json
import os
import subprocess
import sys
import time

def call_retcode(cmd):
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, shell=True)
    outs = p.communicate()
    return outs[0].decode()+outs[1].decode()

def dump_help():
    print("Usage: %s <component>" % (sys.argv[0]))
    print("")
    print("<component>: sys_cpld / port_cpld / fpga ")
    print("")

def dump_reg_syscpld():
    cmd = "busybox devmem 0xfc8010{:02x} 8"
    reg = 0
    ret = ""
    print("dump sys_cpld reg")
    for reg in range(0xFF+1):
        # print(cmd.format(reg))
        ret = call_retcode(cmd.format(reg))
        print("0x%02x:%s" % (reg, ret.strip('\n')))
    pass

def dump_reg_portcpld():
    cmd = "i2cget -y -f 158 0x64 0x{:02x}"
    reg = 0
    ret = ""
    print("dump port_cpld reg")
    for reg in range(0xFF+1):
        # print(cmd.format(reg))
        ret = call_retcode(cmd.format(reg))
        print("0x%02x:%s" % (reg, ret.strip('\n')))
    pass

def dump_reg_fpgacpld():
    
    cmd = "busybox devmem 0xfbc0{:04x} 32"
    reg = 0
    ret = ""
    print("dump fpga reg")
    for reg in range(0, 0x0044, 4):
        # print(cmd.format(reg))
        ret = call_retcode(cmd.format(reg))
        print("0x%04x:%s" % (reg, ret.strip('\n')))
    pass
    for reg in range(0x0400, 0x0B34, 4):
        # print(cmd.format(reg))
        ret = call_retcode(cmd.format(reg))
        print("0x%04x:%s" % (reg, ret.strip('\n')))
    pass
    for reg in range(0x1000, 0x1014, 4):
        # print(cmd.format(reg))
        ret = call_retcode(cmd.format(reg))
        print("0x%04x:%s" % (reg, ret.strip('\n')))
    pass

def dump_main():
    if(len(sys.argv)!=2):
        dump_help()
        return
    elif (sys.argv[1] != "sys_cpld" and \
        sys.argv[1] != "port_cpld" and \
        sys.argv[1] != "fpga" ):
        dump_help()
        return

    if(sys.argv[1] == "sys_cpld"):
        dump_reg_syscpld()
    if(sys.argv[1] == "port_cpld"):
        dump_reg_portcpld()
    if(sys.argv[1] == "fpga"):
        dump_reg_fpgacpld()

dump_main()