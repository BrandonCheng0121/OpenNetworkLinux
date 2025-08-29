#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <asm/mce.h>
#include <asm/msr.h>

#include "switch_cpu_driver.h"

#define SWITCH_MXSONIC_CPU_VERSION "0.0.0.1"

static int debug = 0;
module_param(debug, uint, S_IRUGO);
MODULE_PARM_DESC(debug , "debug=1 to enable debug");

unsigned int loglevel = 0;
struct cpu_drivers_t *cb_func = NULL;

//For MXSONiC
extern struct kobject *mx_switch_kobj;
static struct kobject *cpu_kobj;
static struct kobject *status_kobj;
static struct kobject *iio_index_kobj[IIO_NUM];

enum {
    LOGLEVEL,
    NUM_CPU_ATTR,
};

enum mcaBanks {
    IFU,
    DCU,
    DTLB,
    MLC,
    PCU,
    IIO,
    HA,
    IMC0,
    IMC1,
    CBO0,
    CBO1,
    CBO2,
    NUM_BANK,
};

enum {
    CORE_THERMAL,
    PACKAGE_THERMAL,
    NUM_THERMAL_ATTR,
};

// Bus: 1, Device: 16, Function: 5 (Scratch)
enum {
    UBOX,
    NUM_UBOX_ATTR,
};

// Bus: 0, Device: 0, Function: 0 (DMI2)
enum {
    IIO0_PCI,
    IIO0_DEV,
    IIO0_UNC_ERR,
    IIO0_COR_ERR,
    IIO0_RP_ERR,
    IIO0_XPGLB_ERR,
    NUM_IIO0_ATTR,
};

// Bus: 0, Device: 1-3, Function: 0-1, 0/2, 0-3
enum {
    IIO1_PCI,
    IIO1_SEC,
    IIO1_DEV,
    IIO1_UNC_ERR,
    IIO1_COR_ERR,
    IIO1_RP_ERR,
    IIO1_XPGLB_ERR,
    NUM_IIO1_ATTR,
};

// Bus: 0, Device: 3, Function: 0 (IOU1 as NTB)
enum {
    IIO2_PCI,
    IIO2_DEV,
    IIO2_UNC_ERR,
    IIO2_COR_ERR,
    IIO2_RP_ERR,
    IIO2_XPGLB_ERR,
    IIO2_LN_ERR,
    NUM_IIO2_ATTR,
};

// Bus: 0, Device: 5, Function: 0 (Virtualization)
enum {
    IIO3_PCI,
    IIO3_VTUNC_ERR,
    NUM_IIO3_ATTR,
};

// Bus: 0, Device: 5, Function: 2 (Errors)
enum {
    IIO4_PCI,
    IIO4_ERRPIN,
    IIO4_GC_ERR,
    IIO4_GN_ERR,
    IIO4_GF_ERR,
    IIO4_IRPP0_ERR,
    IIO4_IRPP1_ERR,
    IIO4_IIO_ERR,
    IIO4_MI_ERR,
    NUM_IIO4_ATTR,
};

enum {
    IO_ERR,
    NUM_PIN_ATTR,
};

static char debug_string[NUM_BANK + NUM_THERMAL_ATTR + NUM_UBOX_ATTR + NUM_IIO0_ATTR + NUM_IIO1_ATTR + NUM_IIO2_ATTR + NUM_IIO3_ATTR + NUM_IIO4_ATTR + NUM_PIN_ATTR][DEBUG_STRING_LEN+1];

struct cpu_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct cpu_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct cpu_attribute *attr, const char *buf, size_t count);
};

struct mce_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct mce_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct mce_attribute *attr, const char *buf, size_t count);
    u8 index;
    u8 debug_index;
};

struct therm_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct therm_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct therm_attribute *attr, const char *buf, size_t count);
    u32 offset;
    u8 debug_index;
};

