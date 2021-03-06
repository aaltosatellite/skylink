
#include <zmq.h>
#include <string.h>
#include "skylink/skylink.h"
#include "skylink/mac.h"
#include "skylink/reliable_vc.h"
#include "skylink/diag.h"
#include "skylink/utilities.h"

#include "vc_interface.h"


int handle_control_message(int vc, int cmd, uint8_t* msg, unsigned int msg_len);
int sky_set_config(SkyHandle handle, const char* parameter,  const char *value);

struct zmq_vc {
	void *zmq_tx;
	void *zmq_rx;
	int arq_expected_state;
};

struct zmq_vc vcs[SKY_NUM_VIRTUAL_CHANNELS];


extern SkyHandle handle;
extern void *zmq;


#define PACKET_RX_MAXLEN  0x1000
#define PACKET_TX_MAXLEN  0x200


int vc_init(unsigned int vc_base, bool use_push_pull) {

	int vc;

	#define ZMQ_URI_LEN 64
	char uri[ZMQ_URI_LEN];

	/*
	 * Create ZMQ sockets for the virtual channel interfaces
	 */
	for (vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		void *sock = zmq_socket(zmq, use_push_pull ? ZMQ_PUSH : ZMQ_PUB);
		if (sock == NULL) {
			SKY_PRINTF(SKY_DIAG_BUG, "zmq_socket() failed: %s", zmq_strerror(errno));
		}

		snprintf(uri, ZMQ_URI_LEN, "tcp://*:%d", vc_base + vc);
		SKY_PRINTF(SKY_DIAG_INFO, "VC %d RX binding %s\n", vc, uri);

		if (zmq_bind(sock, uri) < 0) {
			fprintf(stderr, "zmq_bind() failed: %s", zmq_strerror(errno));
		}

		vcs[vc].zmq_rx = sock;
	}

	for (vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		void *sock = zmq_socket(zmq, use_push_pull ? ZMQ_PULL : ZMQ_SUB);
		if (sock == NULL) {
			SKY_PRINTF(SKY_DIAG_BUG, "zmq_socket() failed: %s", zmq_strerror(errno));
		}

		snprintf(uri, ZMQ_URI_LEN, "tcp://*:%d", vc_base + 100 + vc);
		SKY_PRINTF(SKY_DIAG_INFO, "VC %d TX binding %s\n", vc, uri);

		if (zmq_bind(sock, uri) < 0) {
			SKY_PRINTF(SKY_DIAG_BUG, "zmq_bind() failed: %s", zmq_strerror(errno));
		}

		if (use_push_pull == 0) {
			zmq_setsockopt(sock, ZMQ_SUBSCRIBE, "", 0);
		}
		vcs[vc].zmq_tx = sock;
	}

	return 0;
}



int vc_check_arq_states() {

	for (int vc = 0; vc < 4; vc++) {

		if (vcs[vc].arq_expected_state != ARQ_STATE_OFF) {
		 	if (handle->virtual_channels[vc]->arq_state_flag == ARQ_STATE_OFF) {
				vcs[vc].arq_expected_state = ARQ_STATE_OFF;

				SKY_PRINTF(SKY_DIAG_ARQ, "VC%d ARQ has disconnected!\n", vc);

				/* Send ARQ disconnected message */
				uint8_t msg[2] = { VC_CTRL_ARQ_TIMEOUT, vc };
				if (zmq_send(vcs[vc].zmq_rx, msg, 2, ZMQ_DONTWAIT) < 0) {
					SKY_PRINTF(SKY_DIAG_BUG, "zmq_send() failed: %s", zmq_strerror(errno));
				}
			}
		}
		else {
			// Has ARQ turned on
			if (handle->virtual_channels[vc]->arq_state_flag != ARQ_STATE_OFF) {
				vcs[vc].arq_expected_state = 1;
			}
		}
	}

	return 0;
}


