/*
 * Interface code to connect Suo-modem
 */

#include <zmq.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "suo.h"

#include "frame-io/zmq_interface.h"
#include "modem.h"


static timestamp_t last_received_time;

int tick_received = 0;
int tx_active = 0;
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
	snprintf(uri, ZMQ_URI_LEN, "tcp://localhost:%d", modem_base + 3);
	zmq_connect(suo_rx, uri);
	zmq_setsockopt(suo_rx, ZMQ_SUBSCRIBE, "", 0);

	// Connect to transmit frames socket
	snprintf(uri, ZMQ_URI_LEN, "tcp://localhost:%d", modem_base + 1);
	zmq_connect(suo_tx, uri);
	SKY_PRINTF(SKY_DIAG_INFO, "Modem TX connecting to %s\n", uri);

	// Set 1 seconds receiving timeout. If timeout occurs the modem is not running!
	int64_t rx_timeout = 1000;
	zmq_setsockopt(suo_rx, ZMQ_RCVTIMEO, &rx_timeout, sizeof(rx_timeout));


	suo_frame = suo_frame_new(256);

	/* Clear/flush any old frames from ZeroMQ buffers */
	char temp[1024];
	while (zmq_recv(suo_rx, temp, 1024, ZMQ_DONTWAIT) >= 0);
}


void modem_wait_for_sync() {

	SKY_PRINTF(SKY_DIAG_INFO, "Waiting for timing information");
	int len = suo_zmq_recv_frame(suo_rx, suo_frame);

	for (;;) {
		int ret = suo_zmq_recv_frame(suo_rx, suo_frame);
		if (errno != EAGAIN) {
			SKY_PRINTF(SKY_DIAG_BUG, "Modem zmq_recv() error %d\n", zmq_strerror(errno));
			abort();
		}
		if (ret == 0) {
			last_received_time = suo_frame.timestamp / 1e6;
			break;
		}
		SKY_PRINTF(SKY_DIAG_INFO, ".");
	}

	SKY_PRINTF(SKY_DIAG_INFO, "\n");
}


int modem_tx(SkyRadioFrame* frame, timestamp_t t)
	struct frame* suo_frame = suo_frame_new(frame->length);
	tx_active = 1;
	int ret = suo_zmq_send_frame(suo_tx, suo_frame);
	return ret;
}


int modem_rx(SkyRadioFrame* sky_frame, int flags) {

	/*
	 * Wait for new message from the modem
	 */
	suo_frame_clear(suo_frame);
	int len = suo_zmq_recv_frame(suo_rx, suo_frame);
	if (len < 0) {
		if (errno == EAGAIN)
			SKY_PRINTF(SKY_DIAG_INFO, "Ticks lost!\n");
		else
			SKY_PRINTF(SKY_DIAG_BUG, "Modem zmq_recv() error %d\n", zmq_strerror(errno));
		return -1;
	}
	if (len == 0){
		SKY_PRINTF(SKY_DIAG_BUG, "Modem zmq_recv()=0 error (-2)\n");
		return -2;
	}


	if (suo_frame->data_len == 0) {
		/*
		 * Modem control frame (tick frame)
		 */
		SKY_PRINTF(SKY_DIAG_FRAMES, "%20lu: Tick\n", suo_frame->timestamp);

		last_received_time = suo_frame->timestamp / 1e6; // ns to ms
		tick_received = 1;

		if ((suo_frame->flags & SUO_FLAGS_TX_ACTIVE) != 0)
			tx_active = 1;

		return 0;
	}
	else {
		/*
		 * New frame received
		 */
		SKY_PRINTF(SKY_DIAG_FRAMES, "%20lu: Suo receive %d bytes\n", suo_frame->timestamp, suo_frame->data_len);

		// Copy data from Suo frame to Skylink frame
		sky_frame->length = suo_frame->len;
		sky_frame->timestamp = suo_frame->timestamp / 1000;
		//frame->meta.rssi = suo_frame.metadata[0];
		memcpy(sky_frame->raw, suo_frame->data, suo_frame->data_len);

		return 1;
	}

	return 0;
}


int modem_tx_active() {
	return tx_active;
}

int tick() {
	int ret = tick_received;
	tick_received = 0;
	return ret;
}


timestamp_t get_timestamp() {
	return last_received_time;
}
