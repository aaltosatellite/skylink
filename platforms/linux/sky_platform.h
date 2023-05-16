#ifndef __SKY_PLATFORM_H__
#define __SKY_PLATFORM_H__

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>


/* Tick is the time measurement primitive (often milliseconds), 32 bits. */
typedef int32_t sky_tick_t;

#define SKY_MALLOC malloc
#define SKY_FREE free


/*
 *  Skylink's debug printf and assert macros
 */
#ifdef SKY_DEBUG

/* Assert and printf for POSIX platforms */
#define SKY_PRINTF(x, ...)            \
    if ((sky_diag_mask & (x)) == (x)) \
    {                                 \
        fprintf(stderr, __VA_ARGS__); \
        fflush(stderr);               \
    }
#define SKY_ASSERT(...) assert(__VA_ARGS__);


#else

#define SKY_ASSERT(...)   do { } while(0);
#define SKY_PRINTF(...)   do { } while(0);

#endif /* SKY_DEBUG */


#endif /* __SKY_PLATFORM_H__ */
