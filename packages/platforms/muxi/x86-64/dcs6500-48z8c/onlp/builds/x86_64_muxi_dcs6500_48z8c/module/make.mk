###############################################################################
#
# 
#
###############################################################################
THIS_DIR := $(dir $(lastword $(MAKEFILE_LIST)))
x86_64_muxi_dcs6500_48z8c_INCLUDES := -I $(THIS_DIR)inc
x86_64_muxi_dcs6500_48z8c_INTERNAL_INCLUDES := -I $(THIS_DIR)src
x86_64_muxi_dcs6500_48z8c_DEPENDMODULE_ENTRIES := init:x86_64_muxi_dcs6500_48z8c ucli:x86_64_muxi_dcs6500_48z8c

