//
// Created by elmore on 4.11.2021.
//

#include <zmq.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "../skylink/arq_ring.h"
#include "../skylink/skylink.h"
#include "../skylink/skypacket.h"
#include "../skylink/utilities.h"
#include "tst_utilities.h"
#include "tools/tools.h"



#define NO_MODE 		0
#define RX_MODE 		1
#define TX_MODE 		2


//extern double relative_time_speed;


typedef struct physical_params {
	double relative_speed;
	double send_speed_bps;
	double switch_delay_ms;
	uint8_t RADIO_MODE;
} PhysicalParams;


typedef struct skylink_peer {
	int ID;
	PhysicalParams physicalParams;
	SkyHandle self;
	SendFrame* sendFrame;
	RCVFrame* rcvFrame;
	void* zmq_context;
	void* rx_socket;
	void* tx_socket;
	void* pl_tx_socket;
	void* pl_rx_socket;
} SkylinkPeer;




double relative_time_speed = 1.0;

int32_t rget_time_ms(){
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	uint64_t ts = t.tv_sec*1000;
	ts += t.tv_nsec/1000000;

	double ft = (double)ts;
	ft = ft * relative_time_speed;
	ft = round(ft);
	ts = (uint64_t)ft;

	return (int32_t) (ts & 0x7FFFFFFF);
}

void rsleep_ms(int64_t ms){
	double ft = (double)ms;
	ft = ft / relative_time_speed;
	double ft_us = round(ft*1000);
	int64_t us = (int64_t)ft_us;
	sleepus(us);
}

void rsleep_us(int64_t us){
	double ft_us = (double)us;
	ft_us = ft_us / relative_time_speed;
	us = (int64_t)ft_us;
	sleepus(us);
}




SkylinkPeer* new_peer(int ID, int tx_port, int rx_port, int pl_rx_port, int pl_tx_port, double relative_speed, double send_speed_bps, double switch_delay_ms){
	char url[64];
	SkylinkPeer* peer = malloc(sizeof(SkylinkPeer));
	peer->ID = ID;
	peer->physicalParams.relative_speed = relative_speed;
	peer->physicalParams.send_speed_bps = send_speed_bps;
	peer->physicalParams.switch_delay_ms = switch_delay_ms;
	peer->physicalParams.RADIO_MODE = 0;

	peer->rcvFrame = new_receive_frame();
	peer->sendFrame = new_send_frame();
	peer->zmq_context = zmq_ctx_new();
	SkyConfig* config = new_vanilla_config();
	peer->self = new_handle(config);
	peer->tx_socket = zmq_socket(peer->zmq_context, ZMQ_PUB);
	peer->rx_socket = zmq_socket(peer->zmq_context, ZMQ_SUB);
	peer->pl_rx_socket = zmq_socket(peer->zmq_context, ZMQ_PUB);
	peer->pl_tx_socket = zmq_socket(peer->zmq_context, ZMQ_SUB);

	sprintf(url, "tcp://127.0.0.1:%d", pl_tx_port);
	zmq_connect(peer->pl_tx_socket, url);
	sleepms(50);
	zmq_setsockopt(peer->pl_tx_socket, ZMQ_SUBSCRIBE, &ID, 4);

	sprintf(url, "tcp://127.0.0.1:%d", tx_port);
	zmq_connect(peer->tx_socket, url);

	sprintf(url, "tcp://127.0.0.1:%d", pl_rx_port);
	zmq_connect(peer->pl_rx_socket, url);

	sprintf(url, "tcp://127.0.0.1:%d", rx_port);
	zmq_connect(peer->rx_socket, url);
	sleepms(50);
	zmq_setsockopt(peer->rx_socket, ZMQ_SUBSCRIBE, &ID, 4);
	return peer;
}


