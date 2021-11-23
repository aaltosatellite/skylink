#ifndef __SKY_PLATFORM_H__
#define __SKY_PLATFORM_H__

#include <stdint.h>



#if defined(__unix__) // UNIX/POSIX
#include <time.h>

/* Timestamps in microsecond, 32 bits. (wraps around every 4295 seconds) */
typedef int32_t timestamp_t;
typedef int32_t timediff_t;
typedef int32_t time_ms_t;

timestamp_t get_timestamp();

#define TIMESTAMP_MS ((timestamp_t)1000)

//#include "utilities.h"
//#define SKY_MALLOC instr_malloc
#define SKY_MALLOC malloc
#define SKY_FREE free



#else //FreeRTOS


/* Timestamps in microsecond, 32 bits (wraps around every 4295 seconds) */
typedef uint32_t timestamp_t;
typedef int32_t timediff_t;

timestamp_t get_timestamp();

#define TIMESTAMP_MS ((timestamp_t)1000)

#define SKY_MALLOC pvMalloc
#define SKY_FREE pvFree


#endif



#endif /* __SKY_PLATFORM_H__ */
