
#include <zmq.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "skylink/skylink.h"
#include "skylink/reliable_vc.h"
#include "skylink/diag.h"
#include "skylink/platform.h"
#include "skylink/hmac.h"
#include "skylink/mac.h"
#include "skylink/utilities.h"

#include "vc_interface.h"
#include "modem.h"



const uint8_t hmac_key[8] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };

SkyHandle handle;

void *zmq = NULL;


int main(int argc, char *argv[])
{
	// How many nanosecond beforehand transmit frames should be given
	const uint64_t tx_ahead_time = /*20000*/ 0;


	/* ------------------
	 * Read args
	 * ------------------ */
	int modem_base = 4000, vc_base = 5000;
	int mimic_satellite = 0;
	int show_link_state = 0;

	int opt;
	while ((opt = getopt(argc, argv, "m:c:xs")) != -1) {
		switch (opt) {
		case 'm':
			modem_base = atoi(optarg);
			break;

		case 'c':
			vc_base = atoi(optarg);
			break;

		case 'x':
			mimic_satellite = 1;
			break;

		case 's':
			show_link_state = 1;
			break;

		}
	}

	fprintf(stderr, "modem_base: %d  vc_base: %d\n", modem_base, vc_base);
	if(mimic_satellite){
		printf("role: satellite\n");
	} else {
		printf("role: ground station\n");
	}

	/*
	 * Open ZeroMQ sockets interface
	 */
	zmq = zmq_ctx_new();
	modem_init(modem_base);
	vc_init(vc_base, false);


	/* -------------------------
	 * Initialize protocol
	 * ------------------------- */

	//sky_diag_mask = 0xffff; // | SKY_DIAG_INFO  | SKY_DIAG_BUG | SKY_DIAG_BUFFER;
	if (show_link_state)
		sky_diag_mask = SKY_DIAG_INFO | SKY_DIAG_LINK_STATE;
	else
		sky_diag_mask &= ~SKY_DIAG_LINK_STATE;

	SkyConfig* config = SKY_MALLOC(sizeof(SkyConfig));

	/*
	* PHY configurations
	*/
	config->phy.enable_scrambler = 1;
	config->phy.enable_rs = 1;
	config->phy.authenticate_tx = 1;

	if (mimic_satellite == 0) {
		config->identity[0] = 'O';
		config->identity[1] = 'H';
		config->identity[2] = 'A';
		config->identity[3] = 'G';
		config->identity[4] = 'S';
	}
	else {
		config->identity[0] = 'O';
		config->identity[1] = 'H';
		config->identity[2] = 'F';
		config->identity[3] = 'S';
		config->identity[4] = '1';
	}


	/*
	 * MAC configurations
	 */
	config->mac.default_gap_length           = 800; // [ms]
	config->mac.default_tail_length          = 100; // [ms]

	config->mac.maximum_window_length        = 350; // [ms]
	config->mac.minimum_window_length        = 250; // [ms]
	config->mac.default_window_length        = 280; // [ms]

	config->mac.default_tail_length          = 86;  // [ms]
	config->mac.unauthenticated_mac_updates  = 0;
	config->mac.shift_threshold_ms           = 4000; // [ms]

	/*
	 * Virtual channel configurations
	 */
	config->vc[0].horizon_width           = 16;
	config->vc[0].send_ring_len           = 24;
	config->vc[0].rcv_ring_len            = 24;
	config->vc[0].element_size            = 36;
	config->vc[0].require_authentication  = 1;

	config->vc[1].horizon_width           = 16;
	config->vc[1].send_ring_len           = 24;
	config->vc[1].rcv_ring_len            = 24;
	config->vc[1].element_size            = 36;
	config->vc[1].require_authentication  = 1;

	config->vc[2].horizon_width           = 2;
	config->vc[2].send_ring_len           = 8;
	config->vc[2].rcv_ring_len            = 8;
	config->vc[2].element_size            = 36;
	config->vc[2].require_authentication  = 0;

	config->vc[3].horizon_width           = 2;
	config->vc[3].send_ring_len           = 8;
	config->vc[3].rcv_ring_len            = 8;
	config->vc[3].element_size            = 36;
	config->vc[3].require_authentication  = 0;

	config->arq_timeout_ms                = 12000; // [ms]

	/*
	 * HMAC configuration
	 */
	config->hmac.key_length                 = sizeof(hmac_key);
	config->hmac.maximum_jump               = 24;
	memcpy(config->hmac.key, hmac_key, config->hmac.key_length);


	// Kick the actual protocol implementation running
	handle = SKY_MALLOC(sizeof(struct sky_all));
	handle->conf = config;
	handle->mac = sky_mac_create(&config->mac);
	handle->hmac = new_hmac_instance(&config->hmac);
	handle->diag = new_diagnostics();
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		handle->virtual_channels[i] = new_arq_ring(&config->vc[i]);
		if (handle->virtual_channels[i] == NULL) {
			fprintf(stderr, "Failed to create virtual channel %d", i);
			return 1;
		}
	}

	/* Wait for the first timing message */
	modem_wait_for_sync();

	SkyRadioFrame frame;
	SKY_PRINTF(SKY_DIAG_DEBUG, "Running...\n");

	/* ----------------
	 * Run the protocol
	 * ---------------- */
	for (;;) {
		/*
		 * Receive frames from the modem interface (blocking)
		 */
		if (modem_rx(&frame, 0) > 0) {
			sky_rx(handle, &frame, 0);
			//if (ret < 0)
			//	SKY_PRINTF(SKY_DIAG_BUG, "sky_rx() error %d\n", ret);
			//sky_frame_clear(frame);
		}

		/*
		 * Check VC sockets (non-blocking)
		 */
		vc_check_arq_states();
		vc_check_sys_to_rf();
		vc_check_rf_to_sys();

		/*
		 * If we have received tick message run the Skylink TX routine
		 */
		timestamp_t time_ms = get_timestamp();
		if (sky_tick(time_ms)) {


			if (modem_carrier_sensed())
				sky_mac_carrier_sensed(handle->mac, &handle->conf->mac, get_timestamp());

		 	if (modem_tx_active() == 0) {
				uint64_t t = get_timestamp() + tx_ahead_time;

				int ret = sky_tx(handle, &frame, 0);
				if (ret < 0)
					SKY_PRINTF(SKY_DIAG_BUG, "sky_tx() error %d\n", ret);
				if (ret == 1) {
					modem_tx(&frame, t);
					memset(&frame, 0, sizeof(frame));
				}

				// Print diagnostics
				static int d = 0;
				if (++d > 20) {
					sky_print_link_state(handle);
					d = 0;
				}
			}

		}

	}

	return 0;
}
