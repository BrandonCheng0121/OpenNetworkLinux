#!/usr/bin/env python

import json
import sys
import os
import syslog
import time

fpga_upd_tool_pcie = "fpga_upd_pcie_sec"
fpga_upd_tool_lpc = "fpga_upd_lpc_sec"
cpld_upd_tool = "cpldupd_sec"
bios_upd_tool = "flashrom_sec"
json_file_path = '/lib/platform-config/x86-64-muxi-dcs6500-48z8c-r0/onl/device/platform_components.json'
BASE_PATH = "/sys/switch/"
CPLD_PATH = "cpld/cpld"
FPGA_PATH = "fpga/fpga"
BIOS_VERSION_PATH = "/sys/class/dmi/id/bios_version"

UPDATE_SUCCESS = 1
UPDATE_TO_DATE = 0
UPDATE_FAILED = -1
UPDATE_TAR_NOT_MATCH = 2

NO_NEED_UPDATE = 0
NEED_UPDATE = 1

sys_cpld_flag_path = "/run/cpld1_flag"
port_cpld_flag_path = "/run/cpld2_flag"
fpga_flag_path = "/run/fpga_flag"
UPDATE_FAIL_LOG_TEXT = 'AutoUpgradeFirmwareFail: The firmware upgrade failed(Name={}, ErrorCode={}).'

def get_command_status_output(cmd):
    if sys.version_info.major == 2:
        # python2
        import commands
        status, output = commands.getstatusoutput(cmd)
        status = status>>8
    else:
        # python3
        import subprocess
        status, output = subprocess.getstatusoutput(cmd)
    return  status, output
# def print_help():
#     print("Usage: {} <component> <file_path> [-f]".format(sys.argv[0]))
#     print("       {} <component> update [-f]".format(sys.argv[0]))
#     print("")
#     print("<component>: sys_cpld / port_cpld / fpga / MAIN_BIOS / BACKUP_BIOS ")
#     print("    Use update option instead file_path will use defaule component files in components_fw folder.")
#     print("    -f : Option to force update component without version validation.")

def update_fail_log(componet, ret):
    syslog.syslog(syslog.LOG_ERR, UPDATE_FAIL_LOG_TEXT.format(componet, ret))

def _reboot_system():
    cmd = "sudo reboot"
    try:
        syslog.syslog(syslog.LOG_NOTICE, cmd)
        ret, output = get_command_status_output(cmd)
        syslog.syslog(syslog.LOG_NOTICE, output)
    except Exception:
        return False
    return True

def wall_log(log):# Notify all users
    get_command_status_output("wall -t 10 -n '%s'" % (log))
    syslog.syslog(syslog.LOG_NOTICE, log)

LPC_TO_SPI = 1
PCIE_TO_SPI = 2
def _fpga_spi_to(channel):
    cmd = "busybox devmem 0xfc801012 8"
    ret, output = get_command_status_output(cmd)
    reg_val = int(output, 16)
    if channel == LPC_TO_SPI:
        reg_val = reg_val | 0x08
    elif channel == PCIE_TO_SPI:
        reg_val = (reg_val & (~0x08)) & 0xff
    else:
        reg_val = (reg_val & (~0x08)) & 0xff
    cmd = "busybox devmem 0xfc801012 8 0x{:x}".format(reg_val)
    ret, output = get_command_status_output(cmd)

def _update_cpld_firmware(image_path):
    syslog.syslog(syslog.LOG_NOTICE, "sys cpld will update now!")
    cmd = "/lib/platform-config/current/onl/bin/{} -u MX7327 {} -c DO_REAL_TIME_ISP 1".format(cpld_upd_tool, image_path)
    try:
        syslog.syslog(syslog.LOG_NOTICE, cmd)
        wall_log("sys cpld upgrading...")
        ret, output = get_command_status_output(cmd)
        syslog.syslog(syslog.LOG_NOTICE, output)
        wall_log("sys cpld upgrade ends.ret:{}".format(ret))
        if(ret):
            return UPDATE_FAILED
        if not os.path.exists(sys_cpld_flag_path):
            os.mknod(sys_cpld_flag_path)
    except Exception as e:
        syslog.syslog(syslog.LOG_ERR, e)
        return UPDATE_FAILED
    return UPDATE_SUCCESS

