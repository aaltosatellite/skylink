
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <zmq.h>

#include "skylink/skylink.h"
#include "skylink/reliable_vc.h"
#include "skylink/diag.h"
#include "skylink/hmac.h"
#include "skylink/platform.h"
#include "skylink/hmac.h"
#include "skylink/mac.h"
#include "skylink/utilities.h"

#include "vc_interface.h"
#include "modem.h"
#include "../platforms/posix/timestamp.h"


#ifdef EXTERNAL_SECRET
extern const uint8_t hmac_key[];
extern const unsigned int hmac_key_len;
#else
const uint8_t hmac_key[] = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };
const unsigned int hmac_key_len = sizeof(hmac_key);
#endif


SkyHandle handle;

void *zmq = NULL;



void read_sequence_numbers_from_file(int32_t* sequences) {
	int fd = open("sequences", O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "No 'sequences' file found! Starting from zero.\n");
		memset(sequences, 0, SKY_NUM_VIRTUAL_CHANNELS * sizeof(int32_t));
		return;
	}
	if (read(fd, sequences, SKY_NUM_VIRTUAL_CHANNELS * sizeof(int32_t)) != SKY_NUM_VIRTUAL_CHANNELS * sizeof(int32_t)) {
		fprintf(stderr, "Broken 'sequences' file! Starting from zero.\n");
		memset(sequences, 0, SKY_NUM_VIRTUAL_CHANNELS * sizeof(int32_t));
	}

	close(fd);
}

