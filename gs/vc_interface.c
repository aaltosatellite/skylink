
#include <zmq.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>


#include "skylink/skylink.h"
#include "skylink/diag.h"
#include "skylink/utilities.h"

#include "vcs.h"

/*
 * List of control message command/response types
 */
#define VC_CTRL_PUSH                0
#define VC_CTRL_PULL                1

#define VC_CTRL_GET_BUFFER          5
#define VC_CTRL_BUFFER_RSP          6
#define VC_CTRL_FLUSH_BUFFERS       7

#define VC_CTRL_GET_STATS           10
#define VC_CTRL_STATS_RSP           11
#define VC_CTRL_CLEAR_STATS         12

#define VC_CTRL_SET_CONFIG          13
#define VC_CTRL_GET_CONFIG          14
#define VC_CTRL_CONFIG_RSP          15

#define VC_CTRL_ARQ_CONNECT         20
#define VC_CTRL_ARQ_RESET           21
#define VC_CTRL_ARQ_TIMEOUT         22

int handle_control_message(int vc, int cmd, uint8_t* msg, unsigned int msg_len);

const int PP = 0; // use push/pull instead of pub/sub for VC interfaces?


struct zmq_vc {
	void *zmq_tx;
	void *zmq_rx;
};

struct zmq_vc vcs[SKY_NUM_VIRTUAL_CHANNELS];

extern SkyHandle handle;
extern void *zmq;

#define PACKET_RX_MAXLEN  0x1000
#define PACKET_TX_MAXLEN  0x200


void* z_ps_rx[SKY_NUM_VIRTUAL_CHANNELS];
void* z_ps_tx[SKY_NUM_VIRTUAL_CHANNELS];



int vc_init(unsigned int vc_base, bool use_push_pull) {

	int vc;

	#define ZMQ_URI_LEN 64
	char uri[ZMQ_URI_LEN];

	/*
	 * Create ZMQ sockets for the virtual channel interfaces
	 */
	for (vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		void *sock = zmq_socket(zmq, use_push_pull ? ZMQ_PUSH : ZMQ_PUB);
		if (sock == NULL)
				SKY_PRINTF(SKY_DIAG_BUG, "zmq_socket() failed: %s", zmq_strerror(errno));

		snprintf(uri, ZMQ_URI_LEN, "tcp://*:%d", vc_base + vc);
		SKY_PRINTF(SKY_DIAG_INFO, "VC %d RX binding %s\n", vc, uri);

		if (zmq_bind(sock, uri) < 0)
			fprintf(stderr, "zmq_bind() failed: %s", zmq_strerror(errno));

		z_ps_rx[vc] = sock;
	}

	for (vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		void *sock = zmq_socket(zmq, use_push_pull ? ZMQ_PULL : ZMQ_SUB);
		if (sock == NULL)
			SKY_PRINTF(SKY_DIAG_BUG, "zmq_socket() failed: %s", zmq_strerror(errno));

		snprintf(uri, ZMQ_URI_LEN, "tcp://*:%d", vc_base + 100 + vc);
		SKY_PRINTF(SKY_DIAG_INFO, "VC %d TX binding %s\n", vc, uri);

		if (zmq_bind(sock, uri) < 0)
			SKY_PRINTF(SKY_DIAG_BUG, "zmq_bind() failed: %s", zmq_strerror(errno));

		if (!PP)
			zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
		z_ps_tx[vc] = sock;
	}

	return 0;
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

		//if (skyArray_count_readable_rcv_packets(handle->arrayBuffers[vc]) == 0)
		//	continue;

		int sequence;
		ret = skyArray_read_next_received(handle->arrayBuffers[vc], data + 1, &sequence);
		if (ret > 0) {
			SKY_PRINTF(SKY_DIAG_DEBUG, "VC%d: Received %d bytes\n", vc, ret);

			data[0] = VC_CTRL_PULL;
			if (zmq_send(z_ps_rx[vc], data, ret + 1, ZMQ_DONTWAIT) < 0)
				SKY_PRINTF(SKY_DIAG_BUG, "zmq_send() failed: %s", zmq_strerror(errno));

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
		if (ret < 0) {
			if(errno == EAGAIN)
				continue;
			SKY_PRINTF(SKY_DIAG_BUG, "VC%d: zmq_recv() error %d %s\n", vc, errno, zmq_strerror(errno));
		}

		if (ret >= 0) {
			/* Handle control messages */
			int ret2 = handle_control_message(vc, data[0], &data[1], ret-1);

			// Send response
			if (ret2 > 0)
				ret2 = zmq_send(z_ps_rx[vc], data, ret2, ZMQ_DONTWAIT);
			if (ret2 < 0)
				SKY_PRINTF(SKY_DIAG_FRAMES, "VC%d: zmq_send() error %s\n", vc, zmq_strerror(errno));

		}

	}
}