def _update_port_cpld_firmware(image_path):
    syslog.syslog(syslog.LOG_NOTICE, "port cpld will update now!")
    cmd = "/lib/platform-config/current/onl/bin/{} -u MX7327 {} -c DO_REAL_TIME_ISP 1".format(cpld_upd_tool, image_path)
    try:
        syslog.syslog(syslog.LOG_NOTICE, cmd)
        wall_log("port cpld upgrading...")
        ret, output = get_command_status_output(cmd)
        syslog.syslog(syslog.LOG_NOTICE, output)
        wall_log("port cpld upgrade ends.ret:{}".format(ret))
        if(ret):
            return UPDATE_FAILED
        if not os.path.exists(port_cpld_flag_path):
            os.mknod(port_cpld_flag_path)
    except Exception as e:
        syslog.syslog(syslog.LOG_ERR, e)
        return UPDATE_FAILED
    return UPDATE_SUCCESS

def _update_fpga_firmware_lpc(image_path):
    syslog.syslog(syslog.LOG_NOTICE, "fpga will update now!")
    cmd = "/lib/platform-config/current/onl/bin/{} -W -F {} 0xfc800000".format(fpga_upd_tool_lpc, image_path)
    try:
        wall_log("fpga upgrading...")
        _fpga_spi_to(LPC_TO_SPI)
        ret, output = get_command_status_output("/lib/platform-config/current/onl/bin/{} -O 0xfc800000".format(fpga_upd_tool_lpc))
        ret, output = get_command_status_output("/lib/platform-config/current/onl/bin/{} -O 0xfc800000".format(fpga_upd_tool_lpc))
        syslog.syslog(syslog.LOG_NOTICE, output)
        if(ret):
            _fpga_spi_to(PCIE_TO_SPI)
            return UPDATE_FAILED
        ret, output = get_command_status_output("/lib/platform-config/current/onl/bin/{} -R -A 0 -L 64 0xfc800000".format(fpga_upd_tool_lpc))
        syslog.syslog(syslog.LOG_NOTICE, output)
        if(ret):
            _fpga_spi_to(PCIE_TO_SPI)
            return UPDATE_FAILED
        ret, output = get_command_status_output("/lib/platform-config/current/onl/bin/{} -E 0xfc800000".format(fpga_upd_tool_lpc))
        syslog.syslog(syslog.LOG_NOTICE, output)
        if(ret):
            _fpga_spi_to(PCIE_TO_SPI)
            return UPDATE_FAILED
        ret, output = get_command_status_output(cmd)
        syslog.syslog(syslog.LOG_NOTICE, output)
        wall_log("fpga upgrade ends.ret:{}".format(ret))
        if(ret):
            _fpga_spi_to(PCIE_TO_SPI)
            return UPDATE_FAILED
        _fpga_spi_to(PCIE_TO_SPI)
        if not os.path.exists(fpga_flag_path):
            os.mknod(fpga_flag_path)
    except Exception as e:
        syslog.syslog(syslog.LOG_ERR, e)
        return UPDATE_FAILED
    return UPDATE_SUCCESS

def _update_fpga_firmware(image_path):
    syslog.syslog(syslog.LOG_NOTICE, "fpga will update now!")
    try:
        wall_log("fpga upgrading...")
        _fpga_spi_to(PCIE_TO_SPI)
        ret, output = get_command_status_output("/lib/platform-config/current/onl/bin/{} -O".format(fpga_upd_tool_pcie))
        ret, output = get_command_status_output("/lib/platform-config/current/onl/bin/{} -O".format(fpga_upd_tool_pcie))
        syslog.syslog(syslog.LOG_NOTICE, output)
        ret, output = get_command_status_output("/lib/platform-config/current/onl/bin/{} -E".format(fpga_upd_tool_pcie))
        syslog.syslog(syslog.LOG_NOTICE, output)
        if(ret):
            return UPDATE_FAILED
        ret, output = get_command_status_output("/lib/platform-config/current/onl/bin/{} -W -F {}".format(fpga_upd_tool_pcie, image_path))
        syslog.syslog(syslog.LOG_NOTICE, output)
        wall_log("fpga upgrade ends.")
        if(ret):
            return UPDATE_FAILED
        if not os.path.exists(fpga_flag_path):
            os.mknod(fpga_flag_path)
    except Exception as e:
        syslog.syslog(syslog.LOG_ERR, e)
        return UPDATE_FAILED
    return UPDATE_SUCCESS

