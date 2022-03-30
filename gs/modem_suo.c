/*
 * Interface code to connect Suo-modem
 */

#include <zmq.h>
#include <string.h>
#include <stdlib.h>

#include "skylink/skylink.h"
#include "skylink/frame.h"
#include "skylink/diag.h"
#include "../platforms/posix/timestamp.h"

#include "suo.h"
#include "frame-io/zmq_interface.h"
#include "modem.h"


const int tx_transmit_delay = 10; // Minimum delay between consecutive transmissions
const int switching_delay   = 20; // Mimimum delay when switching from RX to TX


static timestamp_t global_tick_time;
static timestamp_t last_rx_tick;
static timestamp_t last_tx_tick;

static int tick_received = 0;
static int carrier_sensed = 0;
static int tx_active = 0;
static int rx_active = 0;
void *suo_rx, *suo_tx;

extern void *zmq;
struct frame* suo_frame;


void modem_init(int modem_base) {

#define ZMQ_URI_LEN 64
	suo_rx = zmq_socket(zmq, ZMQ_SUB);
	suo_tx = zmq_socket(zmq, ZMQ_PUB);

	char uri[ZMQ_URI_LEN];

	// Connect to receive frames socket
	snprintf(uri, ZMQ_URI_LEN, "tcp://localhost:%d", modem_base);
	zmq_connect(suo_rx, uri);
	SKY_PRINTF(SKY_DIAG_INFO, "Modem RX connecting to %s\n", uri);

	// Connect to modem timing socket
	snprintf(uri, ZMQ_URI_LEN, "tcp://localhost:%d", modem_base + 2);
	zmq_connect(suo_rx, uri);
	zmq_setsockopt(suo_rx, ZMQ_SUBSCRIBE, "", 0);

	// Connect to transmit frames socket
	snprintf(uri, ZMQ_URI_LEN, "tcp://localhost:%d", modem_base + 1);
	zmq_connect(suo_tx, uri);
	SKY_PRINTF(SKY_DIAG_INFO, "Modem TX connecting to %s\n", uri);

	// Set 1 seconds receiving timeout. If timeout occurs the modem is not running!
	int rx_timeout = 500; // [ms]
	zmq_setsockopt(suo_rx, ZMQ_RCVTIMEO, &rx_timeout, sizeof(rx_timeout));

	suo_frame = suo_frame_new(256);

	/* Clear/flush any old frames from ZeroMQ buffers */
	char temp[1024];
	while (zmq_recv(suo_rx, temp, 1024, ZMQ_DONTWAIT) >= 0);
}


void modem_wait_for_sync() {

	SKY_PRINTF(SKY_DIAG_INFO, "Waiting for timing information");
	for (;;) {
		int ret = suo_zmq_recv_frame(suo_rx, suo_frame, 0);
		if (ret < 0) {
			SKY_PRINTF(SKY_DIAG_BUG, "suo_zmq_recv_frame() error %s\n", zmq_strerror(errno));
			abort();
		}
		if (ret == 1) {
			global_tick_time = suo_frame->hdr.timestamp / 1e6; // ns -> ms
			break;
		}
		SKY_PRINTF(SKY_DIAG_INFO, ".");
	}

	SKY_PRINTF(SKY_DIAG_INFO, "\n");
}


int modem_tx(SkyRadioFrame* sky_frame, timestamp_t t) {
	(void)t;

	SKY_PRINTF(SKY_DIAG_FRAMES, "\x1B[34m" "Suo transmit %d bytes" "\x1B[0m\n", sky_frame->length);

	suo_frame_clear(suo_frame);
	suo_frame->hdr.id = SUO_MSG_TRANSMIT;
	suo_frame->hdr.flags = 0;
	//suo_frame->hdr.timestamp = sky_frame->timestamp * 1000;

	suo_frame->data_len = sky_frame->length;
	memcpy(suo_frame->data, sky_frame->raw, sky_frame->length);

	tx_active = 1;
	int ret = suo_zmq_send_frame(suo_tx, suo_frame, ZMQ_DONTWAIT);

	suo_frame_clear(suo_frame);

	if (ret < 0)
		SKY_PRINTF(SKY_DIAG_BUG, "Modem suo_zmq_send_frame() error %s\n", zmq_strerror(errno));
	return ret;
}


int modem_rx(SkyRadioFrame* sky_frame, int flags) {

	/*
	 * Wait for new message from the modem
	 */
	int len = suo_zmq_recv_frame(suo_rx, suo_frame, flags);
	if (len < 0) {
		SKY_PRINTF(SKY_DIAG_BUG, "suo_zmq_recv_frame() error %s\n", zmq_strerror(errno));
		return -1;
	}
	if (len == 0){
		SKY_PRINTF(SKY_DIAG_INFO, "Ticks lost!\n");
		return -2;
	}


	if (suo_frame->data_len == 0) {
		/*
		 * Modem control frame (tick frame)
		 */
		//SKY_PRINTF(SKY_DIAG_FRAMES, "%20lu: Tick\n", suo_frame->hdr.timestamp);

		global_tick_time = suo_frame->hdr.timestamp / 1e6; // ns to ms
		tick_received = 1;

		tx_active = ((suo_frame->hdr.flags & SUO_FLAGS_TX_ACTIVE) != 0);
		rx_active = ((suo_frame->hdr.flags & SUO_FLAGS_RX_LOCKED) != 0);

		if (tx_active)
			last_tx_tick = global_tick_time;

		if (rx_active) {
			last_rx_tick = global_tick_time;

			if (carrier_sensed == 0)
				carrier_sensed = 1;
		}
		else {
			carrier_sensed = 0;
		}

		return 0;
	}
	else {
		/*
		 * New frame received
		 */

		// Copy data from Suo frame to Skylink frame
		sky_frame->length = suo_frame->data_len;
		sky_frame->rx_time_ticks = suo_frame->hdr.timestamp / 1e6;
		//frame->meta.rssi = suo_frame.metadata[0];
		memcpy(sky_frame->raw, suo_frame->data, suo_frame->data_len);

		SKY_PRINTF(SKY_DIAG_FRAMES, "\x1B[36m" "Suo receive %d bytes" "\x1B[0m\n", sky_frame->length);

		suo_frame_clear(suo_frame);
		return 1;
	}

	return 0;
}

int modem_can_send() {
	if (tx_active == 1) // TX already/still active
		return 0;
	if (rx_active == 1) // Receiving frame
		return 0;
	if (global_tick_time - last_tx_tick < tx_transmit_delay) // No too fast txing
		return 0;
	if (global_tick_time - last_rx_tick < switching_delay) // RX/TX switching delay
		return 0;
	return 1;
}

int tick() {
	int ret = tick_received;
	tick_received = 0;
	return ret;
}

int modem_carrier_sensed() {
	if (carrier_sensed == 1) {
		carrier_sensed = 2;
		return 1;
	}
	return 0;
}

timestamp_t get_timestamp() {
	return global_tick_time;
}
