#include <linux/kernel.h>

#define PSU_G1156_NAME    "psu_g1156"
#define PSU_G1156_NAME_1  "psu_g1156_0"
#define PSU_G1156_NAME_2  "psu_g1156_1"

int psu_pmbus_read_data(int psu_index, u8 pmbus_command, long *val);
int psu_pmbus_read_char(int psu_index, u8 pmbus_command, char *buf);