def _update_bios_firmware(image_path):
    syslog.syslog(syslog.LOG_NOTICE, "bios will update now!")
    cmd = "/lib/platform-config/current/onl/bin/{} -p internal -i bios -w {} --ifd -N".format(bios_upd_tool, image_path)
    try:
        syslog.syslog(syslog.LOG_NOTICE, cmd)
        wall_log("bios upgrading...")
        ret, output = get_command_status_output("EC_WDT_SPI spi 0x02")
        syslog.syslog(syslog.LOG_NOTICE, output)
        ret, output = get_command_status_output(cmd)
        syslog.syslog(syslog.LOG_NOTICE, output)
        wall_log("bios upgrade ends.")
        if(ret):
            return UPDATE_FAILED
    except Exception:
        return UPDATE_FAILED
    return UPDATE_SUCCESS

def __get_bios_version():
    # Retrieves the BIOS firmware version
    try:
        bios_path = BIOS_VERSION_PATH
        ret, bios_version_raw = get_command_status_output("cat "+bios_path)
        bios_version = "{}".format(bios_version_raw)
    except Exception as e:
        syslog.syslog(syslog.LOG_NOTICE, 'Get exception when read bios version')
        bios_version = 'None'

    return bios_version

def __get_cpld_version():
    # Retrieves the CPLD firmware version
    try:
        cpld_path = "{}{}{}{}".format(BASE_PATH, CPLD_PATH, 1, '/hw_version')
        ret, cpld_version_raw = get_command_status_output("cat "+cpld_path)
        cpld_version = "{}".format(cpld_version_raw )
    except Exception as e:
        syslog.syslog(syslog.LOG_NOTICE, 'Get exception when read sys cpld')
        cpld_version = 'None'

    return cpld_version

def __get_port_cpld_version():
    # Retrieves the CPLD firmware version
    try:
        cpld_path = "{}{}{}{}".format(BASE_PATH, CPLD_PATH, 2, '/hw_version')
        ret, cpld_version_raw = get_command_status_output("cat "+cpld_path)
        cpld_version = "{}".format(cpld_version_raw)
    except Exception as e:
        syslog.syslog(syslog.LOG_NOTICE, 'Get exception when read port cpld')
        cpld_version = 'None'

    return cpld_version

def __get_fpga_version():
    # Retrieves the FPGA firmware version
    try:
        fpga_path = "{}{}{}{}".format(BASE_PATH, FPGA_PATH, 1 ,'/hw_version')
        ret, fpga_version_raw = get_command_status_output("cat "+fpga_path)
        fpga_version = "{}".format(fpga_version_raw)
    except Exception as e:
        syslog.syslog(syslog.LOG_NOTICE, 'Get exception when read fpga')
        fpga_version = 'None'

    return fpga_version

def __get_fpga_status():
    # Retrieves the FPGA firmware version
    try:
        retry_time = 3
        while retry_time:
            fpga_path = "{}{}{}{}".format(BASE_PATH, FPGA_PATH, 1 ,'/selftest/status')
            ret, fpga_version_raw = get_command_status_output("cat "+fpga_path)
            if(ret):
                syslog.syslog(syslog.LOG_NOTICE, fpga_path, fpga_version_raw)
                retry_time -= 1
                continue
            if(int(fpga_version_raw.split()[0], 16) != 0):
                syslog.syslog(syslog.LOG_NOTICE, fpga_path, fpga_version_raw)
                retry_time -= 1
                continue
            else:
                return True
    except Exception as e:
        syslog.syslog(syslog.LOG_NOTICE, 'Get fpga status err:{}'.format(e))
        return False

    return False

def _check_cpld_fw(file_cpld_ver):
    cpld_ver = __get_cpld_version()
    syslog.syslog(syslog.LOG_NOTICE, "sys cpld ver:%s   file ver:%s" % (cpld_ver, file_cpld_ver))
    if(file_cpld_ver == cpld_ver):
        syslog.syslog(syslog.LOG_NOTICE, "sys cpld ver is match!")
        return NO_NEED_UPDATE
    else:
        syslog.syslog(syslog.LOG_NOTICE, "sys cpld ver is not match!")
        return NEED_UPDATE

