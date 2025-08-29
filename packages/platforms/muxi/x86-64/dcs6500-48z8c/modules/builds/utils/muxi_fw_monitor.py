#!/usr/bin/env python

try:
    import os
    import getopt
    import sys
    import subprocess
    #import click
    import logging
    import logging.config
    import logging.handlers
    import types
    import time  # this is only being used as part of the example
    import syslog
except ImportError as e:
    raise ImportError('%s - required module not found' % str(e))

RET_TRUE  = 0
RET_FALSE = -1

EC_IO_PORT_MAPPING_BASE    = 0x6300

OEM_BIOS_SELECT_STATUS_REG = 0x20
OEM_BIOS_SELECT_REG        = 0x21

BIOS_SPI_MAIN_PLASH        = 0xBA
BIOS_SPI_BACKUP_FLASH      = 0xBB
# REBOOT_CAUSE_FILE = "/host/reboot-cause/reboot-cause.txt"

# Deafults
VERSION = '1.0'
FUNCTION_NAME = '/usr/local/bin/muxi_monitor_bios'
DEBUG = False

BIOS_VERSION_FLAG   = 'BIOS_FILE_VERSION'
BIOS_FILE_NAME_FLAG = 'BIOS_FILE_NAME'

#bios flag
g_bios_update_progress_flag = 0
#bios reboot flag
g_bios_reboot_flag = 0

def my_log(txt):
    if DEBUG == True:
        print("[ACCTON DBG]: "+txt)
    return

def log_os_system(cmd):
    logging.info('Run :'+cmd)
    status = 1
    output = ""
    if sys.version_info.major == 2:
        # python2
        import commands
        status, output = commands.getstatusoutput(cmd)
        status = status>>8
    else:
        # python3
        import subprocess
        status, output = subprocess.getstatusoutput(cmd)
    if status:
        logging.info('Failed :'+cmd)
    return  status, output


#all fw update
UPDATE_SUCCESS = 1
UPDATE_TO_DATE = 0
UPDATE_FAILED = -1
UPDATE_TAR_NOT_MATCH = 2
def all_fw_image_update():
    update_cmd = "python /usr/local/bin/update_component_all.py"
    os.system("wall -t 10 -n 'Do not reboot the firmware to upgrade automatically!'")
    ret = os.system(update_cmd)
    if sys.version_info.major == 2:
        return (ret>>8) # just for python2
    else:
        return (ret>>8)
######################BIOS########################################################################

#read ec reg
def ec_ram_register_read(addr_offset):
    reg_addr = EC_IO_PORT_MAPPING_BASE + addr_offset
    read_cmd = "inb 0x%x"
    read_cmd = read_cmd %(reg_addr)

    ret, output = log_os_system(read_cmd)
    if(RET_TRUE != ret):
        syslog.syslog(syslog.LOG_ERR, "ERROR: EC read addr(0x%x), ret(%d)." %(reg_addr, ret));
        return RET_FALSE

    return RET_TRUE, output

#bios update
def bios_image_update():
    update_cmd = "update_component.sh MAIN_BIOS update"

    ret, output = log_os_system(update_cmd)
    #update log
    syslog.syslog(syslog.LOG_NOTICE, output)
    if(RET_TRUE != ret):
        syslog.syslog(syslog.LOG_ERR, "ERROR: getstatusoutput(%s), ret(%d)." %(update_cmd, ret));
        return RET_FALSE

    return RET_TRUE

'''
OEM_BIOS_Select_Status  :EC_IO_port_mapping_base + 0x20
                        OEM_BIOS_Select_Status[7:4] : BIOS Select Status
                        OEM_BIOS_Select_Status[3:2] : Reserved
                        OEM_BIOS_Select_Status[1]   : COMe BIOS_DIS1 status
                        OEM_BIOS_Select_Status[0]   : COMe BIOS_DIS0 status
'''
#read oem bios select status
def oem_bios_select_status_get():
    ret, value = ec_ram_register_read(OEM_BIOS_SELECT_STATUS_REG)
    if(RET_TRUE != ret):
        syslog.syslog(syslog.LOG_ERR, "ERROR: ec_ram_register_read(0x%x), ret(%d)." %(OEM_BIOS_SELECT_STATUS_REG, ret));
        return RET_FALSE

    return RET_TRUE, value

def oem_bios_COMe_pin_flag():
    ret, value = ec_ram_register_read(OEM_BIOS_SELECT_REG)
    if(RET_TRUE != ret):
        syslog.syslog(syslog.LOG_ERR, "ERROR: ec_ram_register_read(0x%x), ret(%d)." %(OEM_BIOS_SELECT_STATUS_REG, ret));
        return RET_FALSE

    bios_select_flag = (int(value) & 0x10)

    return RET_TRUE, bios_select_flag

