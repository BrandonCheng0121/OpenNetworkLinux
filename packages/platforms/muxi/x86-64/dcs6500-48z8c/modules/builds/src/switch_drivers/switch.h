#ifndef SWITCH_H
#define SWITCH_H

#include "libboundscheck/include/securec.h"

#ifndef FALSE
#define FALSE   0
#endif

#ifndef TRUE
#define TRUE    (!FALSE)
#endif

#define ENABLE_S3IP
extern unsigned int loglevel;
#define MAX_STR_BUFF_LENGTH 256
#define MAX_INDEX_STR_LEN 3
#define MAX_INDEX_NAME_LEN 32
#define INDEX_START 1
enum LOG_LEVEL{
    ERR         = 0x01,
    WARNING     = 0x02,
    INFO        = 0x04,
    DBG         = 0x08,
};

#define LOG_ERR(_prefix, fmt, args...) \
    do { \
        if (loglevel & ERR) \
        { \
            printk( KERN_ERR _prefix "%s "fmt, __FUNCTION__, ##args); \
        } \
    } while (0)
    
#define LOG_WARNING(_prefix, fmt, args...) \
    do { \
        if (loglevel & WARNING) \
        { \
            printk( KERN_WARNING _prefix "%s "fmt, __FUNCTION__, ##args); \
        } \
    } while (0)
    
#define LOG_INFO(_prefix, fmt, args...) \
    do { \
        if (loglevel & INFO) \
        { \
            printk( KERN_INFO _prefix "%s "fmt, __FUNCTION__, ##args); \
        } \
    } while (0)
    
#define LOG_DBG(_prefix, fmt, args...) \
    do { \
        if (loglevel & DBG) \
        { \
            printk( KERN_DEBUG _prefix "%s "fmt, __FUNCTION__, ##args); \
        } \
    } while (0)

// For S3IP always return success with NA for HW fail.
#define ERROR_RETURN_NA(ret) do { return sprintf_s(buf, PAGE_SIZE, "%s\n", "NA"); } while(0)

enum s3ip_led_status
{
    S3IP_LED_ALL_OFF,
    S3IP_LED_GREEN_ON,
    S3IP_LED_YELLOW_ON,
    S3IP_LED_RED_ON,
    S3IP_LED_BLUE_ON,
    S3IP_LED_GREEN_FLASH,
    S3IP_LED_YELLOW_FLASH,
    S3IP_LED_RED_FLASH,
    S3IP_LED_BLUE_FLASH
};

#endif /* SWITCH_H */
