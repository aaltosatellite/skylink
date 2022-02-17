#ifndef __FS_LL_TIMESTAMP_H__
#define __FS_LL_TIMESTAMP_H__

#include <stdint.h>

/* Timestamps in microsecond, 32 bits.
 * (wraps around every 4295 seconds) */
typedef int32_t timestamp_t;
typedef int32_t timediff_t;

timestamp_t get_timestamp();

#define TIMESTAMP_MS ((timestamp_t)1000)

#endif
