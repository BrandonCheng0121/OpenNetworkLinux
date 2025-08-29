#!/bin/bash
fpga_upd_tool_pcie="fpga_upd_pcie_sec"
fpga_upd_tool_lpc="fpga_upd_lpc_sec"
cpld_upd_tool="cpldupd_sec"
bios_upd_tool="flashrom_sec"

prog=`basename $0`
usage() {
    echo "Usage: $prog <component> <file_path>"
    echo "       $prog <component> update"
    echo
    echo "<component>: sys_cpld / port_cpld / fpga / fpga_lpc / MAIN_BIOS / BACKUP_BIOS "
    echo "    Use update option instead file_path will use defaule component files in components_fw folder."
    echo ""
}

if [ $# -lt 2 ]||[ $# -ge 3 ]; then
    usage
    exit -1
fi
component=$1

if [ "x$2" = "xupdate" ]; then
    echo "Use defaule component files in components_fw folder."
    while read line; do 
        name=`echo $line|awk -F '=' '{print $1}'`
        value=`echo $line|awk -F '=' '{print $2}'`
        case $name in 
            "SYS_CPLD_VERSION")
                sys_cpld_ver=$value
                cpld_flag="sys"
                ;;
            "PORT_CPLD_VERSION")
                port_cpld_ver=$value
                cpld_flag="port"
                ;;
            "CPLD_FILE_NAME")
                case $cpld_flag in
                    "sys")
                        sys_cpld_file=$value
                        ;;
                    "port")
                        port_cpld_file=$value
                        ;;
                    *)
                        ;;
                esac
                ;;
            "BIOS_VERSION")
                bios_ver=$value
                ;;
            "BIOS_FILE_NAME")
                bios_file=$value
                ;;
            "FPGA_VERSION")
                fpga_ver=$value
                ;;
            "FPGA_FILE_NAME")
                fpga_file=$value
                ;;
            *)
                ;;
        esac done < /lib/platform-config/x86-64-muxi-dcs6500-48z8c-r0/onl/components_fw/muxi_fw_version_info.cfg
    case "$component" in
        sys_cpld)
            echo 'SYS_CPLD_VERSION' $sys_cpld_ver
            echo 'CPLD_FILE_NAME' $sys_cpld_file
            echo ''
            file=$sys_cpld_file
            ;;
        port_cpld)
            echo 'PORT_CPLD_VERSION' $port_cpld_ver
            echo 'PORT_FILE_NAME' $port_cpld_file
            echo ''
            file=$port_cpld_file
            ;;
        fpga)
            echo 'FPGA_VERSION' $fpga_ver
            echo 'FPGA_FILE_NAME' $fpga_file
            echo ''
            file=$fpga_file
            ;;
        fpga_lpc)
            echo 'FPGA_VERSION' $fpga_ver
            echo 'FPGA_FILE_NAME' $fpga_file
            echo ''
            file=$fpga_file
            ;;
        MAIN_BIOS)
            echo 'BIOS_VERSION' $bios_ver
            echo 'BIOS_FILE_NAME' $bios_file
            echo ''
            file=$bios_file
            ;;
        BACKUP_BIOS)
            echo 'BIOS_VERSION' $bios_ver
            echo 'BIOS_FILE_NAME' $bios_file
            echo ''
            file=$bios_file
            ;;
        *)
            usage
            exit -1
            ;;
    esac
else
    file=$2
fi

case "$component" in
    sys_cpld)
        echo "sys cpld update"
        echo ''
        /lib/platform-config/current/onl/bin/$cpld_upd_tool -u MX7327 ${file} -c DO_REAL_TIME_ISP 1
        touch /run/cpld1_flag
        ;;
    port_cpld)
        echo "port cpld update"
        echo ''
        /lib/platform-config/current/onl/bin/$cpld_upd_tool -u MX7327 ${file} -c DO_REAL_TIME_ISP 1
        touch /run/cpld2_flag
        ;;
    fpga)
        echo "fpga update"
        echo ''
        /lib/platform-config/current/onl/bin/$fpga_upd_tool_pcie -O
        /lib/platform-config/current/onl/bin/$fpga_upd_tool_pcie -O
        /lib/platform-config/current/onl/bin/$fpga_upd_tool_pcie -E
        /lib/platform-config/current/onl/bin/$fpga_upd_tool_pcie -W -F ${file}
        touch /run/fpga_flag
        ;;
    fpga_lpc)
        echo "fpga update"
        echo ''
        busybox devmem 0xfc801012 8 0xB8
        /lib/platform-config/current/onl/bin/$fpga_upd_tool_lpc -O 0xfc800000
        /lib/platform-config/current/onl/bin/$fpga_upd_tool_lpc -O 0xfc800000
        /lib/platform-config/current/onl/bin/$fpga_upd_tool_lpc -E 0xfc800000
        /lib/platform-config/current/onl/bin/$fpga_upd_tool_lpc -W -F ${file} 0xfc800000
        busybox devmem 0xfc801012 8 0xB0
        touch /run/fpga_flag
        ;;
    MAIN_BIOS)
        echo "main bios update"
        echo ''
        EC_WDT_SPI spi 0x02
        /lib/platform-config/current/onl/bin/$bios_upd_tool -p internal -i bios -w ${file} --ifd -N
        ;;
    BACKUP_BIOS)
        echo "backup bios update"
        echo ''
        EC_WDT_SPI spi 0x03
        /lib/platform-config/current/onl/bin/$bios_upd_tool -p internal -i bios -w ${file} --ifd -N
        ;;
    *)
        usage
        exit -1
        ;;
esac

