/******************************
******SKYLINK DIAGNOSTICS******
******************************/
#include "skylink/skylink.h"
#include "skylink/diag.h"
#include "skylink/reliable_vc.h"

#include <string.h> // memset

unsigned int sky_diag_mask = SKY_DIAG_INFO | SKY_DIAG_BUG;

//Allocate a new diagnostics struct and zero it.
SkyDiagnostics* sky_diag_create(){
	SkyDiagnostics* diag = SKY_MALLOC(sizeof(SkyDiagnostics));
	SKY_ASSERT(diag != NULL);
	memset(diag, 0, sizeof(SkyDiagnostics));
	return diag;
}

//Free a diagnostics struct.
void sky_diag_destroy(SkyDiagnostics* diag){
	SKY_FREE(diag);
}

//Set the memory of a diagnostics struct to zero.
void sky_diag_clear(SkyDiagnostics* diag) {
	memset(diag, 0, sizeof(SkyDiagnostics));
}

//Print the diagnostics struct to stdout.
void sky_print_link_state(SkyHandle self)
{
#ifndef SKY_DEBUG
	(void)self;
#else
	if ((sky_diag_mask & SKY_DIAG_LINK_STATE) == 0) return;

	//Get the diagnostics struct from the SkyHandle.
	const SkyDiagnostics* diag = self->diag;

	// Clear screen
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "\033[H\033[2J");

	// Print information about frames.
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Received frames: %5u total, %5u OK, %5u failed. FEC corrected octets %5u/%u\n",
		diag->rx_frames, diag->rx_fec_ok, diag->rx_fec_fail, diag->rx_fec_errs, diag->rx_fec_octs);
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Transmit frames: %5u\n",
		diag->tx_frames);


	// Print information about virtual channels.
	for (int vc_i = 0; vc_i < 4; vc_i++) {
		SkyVirtualChannel* vc = self->virtual_channels[vc_i];

		//Current virtual channel
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "VC#%d   ARQ: ", vc_i);


		// ARQ state
		switch (vc->arq_state_flag) {
		case ARQ_STATE_OFF:
			SKY_PRINTF(SKY_DIAG_LINK_STATE, "OFF ");
			break;

		case ARQ_STATE_IN_INIT:
			SKY_PRINTF(SKY_DIAG_LINK_STATE, "INIT");
			break;

		case ARQ_STATE_ON:
			SKY_PRINTF(SKY_DIAG_LINK_STATE, "ON  ");
			break;

		default:
			SKY_PRINTF(SKY_DIAG_LINK_STATE, "????"); //Unknown state
		}

		//SKY_PRINTF(SKY_DIAG_LINK_STATE, " %d  %d ", vc->arq_state_flag, vc->handshake_send);


		// Print the amount of packets to transmit and receive.
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "TX: %d  RX: %d",
				   sky_vc_count_packets_to_tx(vc, 1),
				   sky_vc_count_readable_rcv_packets(vc));

		SKY_PRINTF(SKY_DIAG_LINK_STATE, "\n");

	}

#endif /* SKY_DEBUG */
}
