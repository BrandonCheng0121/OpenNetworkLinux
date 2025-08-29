#ifndef SWITCH_CPU_DRIVER_H
#define SWITCH_CPU_DRIVER_H

#include "switch.h"

#define CPU_ERR(fmt, args...)      LOG_ERR("cpu: ", fmt, ##args)
#define CPU_WARNING(fmt, args...)  LOG_WARNING("cpu: ", fmt, ##args)
#define CPU_INFO(fmt, args...)     LOG_INFO("cpu: ", fmt, ##args)
#define CPU_DEBUG(fmt, args...)    LOG_DBG("cpu: ", fmt, ##args)

#define DEBUG_STRING_LEN            128

#define CHECK_BIT(a, b)  (((a) >> (b)) & 1U)

#define PCI_INTEL_VENDOR_ID         0x8086

// Bus: 1, Device: 16, Function: 5 (Scratch)
#define PCI_UBOX_DEVICE_ID          0x6f1e
#define UBOX_UBOXERRSTS_OFFSET      0x64

// Bus: 0, Device: 0, Function: 0 (DMI2)
#define PCI_IIO0_DEVICE_ID          0x6f00
#define IIO0_PCI_OFFSET             0x06
#define IIO0_DEV_OFFSET             0xf2
#define IIO0_UNC_ERR_OFFSET         0x14c
#define IIO0_COR_ERR_OFFSET         0x158
#define IIO0_RP_ERR_OFFSET          0x178
#define IIO0_XPGLB_ERR_OFFSET       0x230

// Bus: 0, Device: 1-3, Function: 0-1, 0/2, 0-3
#define PCI_IIO1_DEVICE_ID          0x6f04
#define IIO1_PCI_OFFSET             0x06
#define IIO1_SEC_OFFSET             0x1e
#define IIO1_DEV_OFFSET             0x9a
#define IIO1_UNC_ERR_OFFSET         0x14c
#define IIO1_COR_ERR_OFFSET         0x158
#define IIO1_RP_ERR_OFFSET          0x178
#define IIO1_XPGLB_ERR_OFFSET       0x230

// Bus: 0, Device: 3, Function: 0 (IOU1 as NTB)
#define PCI_IIO2_DEVICE_ID          0x6f08
#define IIO2_PCI_OFFSET             0x06
#define IIO2_DEV_OFFSET             0x9a
#define IIO2_UNC_ERR_OFFSET         0x14c
#define IIO2_COR_ERR_OFFSET         0x158
#define IIO2_RP_ERR_OFFSET          0x178
#define IIO2_XPGLB_ERR_OFFSET       0x230
#define IIO2_LN_ERR_OFFSET          0x258

// Bus: 0, Device: 5, Function: 0 (Virtualization)
#define PCI_IIO3_DEVICE_ID          0x6f28
#define IIO3_PCI_OFFSET             0x06
#define IIO3_VTUNC_ERR_OFFSET       0x1a8

// Bus: 0, Device: 5, Function: 2 (Errors)
#define PCI_IIO4_DEVICE_ID          0x6f2a
#define IIO4_PCI_OFFSET             0x06
#define IIO4_ERRPIN_OFFSET          0xa8
#define IIO4_GC_ERR_OFFSET          0x1a8
#define IIO4_GN_ERR_OFFSET          0x1c0
#define IIO4_GF_ERR_OFFSET          0x1c4
#define IIO4_IRPP0_ERR_OFFSET       0x230
#define IIO4_IRPP1_ERR_OFFSET       0x2b0
#define IIO4_IIO_ERR_OFFSET         0x300
#define IIO4_MI_ERR_OFFSET          0x380

#define MSR_THRESHOLD2_STATUS_BITSHIFT                          8
#define MSR_THRESHOLD1_STATUS_BITSHIFT                          6
#define MSR_CRITICAL_TEMP_STATUS_BITSHIFT                       4
#define MSR_PROCHOT_STATUS_BITSHIFT                             2
#define MSR_THERMAL_STATUS_BITSHIFT                             0

#define UBOX_SMI_DELIVERY_VALID_BITSHIFT                        16
#define UBOX_MASTER_LOCK_TIMEOUT_BITSHIFT                       7
#define UBOX_SMI_TIMEOUT_BITSHIFT                               6
#define UBOX_CFG_WR_ADDR_MIS_ALIGNED_BITSHIFT                   5
#define UBOX_CFG_RD_ADDR_MIS_ALIGNED_BITSHIFT                   4
#define UBOX_UNSURPPORTED_OPCODE_BITSHIFT                       3
#define UBOX_POISON_RSVD_BITSHIFT                               2
#define UBOX_SMI_SRC_IMC_BITSHIFT                               1
#define UBOX_SMI_SRC_UMC_BITSHIFT                               0

