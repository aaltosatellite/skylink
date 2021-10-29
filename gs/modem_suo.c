/*
 * Interface code to connect Suo-modem
 */

#include <zmq.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>

#include "suo.h"
#include "modem.h"

int tick_received = 0;
void *suo_rx, *suo_tx;

extern void *zmq;

#define ZMQ_URI_LEN 64

void modem_init(int modem_base) {

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


	/* Clear/flush any old frames from ZeroMQ buffers */
	char temp[RADIOFRAME_MAXLEN];
	while (zmq_recv(suo_rx, temp, RADIOFRAME_MAXLEN, ZMQ_DONTWAIT) >= 0);

}


void modem_wait_for_sync() {

	SKY_PRINTF(SKY_DIAG_INFO, "Waiting for timing information...\n");

	for (;;) {
		struct suoframe fr;
		int ret;
		ret = zmq_recv(suo_rx, &fr, 64, 0);
		if (ret == sizeof(struct suotiming)) {
			//params.initial_time = fr.time / 1000;
			break;
		}
	}

}

static timestamp_t last_received_time;

timestamp_t get_timestamp() {
	return last_received_time / 1000;
}


int modem_tx(SkyRadioFrame* frame, timestamp_t t) {
	//assert(frame);

	struct suoframe fr;
	memset(&fr, 0, sizeof(struct suoframe));

	fr.id = SUO_FRAME_TRANSMIT;
	fr.flags |= 2 | 4; // SUO_FLAG_
	fr.time = t * 1000;
	fr.len = frame->length;
	memcpy(fr.data, frame->raw, frame->length);

	SKY_PRINTF(SKY_DIAG_FRAMES, "%q20lu: Suo transmit %d bytes\n", t, frame->length);

	int ret = zmq_send(suo_tx, &fr, sizeof(struct suoframe) + frame->length, ZMQ_DONTWAIT);
	if (ret < 0)
		SKY_PRINTF(SKY_DIAG_BUG, "Modem zmq_send() error %s\n", zmq_strerror(errno));

	return ret;
}


int modem_rx(SkyRadioFrame* frame, int flags) {
	struct suoframe suo_frame;
	int len = zmq_recv(suo_rx, &suo_frame, 64+RADIOFRAME_MAXLEN, 0);

	int rcv_returns[3] = {0,0,0};
	struct zmq_msg_t rcv_msgs[3];
	int n_rcv = 0;
	while (1){
		zmq_msg_init(&rcv_msgs[n_rcv]);
		int r = zmq_msg_recv(&rcv_msgs[n_rcv], suo_rx, ZMQ_DONTWAIT);
		if(r < 0){
			break;
		}
		rcv_returns[n_rcv] = r;
		n_rcv++;

		int more = 0;
		size_t optlen = 0;
		zmq_getsockopt(suo_rx, ZMQ_RCVMORE, &more, &optlen);
		if(!more){
			break;
		}
	}


	if (len < 0) {
		SKY_PRINTF(SKY_DIAG_BUG, "Modem zmq_recv() error %d\n", zmq_strerror(errno));
		return -1;
	}
	if (len == 0){
		SKY_PRINTF(SKY_DIAG_BUG, "Modem zmq_recv()=0 error (-2)\n");
		return -2;
	}


	if (suo_frame.id == SUO_FRAME_TIMING) {
		/*
		 * Parse timing frame
		 */
		if (len != sizeof(struct suotiming))
			return -2;

		last_received_time = suo_frame.time;
		tick_received = 1;

		SKY_PRINTF(SKY_DIAG_FRAMES, "%20lu: Tick\n", suo_frame.time);

		return 0;
	}
	else if (suo_frame.id == SUO_FRAME_RECEIVE) {
		/*
		 * New frame received
		 */
		if (len <= 64)
 			return -2;

		SKY_PRINTF(SKY_DIAG_FRAMES, "%20lu: Suo receive %d bytes\n", suo_frame.time, suo_frame.len);
		SKY_ASSERT(len == 64 + suo_frame.len);

		// Copy data from Suo frame to Skylink frame
		frame->length = suo_frame.len;
		frame->timestamp = suo_frame.time / 1000;
		//frame->meta.rssi = suo_frame.metadata[0];
		memcpy(frame->raw, suo_frame.data, suo_frame.len);

		return 1;
	}
	else {
		/*
		 * Unknown frame type
		 */
		SKY_PRINTF(SKY_DIAG_FRAMES, "modem_rx: unknown frame %d\n", suo_frame.id);
		return -999;
	}
	return 0;
}


int tick() {
	int ret = tick_received;
	tick_received = 0;
	return ret;
}
