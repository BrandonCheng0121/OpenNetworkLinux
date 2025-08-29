#!/bin/bash

prog="$0"

psu1_bus=1
psu1_addr=0x5a
psu2_bus=2
psu2_addr=0x59

usage() {
    echo "Usage: $prog <command> [command options]"
    echo
    echo "Commands:"
    #echo "  on: Power on microserver if not powered on already"
    #echo "    options:"
    #echo "      -f: Re-do power on sequence no matter if microserver has "
    #echo "          been powered on or not."
    #echo
    echo "  off: Power off microserver ungracefully"
    echo
    echo "  sys_pwr_cycle: Power cycle system ungracefully"
    echo
    echo "  bmc_reset: Reset BMC system"
    echo
}

do_on() {
    echo "Done"
}

do_off() {
    # use CPLD1 PSU on/off(PSON#) control
    busybox devmem 0xfc80101f 8 0x03
    # if [ "$bmc_ok"x = "1"x ]
    # then
    #     ipmitool chassis power off
    # else
    #     # reg 0x02(ON_OFF_CONFIG)   0x15: means PSU on/off only by PSON# control.
    #     #                           0x19: means PSU on/off only by PMBus control
    #     i2cset -f -y $psu1_bus $psu1_addr 0x2 0x19
    #     i2cset -f -y $psu2_bus $psu2_addr 0x2 0x19
    #     # use psu reg 0x01(OPERATION), 
    #     #   0x0: off
    #     #   0x80: power on
    #     i2cset -f -y $psu1_bus $psu1_addr 0x1 0x0
    #     i2cset -f -y $psu2_bus $psu2_addr 0x1 0x0
    # fi

    echo "Done"
}

# same as off
do_sys_pwr_cycle() {
    # use CPLD1 PSU on/off(PSON#) control
    busybox devmem 0xfc80101f 8 0x03

    # if [ "$bmc_ok"x = "1"x ]
    # then
    #     ipmitool chassis power cycle
    # else
    #     # reg 0x02(ON_OFF_CONFIG)   0x15: means PSU on/off only by PSON# control.
    #     #                           0x19: means PSU on/off only by PMBus control
    #     i2cset -f -y $psu1_bus $psu1_addr 0x2 0x19
    #     i2cset -f -y $psu2_bus $psu2_addr 0x2 0x19
    #     # use psu reg 0x01(OPERATION), 
    #     #   0x0: off, 
    #     #   0x80: power on
    #     i2cset -f -y $psu1_bus $psu1_addr 0x1 0x0
    #     i2cset -f -y $psu2_bus $psu2_addr 0x1 0x0
    # fi

    echo "Done"
}

do_bmc_reset() {
    # reset BMC
    busybox devmem 0xfc80100f 8 0x06
    sleep 1
    # Record the BMC reboot reason
    busybox devmem 0xfc801086 8 0x04
    # recover BMC
    busybox devmem 0xfc80100f 8 0x0e
    echo "Done"
}

if [ $# -lt 1 ]; then
    usage
    exit -1
fi

command="$1"
shift

bmc_ok=$(cat /sys/switch/bmc/enable)

case "$command" in
    #on)
    #    do_on $@
    #    exit $ret
    #    ;;
    off)
        echo "WARNING: Power off SYSTEM by turning off PSU after 3 seconds..."
        sync;sync;sync;
        sleep 3
        do_off $@
        exit $ret
        ;;
    sys_pwr_cycle)
        echo "WARNING: Power cycle SYSTEM by turning off/on PSU after 3 seconds..."
        sync;sync;sync;
        sleep 3
        do_sys_pwr_cycle $@
        ;;
    bmc_reset)
        echo "WARNING: BMC reset after 3 seconds..."
        sync;sync;sync;
        sleep 3
        do_bmc_reset $@
        ;;
    *)
        usage
        exit -1
        ;;
esac

exit $?