// iio pcists, secsts
#define IIO_DPE_BITSHIFT                                        15
#define IIO_SSE_BITSHIFT                                        14
#define IIO_RMA_BITSHIFT                                        13
#define IIO_RTA_BITSHIFT                                        12
#define IIO_STA_BITSHIFT                                        11
#define IIO_MDPE_BITSHIFT                                       8

// iio devsts
#define IIO_UNSUPPORTED_REQUEST_DETECTED_BITSHIFT               3
#define IIO_FATAL_ERROR_DETECTED_BITSHIFT                       2
#define IIO_NON_FATAL_ERROR_DETECTED_BITSHIFT                   1
#define IIO_CORRECTABLE_ERROR_DETECTED_BITSHIFT                 0

// iio uncerrsts
#define IIO_ACS_VIOLATION_STATUS_BITSHIFT                       21
#define IIO_RECEIVE_AN_UNSUPPORTED_REQUEST_BITSHIFT             20
#define IIO_ECRC_ERROR_STATUS_BITSHIFT                          19
#define IIO_MALFORMED_TLP_STATUS_BITSHIFT                       18
#define IIO_RECEIVER_BUFFER_OVERFLOW_STATUS_BITSHIFT            17
#define IIO_UNEXPECTED_COMPLETION_STATUS_BITSHIFT               16
#define IIO_COMPLETER_ABORT_STATUS_BITSHIFT                     15
#define IIO_COMPLETION_TIME_OUT_STATUS_BITSHIFT                 14
#define IIO_FLOW_CONTROL_PROTOCOL_ERROR_STATUS_BITSHIFT         13
#define IIO_POISONED_TLP_STATUS_BITSHIFT                        12
#define IIO_SURPRISE_DOWN_ERROR_STATUS_BITSHIFT                 5
#define IIO_DATA_LINK_PROTOCOL_ERROR_STATUS_BITSHIFT            4

// iio corerrsts
#define IIO_ADVISORY_NON_FATAL_ERROR_STATUS_BITSHIFT            13
#define IIO_REPLAY_TIMER_TIME_OUT_STATUS_BITSHIFT               12
#define IIO_REPLAY_NUM_ROLLOVER_STATUS_BITSHIFT                 8
#define IIO_BAD_DLLP_STATUS_BITSHIFT                            7
#define IIO_BAD_TLP_STATUS_BITSHIFT                             6
#define IIO_RECEIVER_ERROR_STATUS_BITSHIFT                      0

// iio rperrsts
#define IIO_FATAL_ERROR_MESSAGES_RECEIVED_BITSHIFT              6
#define IIO_NON_FATAL_ERROR_MESSAGES_RECEIVED_BITSHIFT          5
#define IIO_FIRST_UNCORRECTABLE_FATAL_BITSHIFT                  4
#define IIO_MULTIPLE_ERROR_FATAL_NONFATAL_RECEIVED_BITSHIFT     3
#define IIO_ERROR_FATAL_NONFATAL_RECEIVED_BITSHIFT              2
#define IIO_MULTIPLE_CORRECTABLE_ERROR_RECEIVED_BITSHIFT        1
#define IIO_CORRECTABLE_ERROR_RECEIVED_BITSHIFT                 0

// iio xpglberrsts
#define IIO_PCIE_AER_CORRECTABLE_ERROR_BITSHIFT                 2
#define IIO_PCIE_AER_NON_FATAL_ERROR_BITSHIFT                   1
#define IIO_PCIE_AER_FATAL_ERROR_BITSHIFT                       0

// iio vtuncerrsts
#define IIO_PERR_TLB1_BITSHIFT                                  5
#define IIO_PERR_TLB0_BITSHIFT                                  4
#define IIO_PERR_L3_LOOKUP_BITSHIFT                             3
#define IIO_PERR_L1_LOOKUP_BITSHIFT                             2
#define IIO_PERR_L2_LOOKUP_BITSHIFT                             1
#define IIO_PERR_CONTEXT_CACHE                                  0

// iio errpinsts
#define IIO_PIN2_BITSHIFT                                       2
#define IIO_PIN1_BITSHIFT                                       1
#define IIO_PIN0_BITSHIFT                                       0

