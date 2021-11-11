
#include <zmq.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


#include "suo.h"
#include "skylink/skylink.h"
#include "skylink/platform.h"
#include "skylink/hmac.h"
#include "vcs.h"
#include "modem.h"


#define debugprintf(...) do { } while(0)
#define diagprintf(...) printf(__VA_ARGS__)



const uint8_t hmac_key[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };



void *zmq = NULL;

int main(int argc, char* argv[]) __attribute__((noreturn));

int main(int argc, char *argv[])
{
	// How many nanosecond beforehand transmit frames should be given
	const uint64_t tx_ahead_time = /*20000*/ 0;


	/* ------------------
	 * Read args
	 * ------------------ */
	int modem_base = 43700, vc_base = 52000;
	if (argc >= 4) {
		modem_base = atoi(argv[2]);
		vc_base = atoi(argv[3]);
	}


	/*
	 * Open ZeroMQ sockets interface
	 */
	zmq = zmq_ctx_new();
	modem_init(modem_base);
	vc_init(vc_base);


	/* -------------------------
	 * Initialize protocol
	 * ------------------------- */

	sky_diag_mask = 0xFFFF;
	/*SKY_DIAG_INFO  |
	SKY_DIAG_DEBUG |
	SKY_DIAG_BUG | \
	SKY_DIAG_LINK_STATE | \
	SKY_DIAG_FRAMES |
	SKY_DIAG_BUFFER*/

	/*
	* PHY configurations
	*/
	SkyConfig* config = SKY_MALLOC(sizeof(SkyConfig));;
	config->identity[0] = 'O';
	config->identity[1] = 'H';
	config->identity[2] = 'F';
	config->identity[3] = 'S';
	config->identity[4] = '1';

	config->vc_priority[0] = 0;
	config->vc_priority[1] = 1;
	config->vc_priority[2] = 2;
	config->vc_priority[3] = 3;

	/*
	 * MAC configurations
	 */
	config->mac.maximum_gap_length           = 1000;
	config->mac.minimum_gap_length           = 50;
	config->mac.default_gap_length           = 600;

	config->mac.maximum_window_length        = 350;
	config->mac.minimum_window_length        = 25;
	config->mac.default_window_length        = 220;

	config->mac.default_tail_length          = 86;
	config->mac.unauthenticated_mac_updates  = 0;

	/*
	 * ARQ configurations
	 */
	config->vc[0].arq_on = 1;
	config->vc[0].require_authentication     = 0;
	config->vc[1].arq_on = 1;
	config->vc[1].require_authentication     = 0;
	config->vc[2].arq_on = 0;
	config->vc[2].require_authentication     = 0;
	config->vc[3].arq_on = 0;
	config->vc[3].require_authentication     = 0;

	config->array[0].n_recall                = 16;
	config->array[0].horizon_width           = 16;
	config->array[0].send_ring_len           = 24;
	config->array[0].rcv_ring_len            = 24;
	config->array[0].element_count           = 3600;
	config->array[0].element_size            = 36;
	config->array[0].initial_send_sequence   = 0;
	config->array[0].initial_rcv_sequence    = 0;

	config->array[1].n_recall                = 16;
	config->array[1].horizon_width           = 16;
	config->array[1].send_ring_len           = 24;
	config->array[1].rcv_ring_len            = 24;
	config->array[1].element_count           = 3600;
	config->array[1].element_size            = 36;
	config->array[1].initial_send_sequence   = 0;
	config->array[1].initial_rcv_sequence    = 0;

	config->array[2].n_recall                = 0;
	config->array[2].horizon_width           = 0;
	config->array[2].send_ring_len           = 8;
	config->array[2].rcv_ring_len            = 8;
	config->array[2].element_count           = 800;
	config->array[2].element_size            = 36;
	config->array[2].initial_send_sequence   = 0;
	config->array[2].initial_rcv_sequence    = 0;

	config->array[3].n_recall                = 0;
	config->array[3].horizon_width           = 0;
	config->array[3].send_ring_len           = 8;
	config->array[3].rcv_ring_len            = 8;
	config->array[3].element_count           = 800;
	config->array[3].element_size            = 36;
	config->array[3].initial_send_sequence   = 0;
	config->array[3].initial_rcv_sequence    = 0;

	/*
	 * HMAC configuration
	 */
	config->hmac.key_length                 = sizeof(hmac_key);
	config->hmac.maximum_jump               = 24;
	memcpy(config->hmac.key, hmac_key, config->hmac.key_length);


	// Kick the actual protocol implementation running
	SkyHandle handle = SKY_MALLOC(sizeof(struct sky_all));
	handle->conf = config;
	handle->mac = new_mac_system(&config->mac);
	handle->hmac = new_hmac_instance(&config->hmac);
	handle->diag = new_diagnostics();
	handle->phy = new_physical();
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		handle->arrayBuffers[i] = new_arq_ring(&config->array[i]);
	}


	/* Wait for the first timing message */
	modem_wait_for_sync();

	int ret;
	SkyRadioFrame frame;


	SKY_PRINTF(SKY_DIAG_DEBUG, "Running...\n");

	/* ----------------
	 * Run the protocol
	 * ---------------- */
	for (;;) {
		/*
		 * Receive frames from the modem interface
		 */
		if (modem_rx(&frame, 0) > 0) {

			//sky_mac_carrier_sensed(frame.timestamp);
			sky_rx_raw(handle, &frame);

		}

		/*
		 * Check VC sockets
		 */
		vc_check();

		/*
		 * If we have received tick message run the Skylink TX routine
		 */
		if (tick()) {
			uint64_t t = get_timestamp() + tx_ahead_time;
			if (modem_tx_active() == 0 && sky_tx(handle, &frame, 0, t)) {
				modem_tx(&frame, t);

				// Clear the frame and re-use the memory
				memset(&frame, 0, sizeof(frame));
			}

			//sky_print_diag(sky);
		}

	}
}
