/**************************************************************************//**
 *
 * @file
 * @brief x86_64_muxi_dcs6500_48z8c Porting Macros.
 *
 * @addtogroup x86_64_muxi_dcs6500_48z8c-porting
 * @{
 *
 *****************************************************************************/
#ifndef __x86_64_muxi_dcs6500_48z8c_PORTING_H__
#define __x86_64_muxi_dcs6500_48z8c_PORTING_H__


/* <auto.start.portingmacro(ALL).define> */
#if X86_64_MUXI_DCS6500_48Z8C_CONFIG_PORTING_INCLUDE_STDLIB_HEADERS == 1
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <memory.h>
#endif

#ifndef X86_64_MUXI_DCS6500_48Z8C_MALLOC
    #if defined(GLOBAL_MALLOC)
        #define X86_64_MUXI_DCS6500_48Z8C_MALLOC GLOBAL_MALLOC
    #elif X86_64_MUXI_DCS6500_48Z8C_CONFIG_PORTING_STDLIB == 1
        #define X86_64_MUXI_DCS6500_48Z8C_MALLOC malloc
    #else
        #error The macro X86_64_MUXI_DCS6500_48Z8C_MALLOC is required but cannot be defined.
    #endif
#endif

#ifndef X86_64_MUXI_DCS6500_48Z8C_FREE
    #if defined(GLOBAL_FREE)
        #define X86_64_MUXI_DCS6500_48Z8C_FREE GLOBAL_FREE
    #elif X86_64_MUXI_DCS6500_48Z8C_CONFIG_PORTING_STDLIB == 1
        #define X86_64_MUXI_DCS6500_48Z8C_FREE free
    #else
        #error The macro X86_64_MUXI_DCS6500_48Z8C_FREE is required but cannot be defined.
    #endif
#endif

#ifndef X86_64_MUXI_DCS6500_48Z8C_MEMSET
    #if defined(GLOBAL_MEMSET)
        #define X86_64_MUXI_DCS6500_48Z8C_MEMSET GLOBAL_MEMSET
    #elif X86_64_MUXI_DCS6500_48Z8C_CONFIG_PORTING_STDLIB == 1
        #define X86_64_MUXI_DCS6500_48Z8C_MEMSET memset
    #else
        #error The macro X86_64_MUXI_DCS6500_48Z8C_MEMSET is required but cannot be defined.
    #endif
#endif

#ifndef X86_64_MUXI_DCS6500_48Z8C_MEMCPY
    #if defined(GLOBAL_MEMCPY)
        #define X86_64_MUXI_DCS6500_48Z8C_MEMCPY GLOBAL_MEMCPY
    #elif X86_64_MUXI_DCS6500_48Z8C_CONFIG_PORTING_STDLIB == 1
        #define X86_64_MUXI_DCS6500_48Z8C_MEMCPY memcpy
    #else
        #error The macro X86_64_MUXI_DCS6500_48Z8C_MEMCPY is required but cannot be defined.
    #endif
#endif

#ifndef X86_64_MUXI_DCS6500_48Z8C_VSNPRINTF
    #if defined(GLOBAL_VSNPRINTF)
        #define X86_64_MUXI_DCS6500_48Z8C_VSNPRINTF GLOBAL_VSNPRINTF
    #elif X86_64_MUXI_DCS6500_48Z8C_CONFIG_PORTING_STDLIB == 1
        #define X86_64_MUXI_DCS6500_48Z8C_VSNPRINTF vsnprintf
    #else
        #error The macro X86_64_MUXI_DCS6500_48Z8C_VSNPRINTF is required but cannot be defined.
    #endif
#endif

#ifndef X86_64_MUXI_DCS6500_48Z8C_SNPRINTF
    #if defined(GLOBAL_SNPRINTF)
        #define X86_64_MUXI_DCS6500_48Z8C_SNPRINTF GLOBAL_SNPRINTF
    #elif X86_64_MUXI_DCS6500_48Z8C_CONFIG_PORTING_STDLIB == 1
        #define X86_64_MUXI_DCS6500_48Z8C_SNPRINTF snprintf
    #else
        #error The macro X86_64_MUXI_DCS6500_48Z8C_SNPRINTF is required but cannot be defined.
    #endif
#endif

#ifndef X86_64_MUXI_DCS6500_48Z8C_STRLEN
    #if defined(GLOBAL_STRLEN)
        #define X86_64_MUXI_DCS6500_48Z8C_STRLEN GLOBAL_STRLEN
    #elif X86_64_MUXI_DCS6500_48Z8C_CONFIG_PORTING_STDLIB == 1
        #define X86_64_MUXI_DCS6500_48Z8C_STRLEN strlen
    #else
        #error The macro X86_64_MUXI_DCS6500_48Z8C_STRLEN is required but cannot be defined.
    #endif
#endif

/* <auto.end.portingmacro(ALL).define> */


#endif /* __x86_64_muxi_dcs6500_48z8c_PORTING_H__ */
/* @} */
