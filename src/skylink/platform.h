#ifndef __SKY_PLATFORM_H__
#define __SKY_PLATFORM_H__

#include <stdint.h>



#if defined(__unix__)


/**
 * UNIX/POSIX
 */



/* Timestamps in microsecond, 32 bits. (wraps around every 4295 seconds) */
typedef uint32_t timestamp_t;
typedef int32_t timediff_t;

timestamp_t get_timestamp();

#define TIMESTAMP_MS ((timestamp_t)1000)


#define SKY_BIG_ENDIAN

#define SKY_MALLOC malloc
#define SKY_CALLOC calloc
#define SKY_FREE free



#else


/**
 * FreeRTOS
 */

/* Timestamps in microsecond, 32 bits (wraps around every 4295 seconds) */
typedef uint32_t timestamp_t;
typedef int32_t timediff_t;

timestamp_t get_timestamp();

#define TIMESTAMP_MS ((timestamp_t)1000)


#define SKY_LITTLE_ENDIAN

#define SKY_MALLOC(s) pvMalloc(s)
#define SKY_CALLOC(s) pvMalloc(s)
#define SKY_FREE pvFree

#endif



#endif /* __SKY_PLATFORM_H__ */
