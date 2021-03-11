#include "skylink/skylink.h"
#include "platform/debug.h"
#include <zmq.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "suo.h"

void *zmq = NULL;
const int PP = 1; // use push/pull instead of pub/sub for VC interfaces?

// number of packet streams
#define PS_RX_N 4
#define PS_TX_N 4



struct ap_all ap1_;
struct ap_all *ap1 = &ap1_;

// ZeroMQ sockets for each packet stream
void *z_ps_rx[PS_RX_N], *z_ps_tx[PS_TX_N];

#if 0
#define VC_N 4
struct zmq_vc vcs[VC_N];

int vc_tx(void *arg, uint8_t *data, int maxlen)
{
	struct zmq_vc *vc = arg;
	int ret;
	ret = zmq_recv(vc->zmq_ul, data, maxlen, ZMQ_DONTWAIT);
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
	zmq_send(vc->zmq_dl, data, len, ZMQ_DONTWAIT);
	return 0; /* OK */
}
#endif

#define NAMELEN 100
#define PACKET_RX_MAXLEN 0x10000
#define PACKET_TX_MAXLEN 0x200

int main(int argc, char *argv[])
{
	// How many nanosecond beforehand transmit frames should be given
	const uint64_t tx_ahead_time = /*20000000*/ 0;

	void *z_rx, *z_tx;
	struct radioframe frame;
	char name[NAMELEN];
	int modem_base = 43700, vc_base = 52000;

	/* ------------------
	 * Read configuration
	 * ------------------ */
	struct ap_conf params = {
		.tdd_slave = 1
	};

	if (argc >= 4) {
		if (argv[1][0] == 'm')
			params.tdd_slave = 0;
		modem_base = atoi(argv[2]);
		vc_base = atoi(argv[3]);
	}


	/* -------------------
	 * Open ZeroMQ sockets
	 * ------------------- */

	zmq = zmq_ctx_new();
	z_rx = zmq_socket(zmq, ZMQ_SUB);

	// Receive frames
	snprintf(name, NAMELEN, "tcp://localhost:%d", modem_base);
	zmq_connect(z_rx, name);
	// Transmitter timing
	snprintf(name, NAMELEN, "tcp://localhost:%d", modem_base + 3);
	zmq_connect(z_rx, name);

	zmq_setsockopt(z_rx, ZMQ_SUBSCRIBE, "", 0);

	z_tx = zmq_socket(zmq, ZMQ_PUB);
	// Transmit frames
	snprintf(name, NAMELEN, "tcp://localhost:%d", modem_base + 1);
	zmq_connect(z_tx, name);

	int ps;
	for (ps = 0; ps < PS_RX_N; ps++) {
		void *sock;
		sock = zmq_socket(zmq, PP ? ZMQ_PUSH : ZMQ_PUB);
		snprintf(name, NAMELEN, "tcp://*:%d", vc_base + ps);
		zmq_bind(sock, name);
		z_ps_rx[ps] = sock;
	}

	for (ps = 0; ps < PS_TX_N; ps++) {
		void *sock;
		sock = zmq_socket(zmq, PP ? ZMQ_PULL : ZMQ_SUB);
		snprintf(name, NAMELEN, "tcp://*:%d", vc_base + 100 + ps);
		zmq_bind(sock, name);
		if (!PP)
			zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
		z_ps_tx[ps] = sock;
	}

	/* Clear any old frames from ZeroMQ buffers */
	while (zmq_recv(z_rx, frame.data, RADIOFRAME_MAXLEN, ZMQ_DONTWAIT) >= 0);


	/* -------------------------
	 * Initialize protocol stack
	 * ------------------------- */
	for (ps=0; ps<PS_RX_N; ps++) {
		ap1->rxbuf[ps] = ap_buf_init(0x4000);
	}

	for (ps=0; ps<PS_TX_N; ps++) {
		ap1->txbuf[ps] = ap_buf_init(0x4000);
	}

	/* Read the first timing message */
	for (;;) {
		struct suoframe fr;
		int ret;
		ret = zmq_recv(z_rx, &fr, 64, 0);
		if (ret == sizeof(struct suotiming)) {
			params.initial_time = fr.time / 1000;
			break;
		}
	}
	ap1 = sky_init(ap1, &params);


	/* ----------------
	 * Run the protocol
	 * ---------------- */
	for (;;) {
		int ret;
		struct suoframe fr;

		ret = zmq_recv(z_rx, &fr, 64+RADIOFRAME_MAXLEN, 0);
		if (ret >= 64) {
			/* RX frame */
			SKY_PRINTF(SKY_DIAG_DEBUG, "%20lu: Receive\n", fr.time);
			frame.length = ret - 64;
			frame.timestamp = fr.time / 1000;
			memcpy(frame.data, fr.data, frame.length);
			sky_rx(ap1, &frame);
			sky_print_diag(ap1);
		} else if (ret == sizeof(struct suotiming)) {
			/* Tick message */
			uint64_t t = fr.time + tx_ahead_time;
			if (sky_tx(ap1, &frame, t / 1000) >= 0) {
				SKY_PRINTF(SKY_DIAG_DEBUG, "%20lu: Transmit\n", t);
				memset(&fr, 0, 64);
				fr.id = 1;
				fr.flags |= 2 | 4;
				fr.time = t;
				memcpy(fr.data, frame.data, frame.length);
				fr.len = frame.length;
				zmq_send(z_tx, &fr, 64+frame.length, 0);
			}
		}

		/* If packets appeared to some RX buffer, send them to ZMQ */
		for (ps=0; ps<PS_RX_N; ps++) {
			uint8_t data[PACKET_RX_MAXLEN];
			unsigned flags = 0;
			ret = ap_buf_read(ap1->rxbuf[ps], data, PACKET_RX_MAXLEN, &flags);
			if (ret >= 0) {
				zmq_send(z_ps_rx[ps], data, ret, 0);
				if ((flags & (BUF_FIRST_SEG|BUF_LAST_SEG)) != (BUF_FIRST_SEG|BUF_LAST_SEG)) {
					SKY_PRINTF(SKY_DIAG_DEBUG, "RX %d len %5d flags %u: Buffer read fragmented a packet. This shouldn't really happen here.\n",
						ps, ret, flags);
				}
			}
		}

		/* If some TX packets were received from ZMQ,
		 * write them to TX buffer.
		 * A cleaner way would be to create a thread for each
		 * and use blocking reads from ZMQ,
		 * but this works too. */
		for (ps=0; ps<PS_TX_N; ps++) {
			uint8_t data[PACKET_TX_MAXLEN];
			int ret;
			ret = zmq_recv(z_ps_tx[ps], data, PACKET_TX_MAXLEN, ZMQ_DONTWAIT);
			if (ret >= 0) {
				int ret2;
				ret2 = ap_buf_write(ap1->txbuf[ps], data, ret, BUF_FIRST_SEG|BUF_LAST_SEG);
				if (ret2 < 0) {
					// buffer overrun
					SKY_PRINTF(SKY_DIAG_DEBUG, "TX %d len %5d: error %5d\n",
						ps, ret, ret2);
				}
			}
		}
	}
	return 0;
}