#bios startup flash
def bios_startup_flash_get():
    #1 check flag
    ret, value_flag = oem_bios_COMe_pin_flag()
    if(RET_TRUE != ret):
        syslog.syslog(syslog.LOG_ERR, "ERROR: oem_bios_select_status_get(), ret(%d)." %(ret));
        return RET_FALSE

    #2 read spi status
    ret, status_value = oem_bios_select_status_get()
    if(RET_TRUE != ret):
        syslog.syslog(syslog.LOG_ERR, "ERROR: oem_bios_select_status_get(), ret(%d)." %(ret));
        return RET_FALSE

    syslog.syslog(syslog.LOG_NOTICE, "COMe_pin_flag(0x%X), bios_select_status(0x%X)" %(value_flag, int(status_value)))

    #3 determine the bios startup status
    # Only concern the bit[0:1] since the bios selecting is controlled by CPLD on TH4/TD4-200G.
    bios_select_status = (int(status_value) & 0x3)

    if (0x3 == bios_select_status) :
        syslog.syslog(syslog.LOG_NOTICE, "Startup cpld-bios-backup-flash......")
        return RET_TRUE, BIOS_SPI_BACKUP_FLASH
    else :
        syslog.syslog(syslog.LOG_NOTICE, "Startup cpld-bios-main-flash......")
        return RET_TRUE, BIOS_SPI_MAIN_PLASH

#reboot
def bios_system_reboot():
    UPDATE_OK_LOG_TEXT = 'MAIN BIOS recovery done, reboot in 5 seconds.'
    syslog.syslog(syslog.LOG_WARNING, UPDATE_OK_LOG_TEXT)
    log_os_system("wall %s" % (UPDATE_OK_LOG_TEXT))

    reboot_cmd = "sleep 5; sudo reboot"
    ret, output = log_os_system(reboot_cmd)
    if(RET_TRUE != ret):
        syslog.syslog(syslog.LOG_ERR, "ERROR: getstatusoutput(%s), ret(%d)." %(reboot_cmd, ret));
        return RET_FALSE

    return RET_TRUE

def fw_system_reboot():
    UPDATE_OK_LOG_TEXT = 'All firmware update success, reboot after 5 seconds.'
    syslog.syslog(syslog.LOG_WARNING, UPDATE_OK_LOG_TEXT)
    os.system("wall -t 10 -n %s" % (UPDATE_OK_LOG_TEXT))

    reboot_cmd = "sleep 5;sync;sudo reboot"
    ret, output = log_os_system(reboot_cmd)
    if(RET_TRUE != ret):
        syslog.syslog(syslog.LOG_ERR, "ERROR: getstatusoutput(%s), ret(%d)." %(reboot_cmd, ret))
        return RET_FALSE

    return RET_TRUE

#record software reboot to file
# def record_software_reboot_to_file(reboot_reason):
#     get_time_cmd = "date"
#     status, output = log_os_system(get_time_cmd)
#     if(status):
#         syslog.syslog(syslog.LOG_ERR, "Failed to get time, detail [%s]" % (output))
#     software_reboot_gen_time = str(output)

#     record_cmd = "echo Software reboot [Time: {:s}, Reason: {:s}] > {:s}".format(software_reboot_gen_time, reboot_reason, REBOOT_CAUSE_FILE)
#     status, output = log_os_system(record_cmd)
#     if(status):
#         syslog.syslog(syslog.LOG_ERR, "Failed to record software reboot to file, detail [%s]" % (output))
#     log_os_system("sync")
#     time.sleep(0.1)

class device_monitor(object):

    def __init__(self, log_file, log_level):
        """Needs a logger and a logger level."""
        # set up logging to file
        logging.basicConfig(
            filename=log_file,
            filemode='w',
            level=log_level,
            format= '[%(asctime)s] {%(pathname)s:%(lineno)d} %(levelname)s - %(message)s',
            datefmt='%H:%M:%S'
        )

        # set up logging to console
        if log_level == logging.DEBUG:
            console = logging.StreamHandler()
            console.setLevel(log_level)
            formatter = logging.Formatter('%(name)-12s: %(levelname)-8s %(message)s')
            console.setFormatter(formatter)
            logging.getLogger('').addHandler(console)

        sys_handler = handler = logging.handlers.SysLogHandler(address = '/dev/log')
        sys_handler.setLevel(logging.WARNING)
        logging.getLogger('').addHandler(sys_handler)

        #logging.debug('SET. logfile:%s / loglevel:%d', log_file, log_level)

    def bios_update_progress(self):
        global g_bios_update_progress_flag
        global g_bios_reboot_flag

        g_bios_update_progress_flag = 0x0
        g_bios_reboot_flag          = 0x0

        #1 get startup flash
        ret, bios_startup_flash = bios_startup_flash_get()
        if(RET_TRUE != ret):
            syslog.syslog(syslog.LOG_ERR, "ERROR: bios_startup_flash_get(), ret(%d)." %(ret))
            return False

        #2 update bios for main flash if startup flash is backup
        if (BIOS_SPI_BACKUP_FLASH == bios_startup_flash):
            ret = bios_image_update()
            if(RET_TRUE != ret):
                syslog.syslog(syslog.LOG_ERR, "ERROR: bios_image_update(), ret(%d)." %(ret))
                return False
            else:
                syslog.syslog(syslog.LOG_NOTICE, 'BIOS update successfully!')
                g_bios_reboot_flag = 0x1

        #Done
        g_bios_update_progress_flag = 0x1

        return True

