#!/usr/bin/python3
import sys

RET_OK = 0
RET_ERR = -1

def dump_help():
    print("Usage: %s NAME" % (sys.argv[0]))
    print("NAME: syscpld_i2c / portcpld_i2c / fpga_i2c / clock_i2c")


def call_retcode(cmd):
    if sys.version_info.major == 2:
        # python2
        import commands
        return commands.getstatusoutput(cmd)
    else:
        # python3
        import subprocess
        return subprocess.getstatusoutput(cmd)


def check_clock_i2c():
    cmd_get_testreg = "i2cget -y -f 3 0x6c 0x09"
    ret, output = call_retcode(cmd_get_testreg)
    if(ret):
        print("cmd_set_testreg error:output[{}]".format(output))
        return RET_ERR

    if int(output, 16) != (0x00):
        print("check_syscpld_i2c failed")
        return RET_ERR
    print("check_i2c OK")
    return RET_OK


def test_portcpld_i2c(test_val):
    cmd_set_testreg = "i2cset -y -f 158 0x64 0x05 {}"
    cmd_get_testreg = "i2cget -y -f 158 0x64 0x05"
    check_val = (~test_val)&0xff
    ret, output = call_retcode(cmd_set_testreg.format(test_val))
    if(ret):
        print("cmd_set_testreg error:output[{}]".format(output))
        return RET_ERR
    ret, output = call_retcode(cmd_get_testreg)
    if(ret):
        print("cmd_set_testreg error:output[{}]".format(output))
        return RET_ERR

    if int(output, 16) != check_val:
        print("check_portcpld_i2c failed output:{} {}".format(output, check_val))
        return RET_ERR
    return RET_OK

def check_portcpld_i2c():
    if( test_portcpld_i2c(0x5a) != RET_OK ):
        return RET_ERR
    if( test_portcpld_i2c(0x5a) != RET_OK ):
        return RET_ERR
    print("check_i2c OK")
    return RET_OK


def test_syscpld_i2c(test_val):
    cmd_set_testreg = "i2cset -y -f 157 0x62 0x05 {}"
    cmd_get_testreg = "i2cget -y -f 157 0x62 0x05"
    check_val = (~test_val)&0xff
    ret, output = call_retcode(cmd_set_testreg.format(test_val))
    if(ret):
        print("cmd_set_testreg error:output[{}]".format(output))
        return RET_ERR
    ret, output = call_retcode(cmd_get_testreg)
    if(ret):
        print("cmd_set_testreg error:output[{}]".format(output))
        return RET_ERR

    if int(output, 16) != check_val:
        print("check_syscpld_i2c failed output:{} {}".format(output, check_val))
        return RET_ERR
    return RET_OK

def check_syscpld_i2c():
    if( test_syscpld_i2c(0x5a) != RET_OK ):
        return RET_ERR
    if( test_syscpld_i2c(0x5a) != RET_OK ):
        return RET_ERR
    print("check_i2c OK")
    return RET_OK

def __main__():
    if (len(sys.argv) != 2):
        dump_help()
        return -1
    elif (sys.argv[1] == "syscpld_i2c"):
        if(check_syscpld_i2c() == RET_ERR):
            exit(RET_ERR)
    elif (sys.argv[1] == "portcpld_i2c"):
        if(check_portcpld_i2c() == RET_ERR):
            exit(RET_ERR)
    elif (sys.argv[1] == "fpga_i2c"):
        if(check_syscpld_i2c() == RET_ERR):
            exit(RET_ERR)
    elif (sys.argv[1] == "clock_i2c"):
        if(check_clock_i2c() == RET_ERR):
            exit(RET_ERR)
    else:
        dump_help()
        return

__main__()