/* Check frame going out (skylink -> ZMQ) */
int vc_check_rf_to_sys() {

	uint8_t data[PACKET_RX_MAXLEN];

	/* If packets appeared to some RX buffer, send them to ZMQ */
	for (unsigned int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {

		int ret = sky_vc_read_next_received(handle->virtual_channels[vc], data + 1, PACKET_RX_MAXLEN);
		if (ret > 0) {
			SKY_PRINTF(SKY_DIAG_DEBUG, "VC%d: Received %d bytes\n", vc, ret);

			data[0] = VC_CTRL_RECEIVE_VC0 + vc;
			if (zmq_send(vcs[vc].zmq_rx, data, ret + 1, ZMQ_DONTWAIT) < 0) {
				SKY_PRINTF(SKY_DIAG_BUG, "zmq_send() failed: %s", zmq_strerror(errno));
			}

		}
	}

	return 0;
}

/* Check frame going out (ZMQ -> skylink) */
int vc_check_sys_to_rf() {

	uint8_t data[PACKET_TX_MAXLEN];

	for (unsigned int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {

		int ret = zmq_recv(vcs[vc].zmq_tx, data, PACKET_TX_MAXLEN, ZMQ_DONTWAIT);
		if (ret < 0) {
			if(errno == EAGAIN) {
				continue;
			}
			SKY_PRINTF(SKY_DIAG_BUG, "VC%d: zmq_recv() error %d %s\n", vc, errno, zmq_strerror(errno));
		}

		/* Handle control messages */
		if (ret > 0) {
			handle_control_message(vc, data[0], &data[1], ret - 1);
		}

	}
	return 0;
}


int send_control_response(int vc, int rsp_code, void* data, unsigned int data_len) {

	SKY_PRINTF(SKY_DIAG_DEBUG, "CTRL RSP vc: %d  cmd: %d len: msg_len %d\n", vc, rsp_code, data_len);

	uint8_t rsp[data_len + 1];
	rsp[0] = rsp_code;
	memcpy(&rsp[1], data, data_len);

	if (zmq_send(vcs[vc].zmq_rx, rsp, data_len + 1, ZMQ_DONTWAIT) < 0) {
		SKY_PRINTF(SKY_DIAG_FRAMES, "zmq_send() error %s\n", zmq_strerror(errno));
		return -1;
	}

	return 0;
}


int handle_control_message(int vc, int cmd, uint8_t* msg, unsigned int msg_len) {

	SKY_PRINTF(SKY_DIAG_DEBUG, "CTRL MSG vc: %d  cmd: %d len: msg_len %d\n", vc, cmd, msg_len);

	switch (cmd) {
	case VC_CTRL_TRANSMIT_VC0:
	case VC_CTRL_TRANSMIT_VC1:
	case VC_CTRL_TRANSMIT_VC2:
	case VC_CTRL_TRANSMIT_VC3:
	{
		/*
		 * Write data to buffer
		 */
		//unsigned int vc = cmd - VC_CTRL_TRANSMIT_VC0;
		SKY_PRINTF(SKY_DIAG_FRAMES, "VC%d: Sending %d bytes\n", vc, msg_len);
		int ret = sky_vc_push_packet_to_send(handle->virtual_channels[vc], msg, msg_len);
		if (ret < 0) {
			SKY_PRINTF(SKY_DIAG_BUG, "VC%d: Failed to push new frame! %d\n", vc, ret);
		}
		break; // No response
	}

	case VC_CTRL_RECEIVE_VC0:
	case VC_CTRL_RECEIVE_VC1:
	case VC_CTRL_RECEIVE_VC2:
	case VC_CTRL_RECEIVE_VC3:
	{
		/*
		 * Read data from buffer.
		 */
		unsigned int vc_to_read = cmd - VC_CTRL_RECEIVE_VC0;
		uint8_t frame[500];
		int read = sky_vc_read_next_received(handle->virtual_channels[vc_to_read], frame, 500);
		send_control_response(vc_to_read, VC_CTRL_RECEIVE_VC0 + vc_to_read, frame, read);
		break;
	}

	case VC_CTRL_GET_STATE: {
		/*
		 * Get virtual channel buffer status
		 */
		SkyState state;
		sky_get_state(handle, &state);

		uint16_t* vals = (uint16_t*)&state;
		for (int i = 0; i < (int)sizeof(SkyState)/2; i++) {
			vals[i] = sky_hton16(vals[i]);
		}

		send_control_response(vc, VC_CTRL_STATE_RSP, &state, sizeof(SkyState));
		break;
	}

 	case VC_CTRL_FLUSH_BUFFERS: {
		/*
		 * Flush virtual channel buffers
		 */
		for (int vc_ = 0; vc_ < SKY_NUM_VIRTUAL_CHANNELS; vc_++) {
			sky_vc_wipe_to_arq_off_state(handle->virtual_channels[vc_]);
		}
		break; // No response
	}

	case VC_CTRL_GET_STATS: {
		/*
		 * Get statistics
		 */

		SkyDiagnostics stats;
		memcpy(&stats, handle->diag, sizeof(SkyDiagnostics));

		uint16_t* vals = (uint16_t*)&stats;
		for (unsigned int i = 0; i < sizeof(SkyDiagnostics)/2; i++) {
			vals[i] = sky_hton16(vals[i]);
		}

		send_control_response(vc, VC_CTRL_STATS_RSP, &stats, sizeof(SkyDiagnostics));
		break;
	}

	case VC_CTRL_CLEAR_STATS: {
		/*
		 * Reset statistics
		 */
		SKY_PRINTF(SKY_DIAG_INFO, "Statistics cleared\n");
		memset(handle->diag, 0, sizeof(SkyDiagnostics));
		break; // No response
	}

	case VC_CTRL_SET_CONFIG: {
		/*
		 * Set Skylink Configuration
		 */
		char parameter[64], value[64];
		msg[msg_len] = '\0';
		msg[msg_len+1] = '\0';

		strncpy(parameter, (char*)&msg[0], 64);
		parameter[63] = '\0';
		strncpy(value, (char*)&msg[strlen(parameter) + 1], 64);
		value[63] = '\0';

		sky_set_config(handle, parameter, value);
		break; // No response
	}

	case VC_CTRL_GET_CONFIG: {
		/*
		 * Get Skylink Configuration
		 */
		//unsigned int cfg = msg[0]; // TODO: Correct byte-lengths
		unsigned int val = 0xDEADBEEF;

		//if (handle_get_config(handle, cfg, &val) < 0) {
		//	break; // TODO
		//}

		send_control_response(vc, VC_CTRL_CONFIG_RSP, &val, sizeof(val));
		break;
	}

	case VC_CTRL_ARQ_CONNECT: {
		/*
		 * ARQ connect
		 */
		SKY_PRINTF(SKY_DIAG_ARQ, "VC%d ARQ connecting\n", vc);
		sky_vc_wipe_to_arq_init_state(handle->virtual_channels[vc]);
		break; // No response
	}

	case VC_CTRL_ARQ_DISCONNECT: {
		/*
		 * ARQ disconnect
		 */
		SKY_PRINTF(SKY_DIAG_ARQ, "VC%d ARQ disconnecting\n", vc);
		sky_vc_wipe_to_arq_off_state(handle->virtual_channels[vc]);
		break; // No response
	}

	case VC_CTRL_MAC_RESET: {
		/*
		 * MAC/TDD Reset
		 */
		SKY_PRINTF(SKY_DIAG_MAC, "Commanded MAC reset\n");
		mac_reset(handle->mac, sky_get_tick_time());
		break;
	}

	default:
		return -1;
	}

	return 0;
}



int sky_set_config(SkyHandle handle, const char* parameter,  const char *value) {

#define CONFIG_I(param) if (strcmp(parameter, #param) == 0) { handle->conf->param = atoll(value); return 0; }

	CONFIG_I(mac.maximum_window_length_ticks);
	CONFIG_I(mac.minimum_window_length_ticks);
	CONFIG_I(mac.gap_constant_ticks);
	CONFIG_I(mac.tail_constant_ticks);
	CONFIG_I(mac.shift_threshold_ticks);
	CONFIG_I(mac.idle_timeout_ticks);
	CONFIG_I(mac.window_adjust_increment_ticks);
	CONFIG_I(mac.carrier_sense_ticks);
	CONFIG_I(mac.unauthenticated_mac_updates);
	CONFIG_I(mac.window_adjustment_period);
	CONFIG_I(mac.idle_frames_per_window);

	CONFIG_I(arq_timeout_ticks);
	CONFIG_I(arq_idle_frame_threshold);
	CONFIG_I(arq_idle_frames_per_window);

	return -1;
}
