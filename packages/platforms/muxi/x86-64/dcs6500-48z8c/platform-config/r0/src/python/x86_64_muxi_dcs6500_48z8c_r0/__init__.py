from onl.platform.base import *
from onl.platform.muxi import *
import os
import time
class OnlPlatform_x86_64_muxi_dcs6500_48z8c_r0(OnlPlatformMuxi,
                                              OnlPlatformPortConfig_48x25_8x100):

    PLATFORM='x86-64-muxi-dcs6500-48z8c-r0'
    MODEL="DCS6500-48Z8C"
    SYS_OBJECT_ID=".6500.56"

    def add_path(self, bin_path):
        path = os.environ['PATH']
        path_exist = False
        for sub_path in path.split(":"):
            if bin_path == sub_path:
                path_exist = True

        if(not path_exist):
            new_path = bin_path + ":" + path
            os.environ['PATH'] = new_path

    def baseconfig(self):
        DRIVER_INIT_DONE_FLAG = "/run/sonic_init_driver_done.flag"
        bin_path = "/lib/platform-config/current/onl/bin"
        self.add_path(bin_path)
        sbin_path = "/sbin"
        self.add_path(sbin_path)
        print("PATH={}".format(os.environ['PATH']))

        time.sleep(5)
        if os.path.exists(DRIVER_INIT_DONE_FLAG):
            os.remove(DRIVER_INIT_DONE_FLAG)
        os.system("sudo /usr/bin/python -u {}/muxi_util.py -d -f install".format(bin_path))
        # fw
        # os.system("/usr/bin/python -u /usr/local/bin/muxi_fw_monitor.py".format(bin_path))
        os.system("sudo /usr/bin/python -u {}/muxi_fan_monitor.py &".format(bin_path))
        os.system("sudo /usr/bin/python -u {}/muxi_psu_monitor.py &".format(bin_path))
        os.system("sudo /usr/bin/python -u {}/muxi_monitor_led.py &".format(bin_path))
        os.system("sudo /usr/bin/python -u {}/muxi_set_bmc_time.py &".format(bin_path))
        os.system("sudo /usr/bin/python -u {}/bmc_log_watchdog.py &".format(bin_path))

        time.sleep(5)
        return True