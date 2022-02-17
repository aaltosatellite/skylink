#ifndef __SKY_PLATFORM_H__
#define __SKY_PLATFORM_H__

#include <stdint.h>



#if defined(__unix__) // UNIX/POSIX
#include <time.h>


/* Tick is the time measurement primitive (often milliseconds), 32 bits. */
typedef int32_t tick_t;


//#include "utilities.h"
//#define SKY_MALLOC instrumented_malloc
#include <stdlib.h>
#define SKY_MALLOC malloc
#define SKY_FREE free



#else //FreeRTOS


/* Timestamps in microsecond, 32 bits (wraps around every 4295 seconds) */
typedef int32_t tick_t;

#define SKY_MALLOC pvPortMalloc
#define SKY_FREE pvPortFree


#endif



#endif /* __SKY_PLATFORM_H__ */