// iio gcerrst, gnerrst, gferrst
#define IIO_MC_BITSHIFT                                         26
#define IIO_VTD_BITSHIFT                                        25
#define IIO_MI_BITSHIFT                                         24
#define IIO_IOH_BITSHIFT                                        23
#define IIO_DMI_BITSHIFT                                        20
#define IIO_PCIE10_BITSHIFT                                     15
#define IIO_PCIE9_BITSHIFT                                      14
#define IIO_PCIE8_BITSHIFT                                      13
#define IIO_PCIE7_BITSHIFT                                      12
#define IIO_PCIE6_BITSHIFT                                      11
#define IIO_PCIE5_BITSHIFT                                      10
#define IIO_PCIE4_BITSHIFT                                      9
#define IIO_PCIE3_BITSHIFT                                      8
#define IIO_PCIE2_BITSHIFT                                      7
#define IIO_PCIE1_BITSHIFT                                      6
#define IIO_PCIE0_BITSHIFT                                      5
#define IIO_CSI0_ERR_BITSHIFT                                   1
#define IIO_CSI1_ERR_BITSHIFT                                   0

// iio irpp0errst, irpp1errst
#define IIO_PROTOCOL_PARITY_ERROR_BITSHIFT                      14
#define IIO_PROTOCOL_QT_OVERFLOW_UNDERFLOW_BITSHIFT             13
#define IIO_PROTOCOL_RCVD_UNEXPRSP_BITSHIFT                     10
#define IIO_CSR_ACC_32B_UNALIGNED_BITSHIFT                      6
#define IIO_WRCACHE_UNCECC_ERROR_CS1_BITSHIFT                   5
#define IIO_WRCACHE_UNCECC_ERROR_CS0_BITSHIFT                   4
#define IIO_PROTOCOL_RCVD_POISON_BITSHIFT                       3
#define IIO_WRCACHE_CORRECC_ERROR_CS1_BITSHIFT                  2
#define IIO_WRCACHE_CORRECC_ERROR_CS0_BITSHIFT                  1

// iio iioerrst
#define IIO_C6_BITSHIFT                                         6
#define IIO_C4_BITSHIFT                                         4
#define IIO_C8_IB_HEADER_PARITY_BITSHIFT                        1
#define IIO_8_IB_HEADER_PARITY_BITSHIFT                         0

// iio mierrst
#define IIO_VPP_ERR_STS_BITSHIFT                                3

// CPLD0 reg
#define CHIP_MIS_DET_HIS_OFFSET     0x0f04
#define CHIP_TEMP_DET_HIS_OFFSET    0x0f05

// CPLD0 chip_mis_det_his
#define PIN_FIVR_FAULT_BITSHIFT                                 0
#define PIN_ERR0_BITSHIFT                                       1
#define PIN_ERR1_BITSHIFT                                       2
#define PIN_ERR2_BITSHIFT                                       3
#define PIN_CATERR_BITSHIFT                                     4
#define PIN_SUS_STAT_C_BITSHIFT                                 5
#define PIN_SUS_S5_C_BITSHIFT                                   6
#define PIN_SUS_S4_C_BITSHIFT                                   7
#define PIN_SUS_S3_C_BITSHIFT                                   8
#define PIN_ADR_COMPLETE_BITSHIFT                               14
#define PIN_PWRGD_CPUPWRGD_BITSHIFT                             15

// CPLD0 chip_temp_det_his
#define PIN_THERMALTRIP_BITSHIFT                                0

#define IIO_NUM         5
#define PIN_NODE_NUM    11

struct cpu_drivers_t{
    ssize_t (*fmea_mcstatus_read) (u32 reg, char* buf);
    ssize_t (*fmea_thermal_read) (u32 reg, char* buf);
    ssize_t (*fmea_ubox_read) (int offset, char* buf);
    ssize_t (*fmea_iio_read) (unsigned int index, int length, int offset, const char *attr_name, char *buf);
    ssize_t (*fmea_pin_read) (char* buf);
    void (*get_loglevel) (long *lev);
    void (*set_loglevel) (long lev);
};

ssize_t drv_fmea_mcstatus_read(u32 reg, char* buf);
ssize_t drv_fmea_thermal_read(u32 reg, char* buf);
ssize_t drv_fmea_ubox_read(int offset, char* buf);
ssize_t drv_fmea_iio_read(unsigned int index, int length, int offset, const char *attr_name, char *buf);
ssize_t drv_fmea_pin_read(char* buf);
void drv_get_loglevel(long *lev);
void drv_set_loglevel(long lev);

void hisonic_cpu_drivers_register(struct cpu_drivers_t *p_func);
void hisonic_cpu_drivers_unregister(void);
void kssonic_cpu_drivers_register(struct cpu_drivers_t *p_func);
void kssonic_cpu_drivers_unregister(void);
void mxsonic_cpu_drivers_register(struct cpu_drivers_t *p_func);
void mxsonic_cpu_drivers_unregister(void);

#endif /* SWITCH_CPU_DRIVER_H */
