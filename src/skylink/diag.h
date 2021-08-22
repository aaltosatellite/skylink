#ifndef __SKYLINK_DIAG_H__
#define __SKYLINK_DIAG_H__


/*
 * Diagnostics and debug tools
 */

// TODO: Defines which prints are enabled

#define SKY_DIAG_INFO       0x0001
#define SKY_DIAG_DEBUG      0x0002
#define SKY_DIAG_BUG        0x0004
#define SKY_DIAG_LINK_STATE 0x0008
#define SKY_DIAG_FRAMES     0x0100
#define SKY_DIAG_BUFFER     0x0200


#ifdef DEBUG

#ifdef __unix__
#include <assert.h>
/* Assert for POSIX platforms */
#define SKY_ASSERT(...)   assert(__VA_ARGS__);
#else
/* Assert for embedded platforms */
#define SKY_ASSERT(...)    if ((__VA_ARGS__) != 0) while(1);
#endif

#else
/* No asserts  in release build */
#define SKY_ASSERT(...)   do { } while(0)
#endif


/* Global define for debug print flags */
extern unsigned int sky_diag_mask;


#ifdef DEBUG

#ifdef __unix__
#include <stdio.h>
/* printf for POSIX platforms */
#define SKY_PRINTF(x, ...) if ((sky_diag_mask & (x)) != 0) { fprintf(stderr, __VA_ARGS__); }
#else
/* printf for embedded platforms */
#include "SEGGER_RTT.h"
#define SKY_PRINTF(x, ...) if ((sky_diag_mask & (x)) != 0) { SEGGER_RTT_printf(0, __VA_ARGS__); }
#endif

#else
/* No debug prints in release build */
#define SKY_PRINTF(...) do { } while(0)
#endif


/**/
void sky_diag_dump_hex(uint8_t* data, unsigned int len);

#endif /* __SKYLINK_DIAG_H__ */
