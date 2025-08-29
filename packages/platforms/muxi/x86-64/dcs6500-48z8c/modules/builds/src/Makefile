ccflags-y := -DCUSTOMER=$(CUSTOMER)
ccflags-y += -D_FORTIFY_SOURCE=2 -O2 -Wl,-z,now
ccflags-y += -DLIBBOUNDSCHECK_KERNEL_MODULE=1
subdir-ccflags-y := -I$(src)
subdir-ccflags-y := -I$(src)/switch_drivers
subdir-ccflags-y += -I$(src)/third_party
subdir-ccflags-y += -I$(src)/third_party/libboundscheck/include

obj-m += libboundscheck.o
libboundscheck-objs += \
third_party/libboundscheck/src/sprintf_s.o            third_party/libboundscheck/src/vsprintf_s.o \
third_party/libboundscheck/src/sscanf_s.o             third_party/libboundscheck/src/vsscanf_s.o \
third_party/libboundscheck/src/strcat_s.o \
third_party/libboundscheck/src/memcpy_s.o             third_party/libboundscheck/src/strcpy_s.o \
third_party/libboundscheck/src/memmove_s.o            third_party/libboundscheck/src/strncat_s.o \
third_party/libboundscheck/src/memset_s.o             third_party/libboundscheck/src/strncpy_s.o \
third_party/libboundscheck/src/strtok_s.o \
third_party/libboundscheck/src/securecutil.o \
third_party/libboundscheck/src/secureinput_a.o \
third_party/libboundscheck/src/secureprintoutput_a.o \
third_party/libboundscheck/src/snprintf_s.o           third_party/libboundscheck/src/vsnprintf_s.o

CONFIG_SWITCH = m
obj-$(CONFIG_SWITCH) += switch_drivers/

CONFIG_THIRD_PARTY = m
obj-$(CONFIG_THIRD_PARTY) += third_party/

CONFIG_S3IP = m
obj-$(CONFIG_S3IP) += s3ip/

CONFIG_MX = m
obj-$(CONFIG_MX) += mx/
