#ifndef __SKY_PLATFORM_H__
#define __SKY_PLATFORM_H__


#include <stdint.h>
#include "arena.h"
#include "segger_rtt.h"


/* Timestamps in millisecond, 32 bits (wraps around every 4295 seconds) */
typedef int32_t tick_t;

#define SKY_MALLOC(size)  arena_alloc(size)
#define SKY_FREE(ptr)     arena_free(ptr)


/*
 *  Skylink's debug printf and assert macros
 */
#ifdef SKY_DEBUG

#define SKY_PRINTF(x, ...)                      \
    if ((sky_diag_mask & (x)) == (x))           \
        SEGGER_RTT_printf(0, __VA_ARGS__);

#define SKY_ASSERT(...) assert(__VA_ARGS__);


#else

#define SKY_ASSERT(...)   do { } while(0);
#define SKY_PRINTF(...)   do { } while(0);

#endif /* SKY_DEBUG */



#endif /* __SKY_PLATFORM_H__ */
