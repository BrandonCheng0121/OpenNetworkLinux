#ifndef FMEA_FORMAT_H
#define FMEA_FORMAT_H
#include "libboundscheck/include/securec.h"
#define MAX_DETIAL_LEN 1024

//status bit
typedef struct ott_status_bit_fmea
{
    unsigned int bit;//status bit
    const char *detail;//detial
}OttStatusBitFmea;

#define PSU_STATUS_DESCRIPTION "" //"PSU working state"
OttStatusBitFmea psu_status_tbl[] = {
    //bit   detial
    {BIT(0), "No input"},
    {BIT(1), "Overtemperature"},
    {BIT(2), "Fan abnormal"},
    {BIT(3), "Voltage and current abnormalities"},
    {BIT(4), "I2c fault"},
    {BIT(5), "No output"},
};

#define PLL_STATUS_DESCRIPTION "" //"PLL working state"
OttStatusBitFmea pll_status_tbl[] = {
    //bit   detial
    {BIT(0), "xtal_48M los or err"},
    {BIT(1), "id or bus error"},
    {BIT(2), "IN0-3 oof"},
    {BIT(3), "IN0-3 los"},
    {BIT(4), "DSPLL error"},
};

#define AVS_STATUS_DESCRIPTION "" //"AVS working state"
//mp2976 mp2978 mp2882 mp2971 mp2940B mp5023
OttStatusBitFmea avs_power_status_tbl[] = {
    //bit   detial
    {BIT(0), "Output Overvoltage"},
    {BIT(1), "Output Undervoltage"},
    {BIT(2), "Output Overcurrent"},
    {BIT(3), "Input Overvoltage"},
    {BIT(4), "Input Undervoltage"},
    {BIT(5), "Input Overpower"},
    {BIT(6), "Overtemperature"},
    {BIT(7), "Phase loss"},
    {BIT(8), "Uneven current"},
    {BIT(15), "Failed to get power status"},
};

#define CPLD_PG_STATUS_DESCRIPTION "" //"CPLD PG state reg:0x72+0x73"
OttStatusBitFmea cpld_pg_status_tbl[] = {
    //bit   detial
    {BIT(0), "R_CPU_PGOOD err"},
    {BIT(1), "R_VCC3V3_PG err"},
    {BIT(2), "R_VCC3V3_OSFP_PG err"},
    {BIT(3), "R_VCC_FPGA_1V2_PG err"},
    {BIT(4), "R_VCC_FPGA_3V3_PG err"},
    {BIT(5), "R_VCC_FPGA_1V8_PG err"},
    {BIT(6), "R_VCC_FPGA_1V0_PG err"},
    {BIT(7), "R_VDD_PLL_PG err"},
    {BIT(8), "R_VDD_0V75_ANLG_PG err"},
    {BIT(9), "R_VDD_1V5_D5_PG err"},
    {BIT(10), "R_VDD_CORE_PG err"},
    {BIT(11), "R_VDD_1V2_MDIO_PG err"},
    {BIT(12), "R_VDD_1V8_DUT_PG err"},
    {BIT(13), "R_VDD3V3_PLL_PG err"},
    {BIT(14), "R_VCC12V_IN_PG err"},
};

int fmea_format_status_bit(int bit_val, OttStatusBitFmea *tbl, int tbl_size, char *description, char *buf)
{
    int i = 0;
    int count = 0;
    int len = 0;
    char tmp_buf[MAX_DETIAL_LEN] = {0};

    for(i = 0; i < tbl_size; i++)
    {
        if(tbl[i].bit & bit_val)
        {
            len = sprintf_s(tmp_buf+count, (MAX_DETIAL_LEN-count), "%s;", tbl[i].detail);
            count += len;
        }
    }
    if(len > 0)
        return sprintf_s(buf, PAGE_SIZE, "0x%x %sFaultType:%s\n", bit_val, description, tmp_buf);
    else
        return sprintf_s(buf, PAGE_SIZE, "0x%x %sFaultType:normal\n", bit_val, description);
}

//status value
typedef struct ott_status_val_fmea
{
    unsigned int sta;//status val
    const char *detail;//detial
}OttStatusValFmea;

#define TEMP_STATUS_DESCRIPTION "" //"Temp Overtemperature state"
OttStatusValFmea temp_status_tbl[] =
{
    //val   detial
    {0x0, "normal"},
    {0x1, "Overtemperature high"},
    {0x2, "Overtemperature critical"},
    {0xff, "Failed to get temperature or temperature invalid"},
};

#define FAN_STATUS_DESCRIPTION "" //"Fan rotate speed state"
OttStatusValFmea fan_status_tbl[] = {
    //val   detial
    {0x0, "normal"},
    {0x1, "The fan speed is out of tolerance"},
    {0x2, "The fan stalls"},
};

int fmea_format_status_val(int val, OttStatusValFmea *tbl, int tbl_size, char *description, char *buf)
{
    int i = 0;

    for(i = 0; i < tbl_size; i++)
    {
        if(tbl[i].sta == val)
        {
            return sprintf_s(buf, PAGE_SIZE, "0x%x %sFaultType:%s\n", val, description, tbl[i].detail);
        }
    }
    return sprintf_s(buf, PAGE_SIZE, "NA Unable to get status value\n");
}

#endif /* FMEA_FORMAT_H */
