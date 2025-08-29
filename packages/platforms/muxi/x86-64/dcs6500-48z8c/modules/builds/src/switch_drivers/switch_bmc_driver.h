#ifndef SWITCH_BMC_DRIVER_H
#define SWITCH_BMC_DRIVER_H

#include "switch.h"

#define BMC_ERR(fmt, args...)      LOG_ERR("bmc: ", fmt, ##args)
#define BMC_WARNING(fmt, args...)  LOG_WARNING("bmc: ", fmt, ##args)
#define BMC_INFO(fmt, args...)     LOG_INFO("bmc: ", fmt, ##args)
#define BMC_DEBUG(fmt, args...)    LOG_DBG("bmc: ", fmt, ##args)

#define CHECK_BIT(a, b)  (((a) >> (b)) & 1U)

#define SYS_CPLD_BMC_STATUS_REG     0x80
#define BMC_PRESENT_OFFSET      0x7
#define BMC_PRESENT_MASK        0x1
#define BMC_HEART_OFFSET        0x0
#define BMC_HEART_MASK          0x7

#define BMC_HEART_FRQ_10HZ  0
#define BMC_HEART_FRQ_2HZ   1
#define BMC_HEART_FRQ_05HZ  2
#define BMC_HEART_FRQ_01HZ  3
#define BMC_HEART_FRQ_0HZ   4

#define BMC_PRESENT         0
#define BMC_NOT_PRESENT     1


#define SYS_CPLD_BMC_EN_REG         0x82
#define BMC_ENABLE_OFFSET           0x0
#define BMC_ENABLE_MASK             0x1

#define BMC_EN_ENABLE   1
#define BMC_EN_DISABLE  0

#define S3IP_BMC_STATUS_ABSENT      0
#define S3IP_BMC_STATUS_OK          1
#define S3IP_BMC_STATUS_ERR         2
// #define SYS_CPLD_REG_BMC_HB_STATE_OFFSET    0x1f10

struct bmc_drivers_t{
    ssize_t (*get_status) (char* buf);
    void (*get_loglevel) (long *lev);
    void (*set_loglevel) (long lev);
    ssize_t (*get_enable)(char *buf);
    void (*set_enable)(long enable);
};

// ssize_t drv_get_status(char* buf);
// void drv_get_loglevel(long *lev);
// void drv_set_loglevel(long lev);
// ssize_t drv_get_enable(char *buf);
// void drv_set_enable(long enable);

void mxsonic_bmc_drivers_register(struct bmc_drivers_t *p_func);
void mxsonic_bmc_drivers_unregister(void);

#endif /* SWITCH_BMC_DRIVER_H */