def _check_port_cpld_fw(file_port_cpld_ver):
    port_cpld_ver = __get_port_cpld_version()
    syslog.syslog(syslog.LOG_NOTICE, "port cpld ver:%s   file ver:%s" % (port_cpld_ver, file_port_cpld_ver))
    if(file_port_cpld_ver == port_cpld_ver):
        syslog.syslog(syslog.LOG_NOTICE, "port cpld ver is match!")
        return NO_NEED_UPDATE
    else:
        syslog.syslog(syslog.LOG_NOTICE, "port cpld ver is not match!")
        return NEED_UPDATE

def _check_fpga_fw(file_fpga_ver):
    fpga_ver = __get_fpga_version()
    syslog.syslog(syslog.LOG_NOTICE, "fpga ver:%s   file ver:%s" % (fpga_ver, file_fpga_ver))
    if(file_fpga_ver == fpga_ver):
        syslog.syslog(syslog.LOG_NOTICE, "fpga ver is match!")
        return NO_NEED_UPDATE
    else:
        syslog.syslog(syslog.LOG_NOTICE, "fpga ver is not match!")
        return NEED_UPDATE

def _check_fpga_status():
    fpga_ver = __get_fpga_version()
    syslog.syslog(syslog.LOG_NOTICE, "fpga ver:%s" % (fpga_ver))
    fpga_status = __get_fpga_status()

    if(fpga_ver.strip() == "ff.ff") or fpga_status == False:
        syslog.syslog(syslog.LOG_NOTICE, "FPGA firmware is missing.fpga_ver:{} fpga_status:{}".format(fpga_ver, fpga_status))
        return NEED_UPDATE
    else:
        syslog.syslog(syslog.LOG_NOTICE, "FPGA status is ok.fpga_ver:{} fpga_status:{}".format(fpga_ver, fpga_status))
        return NO_NEED_UPDATE

def _check_bios_fw(file_bios_ver):
    bios_ver = __get_bios_version()
    syslog.syslog(syslog.LOG_NOTICE, "bios ver:%s   file ver:%s" % (bios_ver, file_bios_ver))
    if(file_bios_ver == bios_ver):
        syslog.syslog(syslog.LOG_NOTICE, "bios ver is match!")
        return NO_NEED_UPDATE
    else:
        syslog.syslog(syslog.LOG_NOTICE, "bios ver is not match!")
        return NEED_UPDATE

def get_dev_label_revision():
    label_revision = ""
    retry_time = 60
    while retry_time:
        ret, label_revision = get_command_status_output("cat /sys/switch/syseeprom/label_revision")
        if ret != 0 or len(label_revision)<1:
            retry_time -= 1
            time.sleep(2)
            syslog.syslog(syslog.LOG_NOTICE, "get label_revision failed, retry:'cat /sys/switch/syseeprom/label_revision'")
            continue
        else:
            break
    syslog.syslog(syslog.LOG_NOTICE, "get label_revision ok, 'cat /sys/switch/syseeprom/label_revision'")
    return label_revision.strip()

def get_tar_fw_revision(sys_cpld_ver):
    fw_revision = "R0"
    try:
        fw_revision = fw_revision + sys_cpld_ver.split('.')[0]
    except:
        return None
    return fw_revision


