#ifndef __SKYLINK_DIAG_H__
#define __SKYLINK_DIAG_H__

#include <stdint.h>

/*
 * Diagnostics and debug tools
 */

// TODO: Defines which prints are enabled

#define SKY_DIAG_INFO       0x0001
#define SKY_DIAG_DEBUG      0x0002
#define SKY_DIAG_BUG        0x0004
#define SKY_DIAG_LINK_STATE 0x0008
#define SKY_DIAG_FEC        0x0008
#define SKY_DIAG_MAC        0x0010
#define SKY_DIAG_ARQ        0x0020
#define SKY_DIAG_FRAMES     0x0100
#define SKY_DIAG_BUFFER     0x0200


#define DEBUG
#ifdef DEBUG

#ifdef __unix__
#include <assert.h>
/* Assert for POSIX platforms */

#define SKY_ASSERT(...)   assert(__VA_ARGS__);
#else
/* Assert for embedded platforms */
#define SKY_ASSERT(...)    if ((__VA_ARGS__) != 0) while(1);
#endif //__unix__

#else //DEBUG
/* No asserts  in release build */
#define SKY_ASSERT(...)   do { } while(0)
#endif //DEBUG


/* Global define for debug print flags */
extern unsigned int sky_diag_mask;

#define DEBUG
#ifdef DEBUG

#ifdef __unix__
#include <stdio.h>
#include <stdint.h>
/* printf for POSIX platforms */
#define SKY_PRINTF(x, ...) if ((sky_diag_mask & (x)) != 0) { fprintf(stderr, __VA_ARGS__); fflush(stderr); }
#else
/* printf for embedded platforms */
#include "SEGGER_RTT.h"
#define SKY_PRINTF(x, ...) if ((sky_diag_mask & (x)) != 0) { SEGGER_RTT_printf(0, __VA_ARGS__); }
#endif //__unix__

#else
/* No debug prints in release build */
#define SKY_PRINTF(...) do { } while(0)
#endif //DEBUG


/**/
void sky_diag_dump_hex(uint8_t* data, unsigned int len);



/*
 * Protocol diagnostic information.
 */
typedef struct sky_diag {
	uint16_t rx_frames;      // Total number of received frames
	uint16_t rx_fec_ok;      // Number of successfully decoded codewords
	uint16_t rx_fec_fail;    // Number of failed decodes
	uint16_t rx_fec_octs;    // Total number of octets successfully decoded
	uint16_t rx_fec_errs;    // Number of octet errors corrected
	uint16_t rx_arq_resets;  // Number of octet errors corrected
	uint16_t tx_frames;      // Total number of transmitted frames
} SkyDiagnostics;

SkyDiagnostics* new_diagnostics();

void destroy_diagnostics(SkyDiagnostics* diag);

#endif /* __SKYLINK_DIAG_H__ */
