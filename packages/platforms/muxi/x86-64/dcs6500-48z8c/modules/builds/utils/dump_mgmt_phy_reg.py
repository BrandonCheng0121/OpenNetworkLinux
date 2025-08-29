#!/usr/bin/env python3
try:
    import sys
    import os
    import time
    import re
    import socket
    from fcntl import ioctl
    import struct
except ImportError as e:
    raise ImportError("%s - required module not found" % str(e))

SIOCGMIIPHY  = 0x8947       #Get address of MII PHY in use.
SIOCGMIIREG  = 0x8948       #Read MII PHY register.
SIOCSMIIREG  = 0x8949       #Write MII PHY register.
MGMT_PHY_ADDR = 1
RET_OK  = 0
RET_NOK = -1
REG_RANGE = 0x1f
START_PRINT = 0x0
END_PRINT = 0x20
LINE_LEN = 0x10

def get_management_port_phy_reg(ifname,cmd,reg_offset):
    val_out = 0
    s = socket.socket()
    try:
        ifreq = ioctl(s.fileno(), cmd, struct.pack('16sHHHH', ifname[:15],MGMT_PHY_ADDR,reg_offset,0,0))
        s.close()
    except IOError as err:
        syslog.syslog(syslog.LOG_ERR, "read mgmt phyaddr 0x{:0>2x} cmd 0x{:0>2x} reg 0x{:0>2x} failed { }".format(MGMT_PHY_ADDR,cmd,reg_offset,err))
        s.close()
        return RET_NOK,val_out

    val_out = struct.unpack('16sHHHH', ifreq)[4]
    return RET_OK, val_out

def set_management_port_phy_reg(ifname,phyaddr,reg_offset,value):
    s = socket.socket()
    try:
        ifreq = ioctl(s.fileno(), SIOCSMIIREG, struct.pack('16sHHHH', ifname[:15],phyaddr,reg_offset,value,0))
        s.close()
    except IOError as err:
        syslog.syslog(syslog.LOG_ERR, "write mgmt phyaddr 0x{:0>2x} reg_offset 0x{:0>2x} value 0x{:0>2x} failed { }".format(MGMT_PHY_ADDR,reg_offset,value,err))
        s.close()
        return False
    
    return True

def dump_all():
    i = 0
    tmp_string = ""
    reg_value = {}
    print("phy reg dump: ")
    for i in range(START_PRINT,END_PRINT+1):
        ret, reg_value[i] = get_management_port_phy_reg(b"eth0",SIOCGMIIREG,i)
        if ret == RET_NOK:
            print("ifname:{} faile to dump mgmt phy register, err_reg{},value={}".format(ifname,i,reg_value[i]))
            return False

    for i in range(START_PRINT+1,END_PRINT+1):
        if i % LINE_LEN:
            tmp_string = tmp_string + " " + "{:0>4x}".format(reg_value[i-1])
        else :
            tmp_string ="0x{:0>8x}".format(i-LINE_LEN) + " " + tmp_string + " " + "{:0>4x}".format(reg_value[i-1])
            print(tmp_string)
            tmp_string = ""

    return True

def main(argv):
    dump_all()

if __name__ == '__main__':
    main(sys.argv)