# CFG.TCK_SETUP=0
# CFG.TCK_HOLD=0
# Detect 2 devices
# #0 MachXO3LF-6900C (0x612bd043)
# #1 MachXO3LF-2100C-BG324 (0xe12bc043)
def cpld_jtag_check():
    cmd = 'cpldupd_sec -s MX7327'
    status, output = log_os_system(cmd)
    if status:
        syslog.syslog(syslog.LOG_ERR, "error:CPLD JTAG :{}".format(output))
        return
    if ("MachXO3LF-6900C" not in output) or ("MachXO3LF-2100C-BG324" not in output):
        syslog.syslog(syslog.LOG_ERR, "error:CPLD JTAG :{}".format(output))
        return
    syslog.syslog(syslog.LOG_NOTICE, "CPLD JTAG check OK")

def main(argv):
    global g_bios_update_progress_flag

    log_file  = '%s.log' % FUNCTION_NAME
    log_level = logging.INFO

    if len(sys.argv) != 1:
        try:
            opts, args = getopt.getopt(argv,'hdlr',['lfile='])
        except getopt.GetoptError:
            print('A:Usage: %s [-d] [-l <log_file>]' % sys.argv[0])
            return 0

        for opt, arg in opts:
            if opt == '-h':
                print('B:Usage: %s [-d] [-l <log_file>]' % sys.argv[0])
                return 0
            elif opt in ('-d', '--debug'):
                log_level = logging.DEBUG
            elif opt in ('-l', '--lfile'):
                log_file = arg
    # move to update_component_all.py
    print('Waiting for system finish init...')
    init_driver_timeout = 15
    while not os.path.exists("/run/sonic_init_driver_done.flag"):
        time.sleep(2)
        init_driver_timeout -= 1
        if init_driver_timeout <= 0:
            print("wait driver init timeout")
            break
    print('Waiting for system finish init...done.')

    cpld_jtag_check()
    time.sleep(1)
    monitor = device_monitor(log_file, log_level)

    UPDATE_FAIL_LOG_TEXT = 'AutoUpgradeFirmwareFail: The firmware upgrade failed(Name={}, ErrorCode={}).'

    for retry_num in range(0,3):
        #1 bios progress
        if (0x0 == g_bios_update_progress_flag) :
            ret = monitor.bios_update_progress()
            if ret != True:
                syslog.syslog(syslog.LOG_ERR, UPDATE_FAIL_LOG_TEXT.format('BIOS', 1))

    syslog.syslog(syslog.LOG_NOTICE, "bios_update_flag(0x%x)" %(g_bios_update_progress_flag))

    if(0x1 == g_bios_reboot_flag):
        # record_software_reboot_to_file("Firmware upgrade: device boot with MAIN_BIOS restoration upgrade")
        syslog.syslog(syslog.LOG_NOTICE, "reboot.....")
        time.sleep(1)
        ret = bios_system_reboot()
        if(RET_TRUE != ret):
            syslog.syslog(syslog.LOG_ERR, "ERROR: bios_system_reboot(), ret(%d)." %(ret))
            return False
        return True

    syslog.syslog(syslog.LOG_NOTICE, "INFO: Check the firmware version and upgrade...")
    fw_update = all_fw_image_update()
    if(fw_update == UPDATE_SUCCESS):
        syslog.syslog(syslog.LOG_NOTICE, "INFO: All firmware update success, fw_update(%d)." %(fw_update))
        syslog.syslog(syslog.LOG_NOTICE, "reboot.....")
        time.sleep(1)
        ret = fw_system_reboot()
        if(RET_TRUE != ret):
            syslog.syslog(syslog.LOG_ERR, "ERROR: bios_system_reboot(), ret(%d)." %(ret))
            return False
        return True
    elif(fw_update == UPDATE_TO_DATE):
        syslog.syslog(syslog.LOG_NOTICE, "INFO: The firmware version is up to date, fw_update(%d)." %(fw_update))
        os.system("wall -t 10 -n 'The firmware version is up to date'")
        return True
    elif(fw_update == UPDATE_TAR_NOT_MATCH):
        syslog.syslog(syslog.LOG_NOTICE, "INFO: Version mismatches cannot be upgraded,exit, fw_update(%d)." %(fw_update))
        os.system("wall -t 10 -n 'Version mismatches cannot be upgraded'")
        return True
    else:
        syslog.syslog(syslog.LOG_ERR, "ERROR: Firmware upgrade failed, fw_update(%d)." %(fw_update))
        os.system("wall -t 10 -n 'Firmware upgrade failed.Please check the device status.'")
        syslog.syslog(syslog.LOG_ERR, UPDATE_FAIL_LOG_TEXT.format('FW', 1))
        return False

if __name__ == "__main__":
    main(sys.argv)
