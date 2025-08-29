#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <asm/mce.h>
#include <asm/msr.h>
#include <linux/pci.h>

#include "switch_cpu_driver.h"
#include "cpld_lpc_driver.h"

#define DRVNAME "drv_cpu_driver"
#define SWITCH_CPU_DRIVER_VERSION "0.0.0.1"

unsigned int loglevel = 0;
static struct platform_device *drv_cpu_device;
struct mutex     update_lock;

static int iio_devid[IIO_NUM] = {PCI_IIO0_DEVICE_ID, PCI_IIO1_DEVICE_ID, PCI_IIO2_DEVICE_ID, PCI_IIO3_DEVICE_ID, PCI_IIO4_DEVICE_ID};
static char *pin_node_name[PIN_NODE_NUM] = {"cpu_err0", "cpu_err1", "cpu_err2", "cpu_termaltrip", "CPU_CATERR_N", "fivr_fault", "sus_stat_n_gpio61",
                                            "slp_s3_s4_s5", "PROCPWRGD_PCH", "FIVR_FAULT_and_THERMTRIP_N", "ADR_COMPLETE"};
#if 0
void clear_cpld0_reg(unsigned int offset)
{
    unsigned char buf[2];

    // Write all 1 to clear register
    buf[1] = (0xffff >> 8) & 0xff;
    buf[0] = 0xffff & 0xff;
    lpc_write_cpld(0, offset, buf);
}
#endif
/* MSR access wrappers used for error injection */
static u64 mce_rdmsrl(u32 msr)
{
    u64 v;

    if (rdmsrl_safe(msr, &v))
    {
        CPU_DEBUG("mce: Unable to read MSR 0x%x!\n", msr);
        v = 0xffffffffffffffff;
    }

    return v;
}

static void mce_wrmsrl(u32 msr, u64 v)
{
    wrmsrl(msr, v);
}