struct ubox_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct ubox_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct ubox_attribute *attr, const char *buf, size_t count);
    int offset;
    u8 debug_index;
};

struct iio_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct iio_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct iio_attribute *attr, const char *buf, size_t count);
    int length;
    int offset;  
    u8 debug_index;
};

struct pin_attribute{
    struct attribute attr;
    ssize_t (*show)(struct kobject *obj, struct pin_attribute *attr, char *buf);
    ssize_t (*store)(struct kobject *obj, struct pin_attribute *attr, const char *buf, size_t count);
    u8 debug_index;
};

struct mca_banks {
    char *name;
    int index;
};

static struct mca_banks banks[NUM_BANK] = {
    {"IFU",     0},
    {"DCU",     1},
    {"DTLB",    2},
    {"MLC",     3},
    {"PCU",     4},
    {"IIO",     6},
    {"HA",      7},
    {"IMC0",    9},
    {"IMC1",    10},
    {"CBO0",    17},
    {"CBO1",    18},
    {"CBO2",    19},
};

unsigned int get_iio_index(struct kobject *kobj)
{
    int retval;
    unsigned int iio_index;
    char iio_index_str[2] = {0};

    if(memcpy_s(iio_index_str, 2, (kobject_name(kobj)+3), 1) != 0)
    {
        return -ENOMEM;
    }
    retval = kstrtoint(iio_index_str, 10, &iio_index);
    if(retval == 0)
    {
        CPU_DEBUG("iio_index:%d \n", iio_index);
    }
    else
    {
        CPU_DEBUG("Error:%d, iio_index:%s \n", retval, iio_index_str);
        return -EINVAL;
    }

    return iio_index;
}

static int set_debug_string(unsigned int index, const char *buf)
{
    int len;

    if(debug == 0)
    {
        return -EACCES;
    }

    if(index >= (NUM_BANK + NUM_THERMAL_ATTR + NUM_UBOX_ATTR + NUM_IIO0_ATTR + NUM_IIO1_ATTR + NUM_IIO2_ATTR + NUM_IIO3_ATTR + NUM_IIO4_ATTR + NUM_PIN_ATTR))
    {
        return -EPERM;
    }

    memset_s(debug_string[index], DEBUG_STRING_LEN+1, 0x0, sizeof(debug_string[index]));
    if(buf != NULL)
    {
        len = (strlen(buf) < DEBUG_STRING_LEN) ? strlen(buf) : DEBUG_STRING_LEN;
        if(strncpy_s(debug_string[index], DEBUG_STRING_LEN+1, buf, len) != 0)
        {
            return -ENOMEM;
        }
        if(strlen(buf) > DEBUG_STRING_LEN)
        {
            debug_string[index][len] = '\n';
        }
    }

    return 0;
}

static ssize_t mxsonic_get_loglevel(struct kobject *kobj, struct cpu_attribute *attr, char *buf)
{
    return sprintf_s(buf, PAGE_SIZE, "%ld\n", loglevel);
}

static ssize_t mxsonic_set_loglevel(struct kobject *kobj, struct cpu_attribute *attr, const char *buf, size_t count)
{
    int retval;
    long lev;

    if(cb_func == NULL)
        return -1;

    retval = kstrtol(buf, 0, &lev);
    if(retval == 0)
    {
        CPU_DEBUG("lev:%ld \n", lev);
    }
    else
    {
        CPU_DEBUG("Error:%d, lev:%s \n", retval, buf);
        return -1;
    }

    loglevel = (unsigned int)lev;

    cb_func->set_loglevel(lev);

    return count;
}

