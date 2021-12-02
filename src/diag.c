
#include "skylink/skylink.h"
#include "skylink/diag.h"
#include "skylink/reliable_vc.h"

unsigned int sky_diag_mask;

SkyDiagnostics* new_diagnostics(){
	SkyDiagnostics* diag = SKY_MALLOC(sizeof(SkyDiagnostics));
	memset(diag, 0, sizeof(SkyDiagnostics));
	return diag;
}

void destroy_diagnostics(SkyDiagnostics* diag){
	SKY_FREE(diag);
}



void sky_print_link_state(SkyHandle self) {

	if ((sky_diag_mask & SKY_DIAG_LINK_STATE) == 0)
		return;

	SkyDiagnostics* diag = self->diag;

	SKY_PRINTF(SKY_DIAG_LINK_STATE, "\033[H\033[2J");

	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Received frames: %5u total, %5u OK, %5u failed. FEC corrected octets %5u/%u\n",
		diag->rx_frames, diag->rx_fec_ok, diag->rx_fec_fail, diag->rx_fec_errs, diag->rx_fec_octs);
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Transmit frames: %5u\n",
		diag->tx_frames);


	for (int vc = 0; vc < 4; vc++) {
		SkyVirtualChannel* ring = self->virtualChannels[vc];

		SKY_PRINTF(SKY_DIAG_LINK_STATE, "VC#%d   ARQ: ", vc);


		// ARQ state
		switch (ring->arq_state_flag) {
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
			SKY_PRINTF(SKY_DIAG_LINK_STATE, "????");
		}

		SKY_PRINTF(SKY_DIAG_LINK_STATE, " %d  %d ", ring->arq_state_flag, ring->handshake_send);


		SKY_PRINTF(SKY_DIAG_LINK_STATE, "TX: %d  RX: %d",
				   sky_vc_count_packets_to_tx(ring, 1),
				   sky_vc_count_readable_rcv_packets(ring));

		SKY_PRINTF(SKY_DIAG_LINK_STATE, "\n");

	}



}