void write_sequence_numbers_to_file(const int32_t* sequences) {
	int fd = open("sequences", O_RDONLY);
	if (fd < 1) {
		fprintf(stderr, "Failed to open 'sequences' file for writing!\n");
		return;
	}
	if (write(fd, sequences, SKY_NUM_VIRTUAL_CHANNELS * sizeof(int32_t)) != SKY_NUM_VIRTUAL_CHANNELS * sizeof(int32_t)) {
		fprintf(stderr, "Failed to write 'sequences' file!\n");
		return;
	}
	close(fd);
}



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

	sky_diag_mask |= SKY_DIAG_ARQ; // | SKY_DIAG_INFO  | SKY_DIAG_BUG | SKY_DIAG_BUFFER;
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

	SKY_ASSERT(SKY_IDENTITY_LEN == 6);
	if (mimic_satellite == 0) {
		config->identity[0] = 'O';
		config->identity[1] = 'H';
		config->identity[2] = '2';
		config->identity[3] = 'A';
		config->identity[4] = 'G';
		config->identity[5] = 'S';
	}
	else {
		config->identity[0] = 'O';
		config->identity[1] = 'H';
		config->identity[2] = '2';
		config->identity[3] = 'F';
		config->identity[4] = '1';
		config->identity[5] = 'S';
	}


	/*
	 * MAC configurations
	 */
	config->mac.gap_constant_ticks              = 1000;
	config->mac.tail_constant_ticks             = 60;
	config->mac.maximum_window_length_ticks     = 5000;
	config->mac.minimum_window_length_ticks     = 250;
	config->mac.window_adjust_increment_ticks   = 250;
	config->mac.window_adjustment_period        = 2;
	config->mac.unauthenticated_mac_updates     = 0;
	config->mac.shift_threshold_ticks           = 10000;
	config->mac.idle_frames_per_window          = 0;
	config->mac.idle_timeout_ticks              = 30000;
	config->mac.carrier_sense_ticks             = 200;

	/*
	 * Virtual channel configurations
	 */
	config->vc[0].horizon_width             = 16;
	config->vc[0].send_ring_len             = 24;
	config->vc[0].rcv_ring_len              = 24;
	config->vc[0].element_size              = 36;
	config->vc[0].require_authentication    = SKY_VC_FLAG_REQUIRE_AUTHENTICATION | SKY_VC_FLAG_AUTHENTICATE_TX;

	config->vc[1].horizon_width             = 16;
	config->vc[1].send_ring_len             = 24;
	config->vc[1].rcv_ring_len              = 24;
	config->vc[1].element_size              = 36;
	config->vc[1].require_authentication    = SKY_VC_FLAG_REQUIRE_AUTHENTICATION | SKY_VC_FLAG_AUTHENTICATE_TX;

	config->vc[2].horizon_width             = 2;
	config->vc[2].send_ring_len             = 8;
	config->vc[2].rcv_ring_len              = 8;
	config->vc[2].element_size              = 36;
	config->vc[2].require_authentication    = SKY_VC_FLAG_REQUIRE_AUTHENTICATION | SKY_VC_FLAG_AUTHENTICATE_TX;

	config->vc[3].horizon_width             = 2;
	config->vc[3].send_ring_len             = 8;
	config->vc[3].rcv_ring_len              = 8;
	config->vc[3].element_size              = 36;
	config->vc[3].require_authentication    = 0;

	if (mimic_satellite) {
		config->vc[0].require_authentication = SKY_VC_FLAG_REQUIRE_AUTHENTICATION | SKY_VC_FLAG_REQUIRE_SEQUENCE | SKY_VC_FLAG_AUTHENTICATE_TX;
		config->vc[1].require_authentication = SKY_VC_FLAG_REQUIRE_AUTHENTICATION | SKY_VC_FLAG_REQUIRE_SEQUENCE | SKY_VC_FLAG_AUTHENTICATE_TX;
		config->vc[2].require_authentication = SKY_VC_FLAG_REQUIRE_AUTHENTICATION | SKY_VC_FLAG_AUTHENTICATE_TX;
		config->vc[3].require_authentication = 0;
	}

	config->arq_timeout_ticks               = 12000; // [ticks]
	config->arq_idle_frames_per_window      = 1;

	/*
	 * HMAC configuration
	 */
	config->hmac.key_length                 = hmac_key_len;
	config->hmac.maximum_jump               = 24;
	memcpy(config->hmac.key, hmac_key, hmac_key_len);


	// Kick the actual protocol implementation running
	handle = SKY_MALLOC(sizeof(struct sky_all));
	handle->conf = config;
	handle->mac = sky_mac_create(&config->mac);
	handle->hmac = new_hmac_instance(&config->hmac);
	handle->diag = new_diagnostics();
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		handle->virtual_channels[i] = new_virtual_channel(&config->vc[i]);
		if (handle->virtual_channels[i] == NULL) {
			fprintf(stderr, "Failed to create virtual channel %d", i);
			return 1;
		}
	}


	/*
	 * Load sequence numbers
	 */
	int32_t sequences[SKY_NUM_VIRTUAL_CHANNELS];
	read_sequence_numbers_from_file(sequences);
	sky_hmac_load_sequences(handle, sequences);

	/* Wait for the first timing message */
	modem_wait_for_sync();
	tick_t last_sequences_stored = get_timestamp();
	int txed = 0;


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
			int ret = sky_rx(handle, &frame, 0);
			if (ret < 0)
				SKY_PRINTF(SKY_DIAG_BUG, "sky_rx() error %d\n", ret);
			//sky_frame_clear(&frame);
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
				sky_mac_carrier_sensed(handle->mac, sky_get_tick_time());

		 	if (modem_can_send()) {
				uint64_t t = get_timestamp() + tx_ahead_time;

				int ret = sky_tx(handle, &frame, 0);
				if (ret < 0)
					SKY_PRINTF(SKY_DIAG_BUG, "sky_tx() error %d\n", ret);
				if (ret == 1) {
					modem_tx(&frame, t);
					memset(&frame, 0, sizeof(frame));
					txed = 1;
				}
			}

			// Print diagnostics
			static int d = 0;
			if (++d > 40) {
				sky_print_link_state(handle);
				d = 0;
			}

			// Store HMAC values on the disk every 1 second
			if (txed && get_timestamp() - last_sequences_stored > 1000) {
				last_sequences_stored = get_timestamp();
				txed = 0;

				sky_hmac_dump_sequences(handle, sequences);
				write_sequence_numbers_to_file(sequences);
			}
		}

	}

	return 0;
}