void turn_to_rx(SkylinkPeer* peer){
	if(peer->physicalParams.RADIO_MODE != RX_MODE){
		peer->physicalParams.RADIO_MODE = RX_MODE;
		uint8_t tgt[100];
		memcpy(tgt, &peer->ID, 4);
		tgt[5] = RX_MODE;
		zmq_send(peer->tx_socket, tgt, 5, 0);
	}
}


void turn_to_tx(SkylinkPeer* peer){
	if(peer->physicalParams.RADIO_MODE != TX_MODE){
		peer->physicalParams.RADIO_MODE = TX_MODE;
		uint8_t tgt[100];
		memcpy(tgt, &peer->ID, 4);


		tgt[5] = NO_MODE;
		zmq_send(peer->tx_socket, tgt, 5, 0);


		tgt[5] = TX_MODE;
		zmq_send(peer->tx_socket, tgt, 5, 0);
	}
}


int peer_tx_cycle(SkylinkPeer* peer){
	uint8_t tgt[700];
	memcpy(tgt, &peer->ID, 4);
	for (uint8_t vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; ++vc) {
		int content = sky_tx(peer->self, peer->sendFrame, vc, 1);
		if (content){
			memcpy(tgt+4, peer->sendFrame->radioFrame.raw, peer->sendFrame->radioFrame.length);
			zmq_send(peer->tx_socket, tgt, 4+peer->sendFrame->radioFrame.length, 0); //todo: DONTWAIT?
			return peer->sendFrame->radioFrame.length;
		}
	}
	return 0;
}

int peer_packet_send_cycle(SkylinkPeer* peer){
	uint8_t tgt[700];
	int r = zmq_recv(peer->pl_tx_socket, tgt, 700, ZMQ_DONTWAIT);
	while(r >= 5){
		uint8_t vc = tgt[4];
		skyArray_push_packet_to_send(peer->self->arrayBuffers[vc], tgt+5, r-5);
		r = zmq_recv(peer->pl_tx_socket, tgt, 700, ZMQ_DONTWAIT);
	}
}



int peer_rx_cycle(SkylinkPeer* peer){
	uint8_t tgt[700];
	int r = zmq_recv(peer->rx_socket, tgt, 700, ZMQ_DONTWAIT);
	if(r >= 4){
		memcpy(peer->rcvFrame->radioFrame.raw, tgt+4, r-4);
		peer->rcvFrame->radioFrame.length = r-4;
		sky_rx(peer->self,  peer->rcvFrame,  1);
	}
}



int peer_packet_rcv_cycle(SkylinkPeer* peer){
	uint8_t tgt[700];
	int sequence;
	memcpy(tgt, &peer->ID, 4);
	for (uint8_t vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; ++vc) {
		tgt[4] = vc;
		int n = skyArray_count_readable_rcv_packets(peer->self->arrayBuffers[vc]);
		while (n > 0){
			int r = skyArray_read_next_received(peer->self->arrayBuffers[vc], tgt+5, &sequence);
			zmq_send(peer->pl_rx_socket, tgt, r+5, 0);
			n = skyArray_count_readable_rcv_packets(peer->self->arrayBuffers[vc]);
		}
	}
}


_Noreturn void the_cycle(SkylinkPeer* peer){
	relative_time_speed = peer->physicalParams.relative_speed;
	SkyHandle self = peer->self;

	while (1){
		int32_t now_ms = get_time_ms();
		if(mac_can_send(self->mac, now_ms)){
			turn_to_tx(peer);
			peer_packet_send_cycle(peer);
			int bytes_transmitted = peer_tx_cycle(peer);
			double sleep_time_s = (double)bytes_transmitted / peer->physicalParams.send_speed_bps;
			double sleep_time_us = sleep_time_s * 1000000.0;
			rsleep_us((uint32_t) sleep_time_us);
			continue;
		}

		turn_to_rx(peer);
		peer_rx_cycle(peer);
		peer_packet_rcv_cycle(peer);
		rsleep_us(500);
	}



}