def main():

    # if(len(sys.argv) <= 1) or (len(sys.argv) > 4):
    #     print_help()
    #     return

    try:
        json_file = open(json_file_path, "r")
        dictData = json.load(json_file)
    except:
        syslog.syslog(syslog.LOG_NOTICE, json_file_path, 'not found')
        sys.exit(1)

    # Iterate over key / value pairs of parent dictionary
    for chassis_key, chassis_value in dictData.items():
        # Again iterate over the nested dictionary
        for hwsku_key, hwsku_value in chassis_value.items():
            for component_key, component_value in hwsku_value.items():
                for components_key, components_value in component_value.items():
                    if(components_key == "SYS_CPLD"):
                        sys_cpld_ver = components_value["version"]
                        sys_cpld_fw = components_value["firmware"]
                    elif(components_key == "PORT_CPLD"):
                        port_cpld_ver = components_value["version"]
                        port_cpld_fw = components_value["firmware"]
                    elif(components_key == "FPGA"):
                        fpga_ver = components_value["version"]
                        fpga_fw = components_value["firmware"]
                    elif(components_key == "BIOS"):
                        bios_ver = components_value["version"]
                        bios_fw = components_value["firmware"]

    dev_label_revision = get_dev_label_revision()
    tar_fw_revision = get_tar_fw_revision(sys_cpld_ver)
    syslog.syslog(syslog.LOG_NOTICE, "The current device hardware version:{}".format(dev_label_revision))
    syslog.syslog(syslog.LOG_NOTICE, "Upgrade the target firmware version:{}".format(tar_fw_revision))
    if(dev_label_revision == tar_fw_revision) and (dev_label_revision != None):
        syslog.syslog(syslog.LOG_NOTICE, "The version matches and can be upgraded")
    else:
        syslog.syslog(syslog.LOG_NOTICE, "Version mismatches cannot be upgraded,exit")
        return UPDATE_TAR_NOT_MATCH

    # check fpga status
    fpga_status = _check_fpga_status()
    if fpga_status == NEED_UPDATE:
        # use lpc update fpga
        update_status = _update_fpga_firmware_lpc(fpga_fw)
        if(update_status == UPDATE_SUCCESS):
            # reboot
            syslog.syslog(syslog.LOG_NOTICE, "fpga upgrade fixes, please reboot later.")
            return UPDATE_SUCCESS
        else:
            syslog.syslog(syslog.LOG_NOTICE, "fpga upgrade fixes failed!!!")
            update_fail_log("FPGA", update_status)
            return UPDATE_FAILED
    else:
        pass

    syslog.syslog(syslog.LOG_NOTICE, 'Waiting for system finish init...')
    while not os.path.exists("/run/sonic_init_driver_done.flag"):
        time.sleep(2)
    syslog.syslog(syslog.LOG_NOTICE, 'Waiting for system finish init...done.')

    has_updated = False
    if(_check_cpld_fw(sys_cpld_ver) == NEED_UPDATE):
        update_status = _update_cpld_firmware(sys_cpld_fw)
        if(update_status == UPDATE_SUCCESS):
            has_updated = True
            syslog.syslog(syslog.LOG_NOTICE, "sys cpld update success!!!")
        else:
            syslog.syslog(syslog.LOG_ERR, "sys cpld update failed!!!")
            update_fail_log("CPLD1", update_status)
            return UPDATE_FAILED

    if(_check_port_cpld_fw(port_cpld_ver) == NEED_UPDATE):
        update_status = _update_port_cpld_firmware(port_cpld_fw)
        if(update_status == UPDATE_SUCCESS):
            has_updated = True
            syslog.syslog(syslog.LOG_NOTICE, "port cpld update success!!!")
        else:
            syslog.syslog(syslog.LOG_ERR, "port cpld update failed!!!")
            update_fail_log("CPLD2", update_status)
            return UPDATE_FAILED

    if(_check_fpga_fw(fpga_ver) == NEED_UPDATE):
        update_status = _update_fpga_firmware(fpga_fw)
        if(update_status == UPDATE_SUCCESS):
            has_updated = True
            syslog.syslog(syslog.LOG_NOTICE, "fpga update success!!!")
        else:
            syslog.syslog(syslog.LOG_ERR, "fpga update failed!!!")
            update_fail_log("FPGA", update_status)
            return UPDATE_FAILED

    if(_check_bios_fw(bios_ver) == NEED_UPDATE):
        update_status = _update_bios_firmware(bios_fw)
        if(update_status == UPDATE_SUCCESS):
            has_updated = True
            syslog.syslog(syslog.LOG_NOTICE, "bios update success!!!")
        else:
            syslog.syslog(syslog.LOG_ERR, "bios update failed!!!")
            update_fail_log("BIOS", update_status)
            return UPDATE_FAILED

    if(has_updated == True):
        syslog.syslog(syslog.LOG_NOTICE, "All firmware update success, please reboot later.")
        return UPDATE_SUCCESS
    #     try:
    #         if os.path.exists("/tmp/pending_config_migration"):
    #             ret, output = get_command_status_output("mv -f /etc/sonic/old_config /host/")
    #             ret, output = get_command_status_output("sync;sync;sync")
    #     except Exception:
    #         syslog.syslog(syslog.LOG_NOTICE, "mv -f /etc/sonic/old_config /host/ failed!")
    #     _reboot_system()

    return UPDATE_TO_DATE

if __name__ == "__main__":
    ret = main()
    exit(ret)
