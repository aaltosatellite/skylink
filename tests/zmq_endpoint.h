//
// Created by elmore on 4/3/22.
//

#ifndef SKYLINK_ZMQ_ENDPOINT_H
#define SKYLINK_ZMQ_ENDPOINT_H

#include "../src/skylink/conf.h"
#include "../src/skylink/diag.h"
#include "../src/skylink/frame.h"
#include "../src/skylink/mac.h"
#include "../src/skylink/utilities.h"
#include "../src/skylink/reliable_vc.h"
#include "../src/skylink/skylink.h"
#include "tools/tools.h"
#include "tst_utilities.h"
#include <zmq.h>


#define STATE_RX  	0
#define STATE_TX  	1

#define TX_PORT			3300
#define RX_PORT			3301
#define PL_WRITE_PORT	7701
#define PL_READ_PORT	7701


typedef struct skylink_peer {
	uint8_t on;
	char physical_state;
	uint8_t pipe_up;
	uint8_t pipe_down;
	pthread_mutex_t mutex;
	pthread_t thread1;
	pthread_t thread2;
	pthread_t thread3;
	int ID;
	SkyHandle self;
	SkyRadioFrame* sendFrame;
	SkyRadioFrame* rcvFrame;
	void* zmq_context;
	void* rx_socket;
	void* tx_socket;
	void* pl_write_socket;
	void* pl_read_socket;

	int failed_send_pushes;
} SkylinkPeer;



typedef struct vc_status {
	int32_t arq_state;
	uint32_t arq_identity;
	int32_t n_to_tx;
	int32_t n_to_resend;
	int32_t tx_full;
	int32_t n_to_read;
	int32_t hmac_tx_seq;
	int32_t hmac_rx_seq;
} VCStatus;


typedef struct skylink_status {
	int32_t my_window;
	int32_t peer_window;
	int32_t own_remaining;
	int32_t peer_remaining;
	int32_t last_mac_update;
	int32_t tick;
	int32_t radio_state;
	VCStatus vcs[4];
} SkylinkStatus;




SkylinkPeer* ep_init_peer(int32_t ID, double relspeed, int baudrate, uint8_t pipe_up, uint8_t pipe_down, uint8_t* cfg, int32_t cfg_len);

void ep_close(SkylinkPeer* peer);

int ep_vc_push_pl(SkylinkPeer* peer, int32_t vc, uint8_t* pl, int32_t pl_len);

int ep_vc_read_pl(SkylinkPeer* peer, int32_t vc, uint8_t* pl, int32_t maxlen);

int ep_vc_init_arq(SkylinkPeer* peer, int32_t vc);

int ep_get_skylink_status(SkylinkPeer* peer, SkylinkStatus* status);


#endif //SKYLINK_ZMQ_ENDPOINT_H
