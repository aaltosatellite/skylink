#ifndef __SKYLINK_DIAG_H__
#define __SKYLINK_DIAG_H__

#include <stdint.h>
#include "skylink/skylink.h"

/*
 * Diagnostics and debug tools
 */

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

/*
 * Structs to hold protocol diagnostic information
 */
typedef struct
{
	// Total number of retransmitted frames using this virtual channel. This is not actually updated anywhere.
	uint16_t arq_retransmits;
	//uint16_t arq_dunno;

	/*
	 * Total number of transmitted frames using this virtual channel.
	 */
	uint16_t tx_frames;

	/*
	 * Total number of received/accepted frames using this virtual channel.
	 * Counted frames must have passed HMAC and other sanity checks.
	 */
	uint16_t rx_frames;

} SkyVCDiagnostics;


struct sky_diag
{

	/*
	 * Total number of received frames.
	 * Counted frame has passed integrity checks.
	 */
	uint16_t rx_frames;

	/*
	 * Total number of received bytes.
	 */
	uint16_t rx_bytes;

	/*
	 * Number of successfully decoded codewords.
	 */
	uint16_t rx_fec_ok;

	/*
	 * Number of failed frame FEC decode.
	 * These frames won't be counted in the total "rx_frames" count.
	 */
	uint16_t rx_fec_fail;

	/*
	 * Total number of octets/bytes successfully decoded.
	 * Number includes both payload and parity bytes.
	 */
	uint16_t rx_fec_octs;

	/*
	 * Total number of octet/byte errors corrected.
	 * byte error rate = rx_fec_errs / rx_fec_octs
	 */
	uint16_t rx_fec_errs;

	/*
	 * Number of HMAC failures
	 */
	uint16_t rx_hmac_fail;

	/*
	 * Number of bytes errors corrected
	 */
	uint16_t rx_arq_resets;

	/*
	 * Total number of transmitted frames
	 */
	uint16_t tx_frames;

	/*
	 * Total Number of bytes transmitted
	 */
	uint16_t tx_bytes;

	/*
	 * Stats for individual virtual channels
	 */
	SkyVCDiagnostics vc_stats[SKY_NUM_VIRTUAL_CHANNELS];
};


/* Allocate and initialize a new diagnostics object */
SkyDiagnostics* sky_diag_create();

/* Destroy and free the diagnostics object */
void sky_diag_destroy(SkyDiagnostics* diag);

/* Clear the given diagnostics object */
void sky_diag_clear(SkyDiagnostics *diag);

/* Print out the link state to debug print. */
void sky_print_link_state(SkyHandle self);

/* ASCII color defines */
#define COLOR_BLACK   "\e[0;30m"
#define COLOR_RED     "\e[0;31m"
#define COLOR_GREE    "\e[0;32m"
#define COLOR_YELLOW  "\e[0;33m"
#define COLOR_BLUE    "\e[0;34m"
#define COLOR_MAGENTA "\e[0;35m"
#define COLOR_CYAN    "\e[0;36m"
#define COLOR_WHITE   "\e[0;37m"
#define COLOR_RESET   "\e[0m"

#endif /* __SKYLINK_DIAG_H__ */
