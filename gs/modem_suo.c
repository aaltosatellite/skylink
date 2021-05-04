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


int modem_tx(SkyRadioFrame_t* frame, timestamp_t t) {
	//assert();

	struct suoframe fr;
	memset(&fr, 0, sizeof(struct suoframe));

	fr.id = SUO_FRAME_TRANSMIT;
	fr.flags |= 2 | 4; // SUO_FLAG_
	fr.time = t * 1000;
	fr.len = frame->length;
	memcpy(fr.data, frame->raw, frame->length);

	SKY_PRINTF(SKY_DIAG_FRAMES, "%20lu: Suo transmit %d bytes\n", t, frame->length);

	int ret = zmq_send(suo_tx, &fr, sizeof(struct suoframe) + frame->length, ZMQ_DONTWAIT);
	if (ret < 0)
		printf("zmq_send() error %d\n", ret);

	return ret;
}


int modem_rx(struct suoframe* fr, int flags) {


	int ret = zmq_recv(suo_rx, fr, 64+RADIOFRAME_MAXLEN, 0);

	if (ret < 0)
		printf("zmq_recv() error %d\n", ret);


	if (fr->id == SUO_FRAME_TIMING && ret == sizeof(struct suotiming)) {

		last_received_time = fr->time;
		tick_received = 1;

		return 0;
	}
	else if (fr->id == SUO_FRAME_RECEIVE && ret >= 64 /* sizeof(suoframe)*/) {

		/* RX frame */
		SKY_PRINTF(SKY_DIAG_FRAMES, "%20lu: Suo receive %d bytes\n", fr->time, fr->len);

		return ret;
	}

	return 0;
}


int tick() {
	int ret = tick_received;
	tick_received = 0;
	return ret;
}