static unsigned int msr_check_thermal(u64 val)
{
    if(CHECK_BIT(val, MSR_THRESHOLD2_STATUS_BITSHIFT) || CHECK_BIT(val, MSR_THRESHOLD1_STATUS_BITSHIFT) || CHECK_BIT(val, MSR_CRITICAL_TEMP_STATUS_BITSHIFT) ||
       CHECK_BIT(val, MSR_PROCHOT_STATUS_BITSHIFT) || CHECK_BIT(val, MSR_THERMAL_STATUS_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int ubox_check_uboxerrsts(u32 val)
{
    if(CHECK_BIT(val, UBOX_SMI_DELIVERY_VALID_BITSHIFT) || CHECK_BIT(val, UBOX_MASTER_LOCK_TIMEOUT_BITSHIFT) || CHECK_BIT(val, UBOX_SMI_TIMEOUT_BITSHIFT) ||
       CHECK_BIT(val, UBOX_CFG_WR_ADDR_MIS_ALIGNED_BITSHIFT) || CHECK_BIT(val, UBOX_CFG_RD_ADDR_MIS_ALIGNED_BITSHIFT) || CHECK_BIT(val, UBOX_UNSURPPORTED_OPCODE_BITSHIFT) ||
       CHECK_BIT(val, UBOX_POISON_RSVD_BITSHIFT) || CHECK_BIT(val, UBOX_SMI_SRC_IMC_BITSHIFT) || CHECK_BIT(val, UBOX_SMI_SRC_UMC_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_pcists(unsigned int iio_index, u16 val)
{
    if(iio_index != 4)
    {
        if(CHECK_BIT(val, IIO_DPE_BITSHIFT) || CHECK_BIT(val, IIO_SSE_BITSHIFT) || CHECK_BIT(val, IIO_RMA_BITSHIFT) ||
           CHECK_BIT(val, IIO_RTA_BITSHIFT) || CHECK_BIT(val, IIO_STA_BITSHIFT) || CHECK_BIT(val, IIO_MDPE_BITSHIFT))
            return 1;
        else
            return 0;
    }
    else
    {
        if(CHECK_BIT(val, IIO_DPE_BITSHIFT))
            return 1;
        else
            return 0;
    }
}

static unsigned int iio_check_devsts(u16 val)
{
    if(CHECK_BIT(val, IIO_UNSUPPORTED_REQUEST_DETECTED_BITSHIFT) || CHECK_BIT(val, IIO_FATAL_ERROR_DETECTED_BITSHIFT) || CHECK_BIT(val, IIO_NON_FATAL_ERROR_DETECTED_BITSHIFT) ||
       CHECK_BIT(val, IIO_CORRECTABLE_ERROR_DETECTED_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_uncerrsts(u32 val)
{
    if(CHECK_BIT(val, IIO_ACS_VIOLATION_STATUS_BITSHIFT) || CHECK_BIT(val, IIO_RECEIVE_AN_UNSUPPORTED_REQUEST_BITSHIFT) || CHECK_BIT(val, IIO_ECRC_ERROR_STATUS_BITSHIFT) ||
       CHECK_BIT(val, IIO_MALFORMED_TLP_STATUS_BITSHIFT) || CHECK_BIT(val, IIO_RECEIVER_BUFFER_OVERFLOW_STATUS_BITSHIFT) || CHECK_BIT(val, IIO_UNEXPECTED_COMPLETION_STATUS_BITSHIFT) ||
       CHECK_BIT(val, IIO_COMPLETER_ABORT_STATUS_BITSHIFT) || CHECK_BIT(val, IIO_COMPLETION_TIME_OUT_STATUS_BITSHIFT) || CHECK_BIT(val, IIO_FLOW_CONTROL_PROTOCOL_ERROR_STATUS_BITSHIFT) ||
       CHECK_BIT(val, IIO_POISONED_TLP_STATUS_BITSHIFT) || CHECK_BIT(val, IIO_SURPRISE_DOWN_ERROR_STATUS_BITSHIFT) || CHECK_BIT(val, IIO_DATA_LINK_PROTOCOL_ERROR_STATUS_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_corerrsts(u32 val)
{
    if(CHECK_BIT(val, IIO_ADVISORY_NON_FATAL_ERROR_STATUS_BITSHIFT) || CHECK_BIT(val, IIO_REPLAY_TIMER_TIME_OUT_STATUS_BITSHIFT) || CHECK_BIT(val, IIO_REPLAY_NUM_ROLLOVER_STATUS_BITSHIFT) ||
       CHECK_BIT(val, IIO_BAD_DLLP_STATUS_BITSHIFT) || CHECK_BIT(val, IIO_BAD_TLP_STATUS_BITSHIFT) || CHECK_BIT(val, IIO_RECEIVER_ERROR_STATUS_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_rperrsts(u32 val)
{
    if(CHECK_BIT(val, IIO_FATAL_ERROR_MESSAGES_RECEIVED_BITSHIFT) || CHECK_BIT(val, IIO_NON_FATAL_ERROR_MESSAGES_RECEIVED_BITSHIFT) || CHECK_BIT(val, IIO_FIRST_UNCORRECTABLE_FATAL_BITSHIFT) ||
       CHECK_BIT(val, IIO_MULTIPLE_ERROR_FATAL_NONFATAL_RECEIVED_BITSHIFT) || CHECK_BIT(val, IIO_ERROR_FATAL_NONFATAL_RECEIVED_BITSHIFT) || CHECK_BIT(val, IIO_MULTIPLE_CORRECTABLE_ERROR_RECEIVED_BITSHIFT) ||
       CHECK_BIT(val, IIO_CORRECTABLE_ERROR_RECEIVED_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_xpglberrsts(u16 val)
{
    if(CHECK_BIT(val, IIO_PCIE_AER_CORRECTABLE_ERROR_BITSHIFT) || CHECK_BIT(val, IIO_PCIE_AER_NON_FATAL_ERROR_BITSHIFT) || CHECK_BIT(val, IIO_PCIE_AER_FATAL_ERROR_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_secsts(u16 val)
{
    if(CHECK_BIT(val, IIO_DPE_BITSHIFT) || CHECK_BIT(val, IIO_SSE_BITSHIFT) || CHECK_BIT(val, IIO_RMA_BITSHIFT) ||
       CHECK_BIT(val, IIO_RTA_BITSHIFT) || CHECK_BIT(val, IIO_STA_BITSHIFT) || CHECK_BIT(val, IIO_MDPE_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_lnerrsts(u32 val)
{
    if((val & 0xffff) != 0x0)
        return 1;
    else
        return 0;
}

static unsigned int iio_check_vtuncerrsts(u32 val)
{
    if(CHECK_BIT(val, IIO_PERR_TLB1_BITSHIFT) || CHECK_BIT(val, IIO_PERR_TLB0_BITSHIFT) || CHECK_BIT(val, IIO_PERR_L3_LOOKUP_BITSHIFT) ||
       CHECK_BIT(val, IIO_PERR_L1_LOOKUP_BITSHIFT) || CHECK_BIT(val, IIO_PERR_L2_LOOKUP_BITSHIFT) || CHECK_BIT(val, IIO_PERR_CONTEXT_CACHE))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_errpinsts(u32 val)
{
    if(CHECK_BIT(val, IIO_PIN2_BITSHIFT) || CHECK_BIT(val, IIO_PIN1_BITSHIFT) || CHECK_BIT(val, IIO_PIN0_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_gcerrst(u32 val)
{
    if(CHECK_BIT(val, IIO_MC_BITSHIFT) || CHECK_BIT(val, IIO_VTD_BITSHIFT) || CHECK_BIT(val, IIO_MI_BITSHIFT) ||
       CHECK_BIT(val, IIO_IOH_BITSHIFT) || CHECK_BIT(val, IIO_PCIE10_BITSHIFT) || CHECK_BIT(val, IIO_PCIE9_BITSHIFT) || 
       CHECK_BIT(val, IIO_PCIE8_BITSHIFT) || CHECK_BIT(val, IIO_PCIE6_BITSHIFT) ||
       CHECK_BIT(val, IIO_PCIE5_BITSHIFT) || CHECK_BIT(val, IIO_PCIE4_BITSHIFT) || CHECK_BIT(val, IIO_PCIE3_BITSHIFT) || 
       CHECK_BIT(val, IIO_PCIE2_BITSHIFT) || CHECK_BIT(val, IIO_PCIE1_BITSHIFT) || CHECK_BIT(val, IIO_PCIE0_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_gnerrst(u32 val)
{
    if(CHECK_BIT(val, IIO_VTD_BITSHIFT) || CHECK_BIT(val, IIO_MI_BITSHIFT) || CHECK_BIT(val, IIO_IOH_BITSHIFT) || 
       CHECK_BIT(val, IIO_PCIE10_BITSHIFT) || CHECK_BIT(val, IIO_PCIE9_BITSHIFT) || CHECK_BIT(val, IIO_PCIE8_BITSHIFT) || 
       CHECK_BIT(val, IIO_PCIE4_BITSHIFT) || CHECK_BIT(val, IIO_PCIE3_BITSHIFT) || 
       CHECK_BIT(val, IIO_PCIE2_BITSHIFT) || CHECK_BIT(val, IIO_PCIE1_BITSHIFT) || CHECK_BIT(val, IIO_PCIE0_BITSHIFT) || 
       CHECK_BIT(val, IIO_CSI0_ERR_BITSHIFT) || CHECK_BIT(val, IIO_CSI1_ERR_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_gferrst(u32 val)
{
    if(CHECK_BIT(val, IIO_VTD_BITSHIFT) || CHECK_BIT(val, IIO_MI_BITSHIFT) || CHECK_BIT(val, IIO_IOH_BITSHIFT) ||
       CHECK_BIT(val, IIO_DMI_BITSHIFT) || CHECK_BIT(val, IIO_PCIE10_BITSHIFT) || CHECK_BIT(val, IIO_PCIE9_BITSHIFT) ||
       CHECK_BIT(val, IIO_PCIE8_BITSHIFT) || CHECK_BIT(val, IIO_PCIE7_BITSHIFT) || CHECK_BIT(val, IIO_PCIE6_BITSHIFT) ||
       CHECK_BIT(val, IIO_PCIE5_BITSHIFT) || CHECK_BIT(val, IIO_PCIE4_BITSHIFT) || CHECK_BIT(val, IIO_PCIE3_BITSHIFT) ||
       CHECK_BIT(val, IIO_PCIE2_BITSHIFT) || CHECK_BIT(val, IIO_PCIE1_BITSHIFT) || CHECK_BIT(val, IIO_PCIE0_BITSHIFT) ||
       CHECK_BIT(val, IIO_CSI0_ERR_BITSHIFT) || CHECK_BIT(val, IIO_CSI1_ERR_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_irpp0errst(u32 val)
{
    if(CHECK_BIT(val, IIO_PROTOCOL_PARITY_ERROR_BITSHIFT) || CHECK_BIT(val, IIO_PROTOCOL_QT_OVERFLOW_UNDERFLOW_BITSHIFT) || CHECK_BIT(val, IIO_PROTOCOL_RCVD_UNEXPRSP_BITSHIFT) ||
       CHECK_BIT(val, IIO_CSR_ACC_32B_UNALIGNED_BITSHIFT) || CHECK_BIT(val, IIO_WRCACHE_UNCECC_ERROR_CS1_BITSHIFT) || CHECK_BIT(val, IIO_WRCACHE_UNCECC_ERROR_CS0_BITSHIFT) ||
       CHECK_BIT(val, IIO_PROTOCOL_RCVD_POISON_BITSHIFT) || CHECK_BIT(val, IIO_WRCACHE_CORRECC_ERROR_CS1_BITSHIFT) || CHECK_BIT(val, IIO_WRCACHE_CORRECC_ERROR_CS0_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_irpp1errst(u32 val)
{
    if(CHECK_BIT(val, IIO_PROTOCOL_PARITY_ERROR_BITSHIFT) || CHECK_BIT(val, IIO_PROTOCOL_QT_OVERFLOW_UNDERFLOW_BITSHIFT) || CHECK_BIT(val, IIO_PROTOCOL_RCVD_UNEXPRSP_BITSHIFT) ||
       CHECK_BIT(val, IIO_CSR_ACC_32B_UNALIGNED_BITSHIFT) || CHECK_BIT(val, IIO_WRCACHE_UNCECC_ERROR_CS1_BITSHIFT) || CHECK_BIT(val, IIO_WRCACHE_UNCECC_ERROR_CS0_BITSHIFT) ||
       CHECK_BIT(val, IIO_PROTOCOL_RCVD_POISON_BITSHIFT) || CHECK_BIT(val, IIO_WRCACHE_CORRECC_ERROR_CS1_BITSHIFT) || CHECK_BIT(val, IIO_WRCACHE_CORRECC_ERROR_CS0_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_iioerrst(u32 val)
{
    if(CHECK_BIT(val, IIO_C6_BITSHIFT) || CHECK_BIT(val, IIO_C4_BITSHIFT) || CHECK_BIT(val, IIO_C8_IB_HEADER_PARITY_BITSHIFT) ||
       CHECK_BIT(val, IIO_8_IB_HEADER_PARITY_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int iio_check_mierrst(u32 val)
{
    if(CHECK_BIT(val, IIO_VPP_ERR_STS_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int pin_check_cpu_err0(unsigned short val)
{
    if(CHECK_BIT(val, PIN_ERR0_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int pin_check_cpu_err1(unsigned short val)
{
    if(CHECK_BIT(val, PIN_ERR1_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int pin_check_cpu_err2(unsigned short val)
{
    if(CHECK_BIT(val, PIN_ERR2_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int pin_check_cpu_thermaltrip(unsigned short val)
{
    if(CHECK_BIT(val, PIN_THERMALTRIP_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int pin_check_caterr(unsigned short val)
{
    if(CHECK_BIT(val, PIN_CATERR_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int pin_check_fivr_fault(unsigned short val)
{
    if(CHECK_BIT(val, PIN_FIVR_FAULT_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int pin_check_sus_stat_n_gpio61(unsigned short val)
{
    if(CHECK_BIT(val, PIN_SUS_STAT_C_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int pin_check_slp_s3_s4_s5(unsigned short val)
{
    if(CHECK_BIT(val, PIN_SUS_S5_C_BITSHIFT) || CHECK_BIT(val, PIN_SUS_S4_C_BITSHIFT) || CHECK_BIT(val, PIN_SUS_S3_C_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int pin_check_procpwrgd_pch(unsigned short val)
{
    if(CHECK_BIT(val, PIN_PWRGD_CPUPWRGD_BITSHIFT))
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

static unsigned int pin_check_fivr_fault_and_thermaltrip(unsigned short val_mis, unsigned short val_temp)
{
    if(CHECK_BIT(val_mis, PIN_PWRGD_CPUPWRGD_BITSHIFT) && CHECK_BIT(val_mis, PIN_FIVR_FAULT_BITSHIFT) && !CHECK_BIT(val_temp, PIN_THERMALTRIP_BITSHIFT))
        return 1;
    else
        return 0;
}

static unsigned int pin_check_adr_complete(unsigned short val)
{
    if(CHECK_BIT(val, PIN_ADR_COMPLETE_BITSHIFT))
        return 1;
    else
        return 0;
}

ssize_t drv_fmea_mcstatus_read(u32 reg, char *buf)
{
    u64 val;
    bool is_error;

    mutex_lock(&update_lock);

    val = mce_rdmsrl(reg);
    is_error = (val >> 63) & 0x1;

    mce_wrmsrl(reg, 0); //clear MSR_IA32_MCx_STATUS

    mutex_unlock(&update_lock);

    return sprintf_s(buf, PAGE_SIZE, "%d 0x%016llx\n", is_error, val);
}

ssize_t drv_fmea_thermal_read(u32 reg, char *buf)
{
    u64 val;
    int result;

    mutex_lock(&update_lock);

    val = mce_rdmsrl(reg);

    mutex_unlock(&update_lock);

    result = msr_check_thermal(val);

    return sprintf_s(buf, PAGE_SIZE, "%d 0x%016llx\n", result, val);
}

ssize_t drv_fmea_ubox_read(int offset, char *buf)
{
    u32 val, temp_val;
    struct pci_dev *dev = NULL;
    int ret;
    int result;

    dev = pci_get_device(PCI_INTEL_VENDOR_ID, PCI_UBOX_DEVICE_ID, dev);
    if(!dev)
    {
        CPU_DEBUG("Unable to find UBOX.\n");
        return -ENXIO;
    }

    mutex_lock(&update_lock);

    ret = pci_read_config_dword(dev, offset, &val);
    if(ret < 0)
    {
        CPU_DEBUG("Read UBOX UBOXErrSts failed.\n");
        mutex_unlock(&update_lock);
        return ret;
    }

    mutex_unlock(&update_lock);

    result = ubox_check_uboxerrsts(val);

    if(CHECK_BIT(val, UBOX_SMI_DELIVERY_VALID_BITSHIFT))
    {
        //Write 0 to clear bit[16]
        temp_val = val & ~(0x1 << UBOX_SMI_DELIVERY_VALID_BITSHIFT);
        pci_write_config_dword(dev, offset, temp_val);
    }

    return sprintf_s(buf, PAGE_SIZE, "%d 0x%08x\n", result, val);
}

ssize_t drv_fmea_iio_read(unsigned int index, int length, int offset, const char *attr_name, char *buf)
{
    u32 val;
    struct pci_dev *dev = NULL;
    int ret;
    unsigned int result;

    dev = pci_get_device(PCI_INTEL_VENDOR_ID, iio_devid[index], dev);
    if(!dev)
    {
        CPU_DEBUG("Unable to find IIO devid: 0x%x.\n", iio_devid[index]);
        return -ENXIO;
    }

    mutex_lock(&update_lock);

    if(length == 2)
    {
        ret = pci_read_config_word(dev, offset, (u16 *)&val);
        if(ret < 0)
        {
            CPU_DEBUG("Read IIO%d %s failed.\n", index, attr_name);
            mutex_unlock(&update_lock);
            return ret;
        }

        mutex_unlock(&update_lock);

        if(!strcmp(attr_name, "pci"))
            result = iio_check_pcists(index, (u16)val);
        else if(!strcmp(attr_name, "dev"))
            result = iio_check_devsts((u16)val);
        else if(!strcmp(attr_name, "xpglb_err"))
            result = iio_check_xpglberrsts((u16)val);
        else if(!strcmp(attr_name, "sec"))
            result = iio_check_secsts((u16)val);

        if(result)
            pci_write_config_word(dev, offset, 0xffff); //Write all 1 to clear register

        return sprintf_s(buf, PAGE_SIZE, "%d 0x%04x\n", result, (u16)val);
    }
    else
    {
        ret = pci_read_config_dword(dev, offset, &val);
        if(ret < 0)
        {
            CPU_DEBUG("Read IIO%d %s failed.\n", index, attr_name);
            mutex_unlock(&update_lock);
            return ret;
        }

        mutex_unlock(&update_lock);

        if(!strcmp(attr_name, "unc_err"))
            result = iio_check_uncerrsts(val);
        else if(!strcmp(attr_name, "cor_err"))
            result = iio_check_corerrsts(val);
        else if(!strcmp(attr_name, "rp_err"))
            result = iio_check_rperrsts(val);
        else if(!strcmp(attr_name, "ln_err"))
            result = iio_check_lnerrsts(val);
        else if(!strcmp(attr_name, "vtunc_err"))
            result = iio_check_vtuncerrsts(val);
        else if(!strcmp(attr_name, "errpin"))
            result = iio_check_errpinsts(val);
        else if(!strcmp(attr_name, "gc_err"))
            result = iio_check_gcerrst(val);
        else if(!strcmp(attr_name, "gn_err"))
            result = iio_check_gnerrst(val);
        else if(!strcmp(attr_name, "gf_err"))
            result = iio_check_gferrst(val);
        else if(!strcmp(attr_name, "irpp0_err"))
            result = iio_check_irpp0errst(val);
        else if(!strcmp(attr_name, "irpp1_err"))
            result = iio_check_irpp1errst(val);
        else if(!strcmp(attr_name, "iio_err"))
            result = iio_check_iioerrst(val);
        else if(!strcmp(attr_name, "mi_err"))
            result = iio_check_mierrst(val);

        if(result)
            pci_write_config_dword(dev, offset, 0xffffffff); //Write all 1 to clear register

        return sprintf_s(buf, PAGE_SIZE, "%d 0x%08x\n", result, val);
    }
}

ssize_t drv_fmea_pin_read(char *buf)
{
#if 0
    int i;
    unsigned int result;
    unsigned char ret[2];
    unsigned short val_mis = 0, val_temp = 0;
    int len = 0, tmp_len = 0;
    char tmp[512] = {0};

    mutex_lock(&update_lock);

    // lpc_read_cpld(0, CHIP_MIS_DET_HIS_OFFSET, ret);
    // val_mis = (ret[1] << 8) | ret[0];

    // lpc_read_cpld(0, CHIP_TEMP_DET_HIS_OFFSET, ret);
    // val_temp = (ret[1] << 8) | ret[0];

    for(i = 0; i < PIN_NODE_NUM; i++)
    {
        if(!strcmp(pin_node_name[i], "cpu_err0"))
        {
            result = pin_check_cpu_err0(val_mis);
        }
        else if(!strcmp(pin_node_name[i], "cpu_err1"))
        {
            result = pin_check_cpu_err1(val_mis);
        }
        else if(!strcmp(pin_node_name[i], "cpu_err2"))
        {
            result = pin_check_cpu_err2(val_mis);
        }
        else if(!strcmp(pin_node_name[i], "cpu_termaltrip"))
        {
            result = pin_check_cpu_thermaltrip(val_temp);
        }
        else if(!strcmp(pin_node_name[i], "CPU_CATERR_N"))
        {
            result = pin_check_caterr(val_mis);
        }
        else if(!strcmp(pin_node_name[i], "fivr_fault"))
        {
            result = pin_check_fivr_fault(val_mis);
        }
        else if(!strcmp(pin_node_name[i], "sus_stat_n_gpio61"))
        {
            result = pin_check_sus_stat_n_gpio61(val_mis);
        }
        else if(!strcmp(pin_node_name[i], "slp_s3_s4_s5"))
        {
            result = pin_check_slp_s3_s4_s5(val_mis);
        }
        else if(!strcmp(pin_node_name[i], "PROCPWRGD_PCH"))
        {
            result = pin_check_procpwrgd_pch(val_mis);
        }
        else if(!strcmp(pin_node_name[i], "FIVR_FAULT_and_THERMTRIP_N"))
        {
            result = pin_check_fivr_fault_and_thermaltrip(val_mis, val_temp);
        }
        else if(!strcmp(pin_node_name[i], "ADR_COMPLETE"))
        {
            result = pin_check_adr_complete(val_mis);
        }

        tmp_len = sprintf_s(tmp+len, 512, "%d %s %d\n", i+1, pin_node_name[i], result);
        if( tmp_len > 0 )
        {
            len += tmp_len;
        }
    }

    // clear_cpld0_reg(CHIP_MIS_DET_HIS_OFFSET);
    // clear_cpld0_reg(CHIP_TEMP_DET_HIS_OFFSET);

    mutex_unlock(&update_lock);
#endif
    return sprintf_s(buf, PAGE_SIZE, "NA\n");//(buf, "%d 0x%04x\n", result, val);
}

void drv_get_loglevel(long *lev)
{
    *lev = (long)loglevel;

    return;
}

void drv_set_loglevel(long lev)
{
    loglevel = (unsigned int)lev;

    return;
}

// For kissonic
static struct cpu_drivers_t pfunc = {
    .fmea_mcstatus_read = drv_fmea_mcstatus_read,
    .fmea_thermal_read = drv_fmea_thermal_read,
    .fmea_ubox_read = drv_fmea_ubox_read,
    .fmea_iio_read = drv_fmea_iio_read,
    .fmea_pin_read = drv_fmea_pin_read,
    .get_loglevel = drv_get_loglevel,
    .set_loglevel = drv_set_loglevel,
};

static int drv_cpu_probe(struct platform_device *pdev)
{

#ifdef ENABLE_S3IP
    mxsonic_cpu_drivers_register(&pfunc);
#else
    kssonic_cpu_drivers_register(&pfunc);
    hisonic_cpu_drivers_register(&pfunc);
#endif
    return 0;
}

static int drv_cpu_remove(struct platform_device *pdev)
{
#ifdef ENABLE_S3IP
    mxsonic_cpu_drivers_unregister();
#else
    kssonic_cpu_drivers_unregister();
    hisonic_cpu_drivers_unregister();
#endif
    return 0;
}

static struct platform_driver drv_cpu_driver = {
    .driver = {
        .name   = DRVNAME,
        .owner = THIS_MODULE,
    },
    .probe      = drv_cpu_probe,
    .remove     = drv_cpu_remove,
};

static int __init drv_cpu_init(void)
{
    int err;
    int retval;
    u32 val, temp_val;
    struct pci_dev *dev = NULL;

    drv_cpu_device = platform_device_alloc(DRVNAME, 0);
    if(!drv_cpu_device)
    {
        CPU_DEBUG("(#%d): platform_device_alloc fail\n", __LINE__);
        return -ENOMEM;
    }

    retval = platform_device_add(drv_cpu_device);
    if(retval)
    {
        CPU_DEBUG("(#%d): platform_device_add failed\n", __LINE__);
        err = -retval;
        goto dev_add_failed;
    }

    retval = platform_driver_register(&drv_cpu_driver);
    if(retval)
    {
        CPU_DEBUG("(#%d): platform_driver_register failed(%d)\n", __LINE__, err);
        err = -retval;
        goto dev_reg_failed;
    }

    mutex_init(&update_lock);

    dev = pci_get_device(PCI_INTEL_VENDOR_ID, PCI_UBOX_DEVICE_ID, dev);
    if(!dev)
    {
        CPU_DEBUG("Unable to find UBOX.\n");
        err = -ENXIO;
        goto get_pci_device_ubox_failed;
    }

    mutex_lock(&update_lock);

    retval = pci_read_config_dword(dev, UBOX_UBOXERRSTS_OFFSET, &val);
    if(retval < 0)
    {
        CPU_DEBUG("Read UBOX UBOXErrSts failed.\n");
        mutex_unlock(&update_lock);
        err = retval;
        goto read_ubox_errsts_reg_failed;
    }

    mutex_unlock(&update_lock);

    // clear all bit after init
    if(CHECK_BIT(val, UBOX_SMI_DELIVERY_VALID_BITSHIFT))
    {
        //Write 0 to clear bit[16]
        temp_val = val & ~(0x1 << UBOX_SMI_DELIVERY_VALID_BITSHIFT);
        pci_write_config_dword(dev, UBOX_UBOXERRSTS_OFFSET, temp_val);
    }

//    clear_cpld0_reg(CHIP_MIS_DET_HIS_OFFSET);
//    clear_cpld0_reg(CHIP_TEMP_DET_HIS_OFFSET);

    return 0;

read_ubox_errsts_reg_failed:
get_pci_device_ubox_failed:
    mutex_destroy(&update_lock);

dev_reg_failed:
    platform_device_unregister(drv_cpu_device);

dev_add_failed:
    platform_device_put(drv_cpu_device);
    return err;
}

static void __exit drv_cpu_exit(void)
{
    platform_driver_unregister(&drv_cpu_driver);
    platform_device_unregister(drv_cpu_device);

    mutex_destroy(&update_lock);

    return;
}

MODULE_DESCRIPTION("Huarong CPU Driver");
MODULE_VERSION(SWITCH_CPU_DRIVER_VERSION);
MODULE_LICENSE("GPL");

module_init(drv_cpu_init);
module_exit(drv_cpu_exit);
