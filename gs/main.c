
#include <zmq.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


#include "suo.h"
#include "../skylink/skylink.h"
#include "../skylink/platform.h"
#include "../skylink/hmac.h"
#include "vcs.h"
#include "modem.h"


#define debugprintf(...) do { } while(0)
#define diagprintf(...) printf(__VA_ARGS__)



void *zmq = NULL;

struct sky_all sky_;
SkyHandle sky = &sky_;
SkyConfig_t sky_config;

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
	sky_config.phy.enable_rs = 1;
	sky_config.phy.enable_scrambler = 1;

	/*xbuf[vc] = sky_buf_init(0x4000);
	* MAC configurations
	*/
	sky_config.mac.min_slots = 4;
	sky_config.mac.max_slots = 16;
	sky_config.mac.switching_delay = 3; // [ms]
	sky_config.mac.windows_adjust_interval = 2000; // [ms]


	const uint8_t hmac_key[] = "PASSWORD123";


	// Reserve buffers
	for (int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		//sky->arrayBuffers =
		//sky->txbuf[vc] = sky_buf_init(0x4000);
	}

	// Kick the actual protocol implementation running
	sky = sky_init(sky, &sky_config);
	sky_hmac_init(sky, hmac_key, sizeof(hmac_key));

	/* Wait for the first timing message */
	// modem_wait_for_sync();

	int ret;
	SkyRadioFrame frame;


	SKY_PRINTF(SKY_DIAG_DEBUG, "Running...\n");

	/* ----------------
	 * Run the protocol
	 * ---------------- */
	int loopcount = 0;

	for (;;) {
		loopcount++;
		if((loopcount % 256) == 0){
			printf("[loop: %d]\n", loopcount);
			fflush(stdout);
		}
		/*
		 * Receive frames from the modem interface
		 */
		if (modem_rx(&frame, 0) > 0) {

			//sky_mac_carrier_sensed(frame.timestamp);
			sky_rx_raw(sky, &frame);

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
			if (sky_tx(sky, &frame, t) >= 0) {
				modem_tx(&frame, t);

				// Clear the frame and re-use the memory
				memset(&frame, 0, sizeof(frame));
			}

			//sky_print_diag(sky);
		}

	}
}
