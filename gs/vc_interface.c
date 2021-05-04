
#include <zmq.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>


#include "skylink/skylink.h"

const int PP = 1; // use push/pull instead of pub/sub for VC interfaces?


struct zmq_vc {
	void *zmq_tx;
	void *zmq_rx;
};

struct zmq_vc vcs[SKY_NUM_VIRTUAL_CHANNELS];

extern SkyHandle_t sky;
extern void *zmq;

#define PACKET_RX_MAXLEN  0x1000
#define PACKET_TX_MAXLEN  0x200


void* z_ps_rx[4];
void* z_ps_tx[4];



int vc_init(int vc_base) {

	int vc;

#define ZMQ_URI_LEN 64
	char uri[ZMQ_URI_LEN];

	/*
	 * Create ZMQ sockets for the virtual channel interfaces
	 */
	for (vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		void *sock = zmq_socket(zmq, PP ? ZMQ_PUSH : ZMQ_PUB);
		snprintf(uri, ZMQ_URI_LEN, "tcp://*:%d", vc_base + vc);
		SKY_PRINTF(SKY_DIAG_INFO, "VC %d RX binding %s\n", vc, uri);
		zmq_bind(sock, uri);
		z_ps_rx[vc] = sock;
	}

	for (vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		void *sock = zmq_socket(zmq, PP ? ZMQ_PULL : ZMQ_SUB);
		snprintf(uri, ZMQ_URI_LEN, "tcp://*:%d", vc_base + 100 + vc);
		SKY_PRINTF(SKY_DIAG_INFO, "VC %d TX binding %s\n", vc, uri);
		zmq_bind(sock, uri);
		if (!PP)
			zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
		z_ps_tx[vc] = sock;
	}

}

int vc_tx(void *arg, uint8_t *data, int maxlen)
{
	struct zmq_vc *vc = arg;
	int ret;

	ret = zmq_recv(vc->zmq_tx, data, maxlen, ZMQ_DONTWAIT);

	if (ret < 0) {
		return -1; /* Nothing to send */
	} else if (ret > maxlen) {
		// TODO?
		SKY_PRINTF(SKY_DIAG_DEBUG, "Truncated packet\n");
		return maxlen;
	} else {
		return ret; /* Packet returned */
	}
}


int vc_rx(void *arg, const uint8_t *data, int len)
{
	struct zmq_vc *vc = arg;
	SKY_PRINTF(SKY_DIAG_DEBUG, "Received packet of %d bytes\n", len);
	zmq_send(vc->zmq_rx, data, len, ZMQ_DONTWAIT);
	return 0; /* OK */
}





void vc_check() {

	int vc;
	int ret;

	/* If packets appeared to some RX buffer, send them to ZMQ */
	for (vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		uint8_t data[PACKET_RX_MAXLEN];
		unsigned flags = 0;
		ret = sky_buf_read(sky->rxbuf[vc], data, PACKET_RX_MAXLEN, &flags);
		if (ret >= 0) {
			zmq_send(z_ps_rx[vc], data, ret, ZMQ_DONTWAIT);
			if ((flags & (BUF_FIRST_SEG|BUF_LAST_SEG)) != (BUF_FIRST_SEG|BUF_LAST_SEG)) {
				SKY_PRINTF(SKY_DIAG_DEBUG, "RX %d len %5d flags %u: Buffer read fragmented a packet. This shouldn't really happen here.\n",
					vc, ret, flags);
			}
		}
	}

	/* If some TX packets were received from ZMQ,
	 * write them to TX buffer.
	 * A cleaner way would be to create a thread for each
	 * and use blocking reads from ZMQ,
	 * but this works too. */
	for (vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		uint8_t data[PACKET_TX_MAXLEN];
		ret = zmq_recv(z_ps_tx[vc], data, PACKET_TX_MAXLEN, ZMQ_DONTWAIT);
		if (ret >= 0) {
			int ret2;
			ret2 = sky_buf_write(sky->txbuf[vc], data, ret, BUF_FIRST_SEG|BUF_LAST_SEG);
			if (ret2 < 0) {
				// buffer overrun
				SKY_PRINTF(SKY_DIAG_DEBUG, "TX %d len %5d: error %5d\n",
					vc, ret, ret2);
			}
		}
	}
}
