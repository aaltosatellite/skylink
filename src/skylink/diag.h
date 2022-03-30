#ifndef __SKYLINK_DIAG_H__
#define __SKYLINK_DIAG_H__

#include <stdint.h>
#include "skylink.h"
/*
 * Diagnostics and debug tools
 */

// TODO: Defines which prints are enabled

#define SKY_DIAG_INFO       0x0001
#define SKY_DIAG_DEBUG      0x0002
#define SKY_DIAG_BUG        0x0004
#define SKY_DIAG_LINK_STATE 0x0008
#define SKY_DIAG_FEC        0x0010
#define SKY_DIAG_MAC        0x0020
#define SKY_DIAG_HMAC       0x0040
#define SKY_DIAG_ARQ        0x0080
#define SKY_DIAG_FRAMES     0x0100
#define SKY_DIAG_BUFFER     0x0200

/* Global define for debug print flags */
extern unsigned int sky_diag_mask;

//#define SKY_DEBUG


#ifdef SKY_DEBUG
// --- DEBUG in UNIX environment ------------------------------------------------------------------
#ifdef __unix__
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
/* Assert and printf for POSIX platforms */
#define SKY_PRINTF(x, ...) if ((sky_diag_mask & (x)) == (x)) { fprintf(stderr, __VA_ARGS__); fflush(stderr); }
#define SKY_ASSERT(...)   assert(__VA_ARGS__);
// ------------------------------------------------------------------------------------------------


// --- DEBUG outside UNIX environment -------------------------------------------------------------
#else
#ifndef SKY_ASSERT
#define SKY_ASSERT(...)   do { } while(0);
#endif
#ifndef SKY_PRINTF
#define SKY_PRINTF(...)    do { } while(0);
#endif
#endif //__unix__
// ------------------------------------------------------------------------------------------------


// --- no DEBUG -----------------------------------------------------------------------------------
#else
#define SKY_ASSERT(...)   do { } while(0);
#define SKY_PRINTF(...)   do { } while(0);
// ------------------------------------------------------------------------------------------------
#endif //SKY_DEBUG



/*
 * Protocol diagnostic information.
 */
struct sky_diag {
	uint16_t rx_frames;      // Total number of received frames
	uint16_t rx_fec_ok;      // Number of successfully decoded codewords
	uint16_t rx_fec_fail;    // Number of failed decodes
	uint16_t rx_fec_octs;    // Total number of octets successfully decoded
	uint16_t rx_fec_errs;    // Number of bytes errors corrected
	uint16_t rx_arq_resets;  // Number of bytes errors corrected
	uint16_t tx_frames;      // Total number of transmitted frames
	uint16_t tx_bytes;       // Number of bytes transmitted
};
typedef struct sky_diag SkyDiagnostics;

SkyDiagnostics* new_diagnostics();

void destroy_diagnostics(SkyDiagnostics* diag);

void sky_print_link_state(SkyHandle self);

#endif /* __SKYLINK_DIAG_H__ */