int handle_control_message(int vc, int cmd, uint8_t* msg, unsigned int msg_len) {

	int rsp_len = 0;
	int rsp_code = 0;
	uint8_t* rsp = msg;

	SKY_PRINTF(SKY_DIAG_DEBUG, "CTRL MSG vc: %d  cmd: %d len: msg_len %d\n", vc, cmd, msg_len);

	switch (cmd) {
	case VC_CTRL_PUSH: {
		/*
		 * Write data to buffer
		 */
		SKY_PRINTF(SKY_DIAG_FRAMES, "VC%d: Sending %d bytes\n", vc, msg_len);
		skyArray_push_packet_to_send(handle->arrayBuffers[vc], msg, msg_len);
		break; // No response
	}
	case VC_CTRL_PULL: {
		/*
		 * Read data from buffer.
		 */
		skyArray_count_readable_rcv_packets(handle->arrayBuffers[vc]);

		int sequence;
		uint8_t* tgt = malloc(1000);
		int read = skyArray_read_next_received(handle->arrayBuffers[vc], tgt, &sequence);
		printf("Read %d\n", read);
		break;
	}
	case VC_CTRL_GET_BUFFER: {
		/*
		 * Get virtual channel buffer status
		 */
#if 0
		sky_get_buffer_status(handle, (SkyBufferState_t*)rsp);
		uint16_t* vals = (uint16_t*)rsp;
		for (int i = 2; i < sizeof(SkyBufferState_t)/2; i++)
			vals[i] = sky_hton16(vals[i]);
		rsp_len = sizeof(SkyBufferState_t);
		rsp_code = VC_CTRL_BUFFER_RSP;
#endif
		break;
	}
 	case VC_CTRL_FLUSH_BUFFERS: {
		/*
		 * Flush virtual channel buffers
		 */
 		//sky_flush_buffers(handle);
		break; // No response
	}
	case VC_CTRL_GET_STATS: {
		/*
		 * Get statistics
		 */
		memcpy(rsp, handle->diag, sizeof(SkyDiagnostics));
		uint16_t* vals = (uint16_t*)rsp;
		for (unsigned int i = 0; i < sizeof(SkyDiagnostics)/2; i++)
			vals[i] = sky_hton16(vals[i]);
		rsp_len = sizeof(SkyDiagnostics);
		rsp_code = VC_CTRL_STATS_RSP;
		break;
	}
	case VC_CTRL_CLEAR_STATS: {
		/*
		 * Reset statistics
		 */
		//sky_clear_stats(handle);
		break; // No response
	}
	case VC_CTRL_SET_CONFIG: {
		/*
		 * Set Skylink Configuration
		 */
		unsigned int cfg = msg[0]; // TODO: Correct byte-lengths
		unsigned int val = msg[1];
		//sky_set_config(handle, cfg, val);
		break; // No response
	}
	case VC_CTRL_GET_CONFIG: {
		/*
		 * Get Skylink Configuration
		 */
		unsigned int cfg = msg[0]; // TODO: Correct byte-lengths
		unsigned int val;

		//if (handle_get_config(handle, cfg, &val) < 0) {
		//	break; // TODO
		//}

		rsp[0] = val;
		rsp_len = 1;
		rsp_code = VC_CTRL_CONFIG_RSP;
		break;
	}
	case VC_CTRL_ARQ_CONNECT: {
		/*
		 * ARQ
		 */
		//sky_arq_connect(handle, vc);
		break; // No response
	}
	case VC_CTRL_ARQ_RESET: {
		/*
		 * ARQ
		 */
		//sky_arq_reset(handle, vc);
		break; // No response
	}
	default:
		return -1;
	}

	cmd = rsp_code;

	return rsp_len;
}
