# Get I210 memorybase
pci_slot_str=$(lspci | grep I210 | cut -d " " -f 1)
bar_str=0x$(lspci -s $pci_slot_str -vv | grep "Region 0:" | cut -d " " -f 5)

# Get MDI CONTROL REG offset
printf -v mdi_ctl_reg '%#x' "$(($bar_str + 0x20))"
# echo $mdi_ctl_reg

#Write page to 0, and read reg PHY Identifier 1 - Page 0, Register 2, should be 0x0141
busybox devmem $mdi_ctl_reg 32 0x4160000
busybox devmem $mdi_ctl_reg 32 0x8020000
reg_val1=$(busybox devmem $mdi_ctl_reg 16)


#Write page to 0, and read reg PHY Identifier 2 - Page 0, Register 3, should be 0x0C00
busybox devmem $mdi_ctl_reg 32 0x4160000
busybox devmem $mdi_ctl_reg 32 0x8030000
reg_val2=$(busybox devmem $mdi_ctl_reg 16)

if [ "$reg_val1" != "0x0141" ];then
    echo "error: i210 phyid check error reg_val1=$reg_val1"
    exit -1
fi

if [ "$reg_val2" != "0x0C00" ];then
    echo "error: i210 phyid check error reg_val2=$reg_val2"
    exit -1
fi
echo "OK: i210 phyid check OK"