static ssize_t mxsonic_fmea_mcstatus_read(struct kobject *kobj, struct mce_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    if(debug && (strlen(debug_string[attr->debug_index]) != 0))
    {
        return sprintf_s(buf, DEBUG_STRING_LEN+1, "%s", debug_string[attr->debug_index]);
    }

    retval = cb_func->fmea_mcstatus_read(MSR_IA32_MCx_STATUS(banks[attr->index].index), buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_fmea_thermal_read(struct kobject *kobj, struct therm_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    if(debug && (strlen(debug_string[attr->debug_index]) != 0))
    {
        return sprintf_s(buf, DEBUG_STRING_LEN+1, "%s", debug_string[attr->debug_index]);
    }

    retval = cb_func->fmea_thermal_read(attr->offset, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_fmea_ubox_read(struct kobject *kobj, struct ubox_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    if(debug && (strlen(debug_string[attr->debug_index]) != 0))
    {
        return sprintf_s(buf, DEBUG_STRING_LEN+1, "%s", debug_string[attr->debug_index]);
    }

    retval = cb_func->fmea_ubox_read(attr->offset, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_fmea_iio_read(struct kobject *kobj, struct iio_attribute *attr, char *buf)
{
    int retval;
    unsigned int iio_index;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    if(debug && (strlen(debug_string[attr->debug_index]) != 0))
    {
        return sprintf_s(buf, DEBUG_STRING_LEN+1, "%s", debug_string[attr->debug_index]);
    }

    iio_index = get_iio_index(kobj);
    if(iio_index < 0)
    {
        CPU_DEBUG("Get iio index failed.\n");
        ERROR_RETURN_NA(-EINVAL);
    }

    retval = cb_func->fmea_iio_read(iio_index, attr->length, attr->offset, attr->attr.name, buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_fmea_pin_read(struct kobject *kobj, struct pin_attribute *attr, char *buf)
{
    int retval;

    if(cb_func == NULL)
    {
        ERROR_RETURN_NA(-1);
    }

    if(debug && (strlen(debug_string[attr->debug_index]) != 0))
    {
        return sprintf_s(buf, DEBUG_STRING_LEN+1, "%s", debug_string[attr->debug_index]);
    }

    retval = cb_func->fmea_pin_read(buf);
    if(retval < 0)
    {
        ERROR_RETURN_NA(retval);
    }
    else
    {
        return retval;
    }
}

static ssize_t mxsonic_fmea_mcstatus_debug_write(struct kobject *kobj, struct mce_attribute *attr, const char *buf, size_t count)
{
    int rv;

    rv = set_debug_string(attr->debug_index, buf);

    if(rv != 0)
    {
        return rv;
    }

    return count;
}

static ssize_t mxsonic_fmea_thermal_debug_write(struct kobject *kobj, struct therm_attribute *attr, const char *buf, size_t count)
{
    int rv;

    rv = set_debug_string(attr->debug_index, buf);

    if(rv != 0)
    {
        return rv;
    }

    return count;
}

static ssize_t mxsonic_fmea_ubox_debug_write(struct kobject *kobj, struct ubox_attribute *attr, const char *buf, size_t count)
{
    int rv;

    rv = set_debug_string(attr->debug_index, buf);

    if(rv != 0)
    {
        return rv;
    }

    return count;
}

static ssize_t mxsonic_fmea_iio_debug_write(struct kobject *kobj, struct iio_attribute *attr, const char *buf, size_t count)
{
    int rv;

    rv = set_debug_string(attr->debug_index, buf);

    if(rv != 0)
    {
        return rv;
    }

    return count;
}

static ssize_t mxsonic_fmea_pin_debug_write(struct kobject *kobj, struct pin_attribute *attr, const char *buf, size_t count)
{
    int rv;

    rv = set_debug_string(attr->debug_index, buf);

    if(rv != 0)
    {
        return rv;
    }

    return count;
}

static struct cpu_attribute cpu_attr[NUM_CPU_ATTR] = {
    [LOGLEVEL]              = {{.name = "loglevel",             .mode = S_IRUGO | S_IWUSR},     mxsonic_get_loglevel,           mxsonic_set_loglevel},
};

static struct mce_attribute mce_attr[NUM_BANK] = {
    [IFU]                   = {{.name = "ifu",                  .mode = 0644},  mxsonic_fmea_mcstatus_read,  mxsonic_fmea_mcstatus_debug_write,   IFU,    0},
    [DCU]                   = {{.name = "dcu",                  .mode = 0644},  mxsonic_fmea_mcstatus_read,  mxsonic_fmea_mcstatus_debug_write,   DCU,    1},
    [DTLB]                  = {{.name = "dtlb",                 .mode = 0644},  mxsonic_fmea_mcstatus_read,  mxsonic_fmea_mcstatus_debug_write,   DTLB,   2},
    [MLC]                   = {{.name = "mlc",                  .mode = 0644},  mxsonic_fmea_mcstatus_read,  mxsonic_fmea_mcstatus_debug_write,   MLC,    3},
    [PCU]                   = {{.name = "pcu",                  .mode = 0644},  mxsonic_fmea_mcstatus_read,  mxsonic_fmea_mcstatus_debug_write,   PCU,    4},
    [IIO]                   = {{.name = "iio",                  .mode = 0644},  mxsonic_fmea_mcstatus_read,  mxsonic_fmea_mcstatus_debug_write,   IIO,    5},
    [HA]                    = {{.name = "ha",                   .mode = 0644},  mxsonic_fmea_mcstatus_read,  mxsonic_fmea_mcstatus_debug_write,   HA,     6},
    [IMC0]                  = {{.name = "imc0",                 .mode = 0644},  mxsonic_fmea_mcstatus_read,  mxsonic_fmea_mcstatus_debug_write,   IMC0,   7},
    [IMC1]                  = {{.name = "imc1",                 .mode = 0644},  mxsonic_fmea_mcstatus_read,  mxsonic_fmea_mcstatus_debug_write,   IMC1,   8},
    [CBO0]                  = {{.name = "cbo0",                 .mode = 0644},  mxsonic_fmea_mcstatus_read,  mxsonic_fmea_mcstatus_debug_write,   CBO0,   9},
    [CBO1]                  = {{.name = "cbo1",                 .mode = 0644},  mxsonic_fmea_mcstatus_read,  mxsonic_fmea_mcstatus_debug_write,   CBO1,   10},
    [CBO2]                  = {{.name = "cbo2",                 .mode = 0644},  mxsonic_fmea_mcstatus_read,  mxsonic_fmea_mcstatus_debug_write,   CBO2,   11},
};

static struct therm_attribute therm_attr[NUM_THERMAL_ATTR] = {
    [CORE_THERMAL]          = {{.name = "core_thermal",         .mode = 0644},  mxsonic_fmea_thermal_read,   mxsonic_fmea_thermal_debug_write,   MSR_IA32_THERM_STATUS,         12},
    [PACKAGE_THERMAL]       = {{.name = "package_thermal",      .mode = 0644},  mxsonic_fmea_thermal_read,   mxsonic_fmea_thermal_debug_write,   MSR_IA32_PACKAGE_THERM_STATUS, 13},
};

static struct ubox_attribute ubox_attr[NUM_UBOX_ATTR] = {
    [UBOX]                  = {{.name = "ubox",                 .mode = 0644},  mxsonic_fmea_ubox_read,      mxsonic_fmea_ubox_debug_write,   UBOX_UBOXERRSTS_OFFSET, 14},
};

static struct iio_attribute iio0_attr[NUM_IIO0_ATTR] = {
    [IIO0_PCI]                  = {{.name = "pci",              .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   2,  IIO0_PCI_OFFSET,       15},
    [IIO0_DEV]                  = {{.name = "dev",              .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   2,  IIO0_DEV_OFFSET,       16},
    [IIO0_UNC_ERR]              = {{.name = "unc_err",          .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO0_UNC_ERR_OFFSET,   17},
    [IIO0_COR_ERR]              = {{.name = "cor_err",          .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO0_COR_ERR_OFFSET,   18},
    [IIO0_RP_ERR]               = {{.name = "rp_err",           .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO0_RP_ERR_OFFSET,    19},
    [IIO0_XPGLB_ERR]            = {{.name = "xpglb_err",        .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   2,  IIO0_XPGLB_ERR_OFFSET, 20},
};

static struct iio_attribute iio1_attr[NUM_IIO1_ATTR] = {
    [IIO1_PCI]                  = {{.name = "pci",              .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   2,  IIO1_PCI_OFFSET,       21},
    [IIO1_SEC]                  = {{.name = "sec",              .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   2,  IIO1_SEC_OFFSET,       22},
    [IIO1_DEV]                  = {{.name = "dev",              .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   2,  IIO1_DEV_OFFSET,       23},
    [IIO1_UNC_ERR]              = {{.name = "unc_err",          .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO1_UNC_ERR_OFFSET,   24},
    [IIO1_COR_ERR]              = {{.name = "cor_err",          .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO1_COR_ERR_OFFSET,   25},
    [IIO1_RP_ERR]               = {{.name = "rp_err",           .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO1_RP_ERR_OFFSET,    26},
    [IIO1_XPGLB_ERR]            = {{.name = "xpglb_err",        .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   2,  IIO1_XPGLB_ERR_OFFSET, 27},
};

static struct iio_attribute iio2_attr[NUM_IIO2_ATTR] = {
    [IIO2_PCI]                  = {{.name = "pci",              .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   2,  IIO2_PCI_OFFSET,       28},
    [IIO2_DEV]                  = {{.name = "dev",              .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   2,  IIO2_DEV_OFFSET,       29},
    [IIO2_UNC_ERR]              = {{.name = "unc_err",          .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO2_UNC_ERR_OFFSET,   30},
    [IIO2_COR_ERR]              = {{.name = "cor_err",          .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO2_COR_ERR_OFFSET,   31},
    [IIO2_RP_ERR]               = {{.name = "rp_err",           .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO2_RP_ERR_OFFSET,    32},
    [IIO2_XPGLB_ERR]            = {{.name = "xpglb_err",        .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   2,  IIO2_XPGLB_ERR_OFFSET, 33},
    [IIO2_LN_ERR]               = {{.name = "ln_err",           .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO2_LN_ERR_OFFSET,    34},
};

static struct iio_attribute iio3_attr[NUM_IIO3_ATTR] = {
    [IIO3_PCI]                  = {{.name = "pci",              .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   2,  IIO3_PCI_OFFSET,       35},
    [IIO3_VTUNC_ERR]            = {{.name = "vtunc_err",        .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO3_VTUNC_ERR_OFFSET, 36},
};

static struct iio_attribute iio4_attr[NUM_IIO4_ATTR] = {
    [IIO4_PCI]                  = {{.name = "pci",              .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   2,  IIO4_PCI_OFFSET,       37},
    [IIO4_ERRPIN]               = {{.name = "errpin",           .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO4_ERRPIN_OFFSET,    38},
    [IIO4_GC_ERR]               = {{.name = "gc_err",           .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO4_GC_ERR_OFFSET,    39},
    [IIO4_GN_ERR]               = {{.name = "gn_err",           .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO4_GN_ERR_OFFSET,    40},
    [IIO4_GF_ERR]               = {{.name = "gf_err",           .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO4_GF_ERR_OFFSET,    41},
    [IIO4_IRPP0_ERR]            = {{.name = "irpp0_err",        .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO4_IRPP0_ERR_OFFSET, 42},
    [IIO4_IRPP1_ERR]            = {{.name = "irpp1_err",        .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO4_IRPP1_ERR_OFFSET, 43},
    [IIO4_IIO_ERR]              = {{.name = "iio_err",          .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO4_IIO_ERR_OFFSET,   44},
    [IIO4_MI_ERR]               = {{.name = "mi_err",           .mode = 0644},  mxsonic_fmea_iio_read,       mxsonic_fmea_iio_debug_write,   4,  IIO4_MI_ERR_OFFSET,    45},
};

static struct pin_attribute pin_attr[NUM_PIN_ATTR] = {
    [IO_ERR]                    = {{.name = "io_err",           .mode = 0644},  mxsonic_fmea_pin_read,   mxsonic_fmea_pin_debug_write, 46},
};

void mxsonic_cpu_drivers_register(struct cpu_drivers_t *pfunc)
{
    cb_func = pfunc;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_cpu_drivers_register);

void mxsonic_cpu_drivers_unregister(void)
{
    cb_func = NULL;

    return;
}
EXPORT_SYMBOL_GPL(mxsonic_cpu_drivers_unregister);

static int __init switch_cpu_init(void)
{
    int retval = 0;
    int i = 0;
    int iio_index;
    char *iio_index_str;
    int err;

    iio_index_str = (char *)kzalloc(6*sizeof(char), GFP_KERNEL);
    if (!iio_index_str)
    {
        CPU_DEBUG( "Fail to alloc iio_index_str memory\n");
        return -ENOMEM;
    }

    cpu_kobj = kobject_create_and_add("cpu", mx_switch_kobj);
    if(!cpu_kobj)
    {
        printk(KERN_ERR "[%s]Failed to create 'cpu'\n", __func__);
        retval = -ENOMEM;
        goto err_create_cpu_kobj_lable;
    }
    
    for(i = 0; i < NUM_CPU_ATTR; i++)
    {
        CPU_DEBUG("[%s]sysfs_create_file /cpu/%s\n", __func__, cpu_attr[i].attr.name);
        
        retval = sysfs_create_file(cpu_kobj, &cpu_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", cpu_attr[i].attr.name);          
            retval = -ENOMEM;            
            goto err_create_cpu_file_lable;
        }
    }
    
    /*cpu mce check*/
    status_kobj = kobject_create_and_add("status", cpu_kobj);
    if(!status_kobj)
    {
        printk(KERN_ERR "[%s]Failed to create 'ott_cpu'\n", __func__);
        retval = -ENOMEM;
        goto err_create_status_kobj_lable;
    }

    for(i = 0; i < NUM_BANK; i++)
    {
        CPU_DEBUG("[%s]sysfs_create_file /cpu/status/%s\n", __func__, mce_attr[i].attr.name);
        
        retval = sysfs_create_file(status_kobj, &mce_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", mce_attr[i].attr.name);          
            retval = -ENOMEM;            
            goto err_create_mce_file_lable;
        }
    }

    for(i = 0; i < NUM_THERMAL_ATTR; i++)
    {
        CPU_DEBUG( "[%s]sysfs_create_file /cpu/status/%s\n", __func__, therm_attr[i].attr.name);
        retval = sysfs_create_file(status_kobj, &therm_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", therm_attr[i].attr.name);  
            retval = -ENOMEM;
            goto err_create_therm_file_lable;
        }
    }

    CPU_DEBUG( "[%s]sysfs_create_file /cpu/status/%s\n", __func__, ubox_attr[0].attr.name);
    retval = sysfs_create_file(status_kobj, &ubox_attr[0].attr);
    if(retval)
    {
        printk(KERN_ERR "Failed to create file '%s'\n", ubox_attr[0].attr.name);
        retval = -ENOMEM;
        goto err_create_ubox_file_lable;
    }

    for(iio_index = 0; iio_index < IIO_NUM; iio_index++)
    {
        if(sprintf_s(iio_index_str, 6, "iio%d", iio_index) < 0)
        {
            CPU_DEBUG( "Failed to sprintf_s %s\n", iio_index_str);
            err = -ENOMEM;
            goto err_create_iio_index_label;
        }
        iio_index_kobj[iio_index] = kobject_create_and_add(iio_index_str, status_kobj);
        if(!iio_index_kobj[iio_index])
        {
            CPU_DEBUG( "Failed to create %s\n", iio_index_str);
            err = -ENOMEM;
            goto err_create_iio_index_label;
        }
    }

    for(i = 0; i < NUM_IIO0_ATTR; i++)
    {
        CPU_DEBUG( "[%s]sysfs_create_file /cpu/status/iio0/%s\n", __func__, iio0_attr[i].attr.name);
        retval = sysfs_create_file(iio_index_kobj[0], &iio0_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", iio0_attr[i].attr.name);
            retval = -ENOMEM;
            goto err_create_iio0_file_lable;
        }
    }

    for(i = 0; i < NUM_IIO1_ATTR; i++)
    {
        CPU_DEBUG( "[%s]sysfs_create_file /cpu/status/iio1/%s\n", __func__, iio1_attr[i].attr.name);
        retval = sysfs_create_file(iio_index_kobj[1], &iio1_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", iio1_attr[i].attr.name);
            retval = -ENOMEM;
            goto err_create_iio1_file_lable;
        }
    }

    for(i = 0; i < NUM_IIO2_ATTR; i++)
    {
        CPU_DEBUG( "[%s]sysfs_create_file /cpu/status/iio2/%s\n", __func__, iio2_attr[i].attr.name);
        retval = sysfs_create_file(iio_index_kobj[2], &iio2_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", iio2_attr[i].attr.name);
            retval = -ENOMEM;
            goto err_create_iio2_file_lable;
        }
    }

    for(i = 0; i < NUM_IIO3_ATTR; i++)
    {
        CPU_DEBUG( "[%s]sysfs_create_file /cpu/status/iio3/%s\n", __func__, iio3_attr[i].attr.name);
        retval = sysfs_create_file(iio_index_kobj[3], &iio3_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", iio3_attr[i].attr.name);
            retval = -ENOMEM;
            goto err_create_iio3_file_lable;
        }
    }

    for(i = 0; i < NUM_IIO4_ATTR; i++)
    {
        CPU_DEBUG( "[%s]sysfs_create_file /cpu/status/iio4/%s\n", __func__, iio4_attr[i].attr.name);
        retval = sysfs_create_file(iio_index_kobj[4], &iio4_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", iio4_attr[i].attr.name);
            retval = -ENOMEM;
            goto err_create_iio4_file_lable;
        }
    }

    for(i = 0; i < NUM_PIN_ATTR; i++)
    {
        CPU_DEBUG( "[%s]sysfs_create_file /cpu/status/%s\n", __func__, pin_attr[i].attr.name);
        retval = sysfs_create_file(status_kobj, &pin_attr[i].attr);
        if(retval)
        {
            printk(KERN_ERR "Failed to create file '%s'\n", pin_attr[i].attr.name);
            retval = -ENOMEM;
            goto err_create_pin_file_lable;
        }
    }

    return 0;

err_create_pin_file_lable:
    for(i = 0; i < NUM_PIN_ATTR; i++)
    {
        sysfs_remove_file(status_kobj, &pin_attr[i].attr);
    }

err_create_iio4_file_lable:
    for(i = 0; i < NUM_IIO4_ATTR; i++)
    {
        sysfs_remove_file(iio_index_kobj[4], &iio4_attr[i].attr);
    }

err_create_iio3_file_lable:
    for(i = 0; i < NUM_IIO3_ATTR; i++)
    {
        sysfs_remove_file(iio_index_kobj[3], &iio3_attr[i].attr);
    }

err_create_iio2_file_lable:
    for(i = 0; i < NUM_IIO2_ATTR; i++)
    {
        sysfs_remove_file(iio_index_kobj[2], &iio2_attr[i].attr);
    }

err_create_iio1_file_lable:
    for(i = 0; i < NUM_IIO1_ATTR; i++)
    {
        sysfs_remove_file(iio_index_kobj[1], &iio1_attr[i].attr);
    }

err_create_iio0_file_lable:
    for(i = 0; i < NUM_IIO0_ATTR; i++)
    {
        sysfs_remove_file(iio_index_kobj[0], &iio0_attr[i].attr);
    }

err_create_iio_index_label:
    for(iio_index=0; iio_index < IIO_NUM; iio_index++)
    {
        if(iio_index_kobj[iio_index])
        {
            kobject_put(iio_index_kobj[iio_index]);
            iio_index_kobj[iio_index] = NULL;
        }
    }

err_create_ubox_file_lable:
    for(i = 0; i < NUM_THERMAL_ATTR; i++)
    {
        sysfs_remove_file(status_kobj, &therm_attr[i].attr);
    }

err_create_therm_file_lable:  
    for(i = 0; i < NUM_BANK; i++)
    {
        sysfs_remove_file(status_kobj, &mce_attr[i].attr);
    }

err_create_mce_file_lable:
    if(status_kobj)
    {   
        kobject_put(status_kobj);
        status_kobj = NULL;
    }

err_create_status_kobj_lable:
    for(i = 0; i < NUM_CPU_ATTR; i++)
    {
        sysfs_remove_file(cpu_kobj, &cpu_attr[i].attr);
    }

err_create_cpu_file_lable:
    kobject_put(cpu_kobj);
    cpu_kobj = NULL;
    
err_create_cpu_kobj_lable:
    kfree(iio_index_str);

    return retval;
}

static void __exit switch_cpu_exit(void)
{
    int i = 0;
    int iio_index;

    for(i = 0; i < NUM_PIN_ATTR; i++)
    {
        sysfs_remove_file(status_kobj, &pin_attr[i].attr);
    }

    for(i = 0; i < NUM_IIO4_ATTR; i++)
    {
        sysfs_remove_file(iio_index_kobj[4], &iio4_attr[i].attr);
    }

    for(i = 0; i < NUM_IIO3_ATTR; i++)
    {
        sysfs_remove_file(iio_index_kobj[3], &iio3_attr[i].attr);
    }

    for(i = 0; i < NUM_IIO2_ATTR; i++)
    {
        sysfs_remove_file(iio_index_kobj[2], &iio2_attr[i].attr);
    }

    for(i = 0; i < NUM_IIO1_ATTR; i++)
    {
        sysfs_remove_file(iio_index_kobj[1], &iio1_attr[i].attr);
    }

    for(i = 0; i < NUM_IIO0_ATTR; i++)
    {
        sysfs_remove_file(iio_index_kobj[0], &iio0_attr[i].attr);
    }

    sysfs_remove_file(status_kobj, &ubox_attr[0].attr);

    for(i = 0; i < NUM_THERMAL_ATTR; i++)
    {
        sysfs_remove_file(status_kobj, &therm_attr[i].attr);
    }

    for(i = 0; i < NUM_BANK; i++)
    {
        sysfs_remove_file(status_kobj, &mce_attr[i].attr);
    }

    for(iio_index=0; iio_index < IIO_NUM; iio_index++)
    {
        if(iio_index_kobj[iio_index])
        {
            kobject_put(iio_index_kobj[iio_index]);
            iio_index_kobj[iio_index] = NULL;
        }
    }

    for(i = 0; i < NUM_CPU_ATTR; i++)
    {
        sysfs_remove_file(cpu_kobj, &cpu_attr[i].attr);
    }

    if(status_kobj)
    {
        kobject_put(status_kobj);
        status_kobj = NULL;
    }

    if(cpu_kobj)
    {
        kobject_put(cpu_kobj);
        cpu_kobj = NULL;
    }

    cb_func = NULL;

    printk(KERN_INFO "CPU module remove done.\n");

    return;
}

MODULE_DESCRIPTION("Muxi Switch MXSONiC CPU Driver");
MODULE_VERSION(SWITCH_MXSONIC_CPU_VERSION);
MODULE_LICENSE("GPL");

module_init(switch_cpu_init);
module_exit(switch_cpu_exit);
