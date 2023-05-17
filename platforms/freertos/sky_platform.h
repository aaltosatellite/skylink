#ifndef __SKY_PLATFORM_H__
#define __SKY_PLATFORM_H__

#include <stdint.h>
#include "FreeRTOS.h"
#include "SEGGER_RTT.h"

/* Timestamps in millisecond, 32 bits (wraps around every 49.71 days) */
typedef uint32_t sky_tick_t;


#define SKY_MALLOC(size)  pvPortMalloc(size)
#define SKY_FREE(ptr)     vPortFree(ptr)


/*
 *  Skylink's debug printf and assert macros
 */
#ifdef SKY_DEBUG

#define SKY_PRINTF(x, ...)                      \
    if ((sky_diag_mask & (x)) == (x))           \
        SEGGER_RTT_printf(0, __VA_ARGS__);

#define SKY_ASSERT(...) configASSERT(__VA_ARGS__);

#else

#define SKY_ASSERT(...)   do { } while(0);
#define SKY_PRINTF(...)   do { } while(0);

#endif /* SKY_DEBUG */



#endif /* __SKY_PLATFORM_H__ */
