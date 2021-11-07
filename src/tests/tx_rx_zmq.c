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
#include "../skylink/frame.h"
#include "../skylink/utilities.h"
#include "tst_utilities.h"
#include "tools/tools.h"



#define RX_MODE 		1
#define TX_MODE 		2


//extern double relative_time_speed;


typedef struct physical_params {
	double relative_speed;
	double send_speed_bps;
	double switch_delay_ms;
	uint8_t RADIO_MODE;
	int frames_sent_this_cycle;
} PhysicalParams;


typedef struct skylink_peer {
	pthread_mutex_t mutex;
	pthread_t thread1;
	pthread_t thread2;
	pthread_t thread3;
	int ID;
	PhysicalParams physicalParams;
	SkyHandle self;
	SendFrame* sendFrame;
	RCVFrame* rcvFrame;
	void* zmq_context;
	void* rx_socket;
	void* tx_socket;
	void* pl_write_socket;
	void* pl_read_socket;
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



SkylinkPeer* new_peer(int ID, int tx_port, int rx_port, int pl_write_port, int pl_read_port, double relative_speed, double send_speed_bps, double switch_delay_ms){
	char url[64];
	SkylinkPeer* peer = malloc(sizeof(SkylinkPeer));
	pthread_mutex_init(&peer->mutex, NULL);
	peer->ID = ID;
	peer->physicalParams.relative_speed = relative_speed;
	peer->physicalParams.send_speed_bps = send_speed_bps;
	peer->physicalParams.switch_delay_ms = switch_delay_ms;
	peer->physicalParams.RADIO_MODE = RX_MODE;

	peer->rcvFrame = new_receive_frame();
	peer->sendFrame = new_send_frame();
	peer->zmq_context = zmq_ctx_new();
	SkyConfig* config = new_vanilla_config();
	peer->self = new_handle(config);
	peer->tx_socket = zmq_socket(peer->zmq_context, ZMQ_PUB);
	peer->rx_socket = zmq_socket(peer->zmq_context, ZMQ_SUB);
	peer->pl_write_socket = zmq_socket(peer->zmq_context, ZMQ_SUB);
	peer->pl_read_socket = zmq_socket(peer->zmq_context, ZMQ_PUB);

	mac_shift_windowing(peer->self->mac, randint_i32(100, 10000) );

	sprintf(url, "tcp://127.0.0.1:%d", pl_write_port);
	zmq_connect(peer->pl_write_socket, url);
	sleepms(50);
	zmq_setsockopt(peer->pl_write_socket, ZMQ_SUBSCRIBE, &ID, 4);

	sprintf(url, "tcp://127.0.0.1:%d", tx_port);
	zmq_connect(peer->tx_socket, url);

	sprintf(url, "tcp://127.0.0.1:%d", pl_read_port);
	zmq_connect(peer->pl_read_socket, url);

	sprintf(url, "tcp://127.0.0.1:%d", rx_port);
	zmq_connect(peer->rx_socket, url);
	sleepms(50);
	zmq_setsockopt(peer->rx_socket, ZMQ_SUBSCRIBE, &ID, 4);
	sleepms(50);
	return peer;
}



void turn_to_rx(SkylinkPeer* peer){
	if(peer->physicalParams.RADIO_MODE != RX_MODE){
		peer->physicalParams.RADIO_MODE = RX_MODE;
		PRINTFF(0,"\n=== RX ================================\n");
	}
}



void turn_to_tx(SkylinkPeer* peer){
	if(peer->physicalParams.RADIO_MODE != TX_MODE){
		peer->physicalParams.RADIO_MODE = TX_MODE;
		PRINTFF(0,"\n=== TX ================================\n");
		peer->physicalParams.frames_sent_this_cycle = 0;
	}
}



int peer_tx_round(SkylinkPeer* peer){
	uint8_t tgt[700];
	memcpy(tgt, &peer->ID, 4);
	int any_content = any_content_to_send(peer->self);
	for (uint8_t vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; ++vc) {
		int content = content_to_send(peer->self, vc);
		int b = (peer->physicalParams.frames_sent_this_cycle < 2) && (any_content == 0);
		if (content || b){
			int cc = sky_tx(peer->self, peer->sendFrame, vc, 1);
			assert(cc == content);
			peer->physicalParams.frames_sent_this_cycle++;
			memcpy(tgt+4, peer->sendFrame->radioFrame.raw, peer->sendFrame->radioFrame.length);
			zmq_send(peer->tx_socket, tgt, 4+peer->sendFrame->radioFrame.length, 0); //todo: DONTWAIT?
			PRINTFF(0,"#2 Transmitted.\n");
			return peer->sendFrame->radioFrame.length;
		}
	}
	return 0;
}










void peer_packets_from_ring_to_zmq(SkylinkPeer* peer){
	uint8_t tgt[700];
	int sequence;
	memcpy(tgt, &peer->ID, 4);
	for (uint8_t vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; ++vc) {
		tgt[4] = vc;
		int n = skyArray_count_readable_rcv_packets(peer->self->arrayBuffers[vc]);
		while (n > 0){
			int r = skyArray_read_next_received(peer->self->arrayBuffers[vc], tgt+5, &sequence);
			zmq_send(peer->pl_read_socket, tgt, r + 5, 0);
			PRINTFF(0,"#4 packets sent to read socket.\n");
			n = skyArray_count_readable_rcv_packets(peer->self->arrayBuffers[vc]);
		}
	}
}




void* write_to_send_cycle(void* arg){
	SkylinkPeer* peer = arg;
	uint8_t tgt[1000];
	int32_t timeo = 200;
	zmq_setsockopt(peer->pl_write_socket, ZMQ_RCVTIMEO, &timeo, sizeof(int32_t));
	while (1) {
		int r = zmq_recv(peer->pl_write_socket, tgt, 1000, 0);
		if (r >= 5) {
			uint8_t vc = tgt[4];
			if (vc >= SKY_NUM_VIRTUAL_CHANNELS) {
				PRINTFF(0, "\n\n=== ERROR! WRONG VC:%d ====\n", vc);
				quick_exit(2);
			}
			pthread_mutex_lock(&peer->mutex);    //lock
			skyArray_push_packet_to_send(peer->self->arrayBuffers[vc], &tgt[5], r - 5);
			PRINTFF(0, "#1 payload written.\n");
			pthread_mutex_unlock(&peer->mutex);    //unlock
		}
	}
}




_Noreturn void* tx_cycle(void* arg){
	SkylinkPeer* peer = arg;
	int64_t cycle = -1;
	pthread_mutex_lock(&peer->mutex); 	//lock
	while (1){
		cycle++;
		if(cycle % 50 == 0){
			PRINTFF(0, "s Cycle %ld.\n", cycle);
		}

		int32_t now_ms = rget_time_ms();
		int can_send = mac_can_send(peer->self->mac, now_ms);
		if(can_send){
			turn_to_tx(peer);

			int bytes_transmitted = peer_tx_round(peer);
			if(bytes_transmitted){
				double sleep_time_s = (double)bytes_transmitted / peer->physicalParams.send_speed_bps;
				double sleep_time_us = sleep_time_s * 1000000.0;
				pthread_mutex_unlock(&peer->mutex); //unlock
				rsleep_us((uint32_t) sleep_time_us);
				pthread_mutex_lock(&peer->mutex); 	//lock
			} else {
				pthread_mutex_unlock(&peer->mutex); //unlock
				rsleep_us((uint32_t) 1200);
				pthread_mutex_lock(&peer->mutex); 	//lock
			}
		}

		if(!can_send){
			turn_to_rx(peer);

			int32_t sleep_ms = mac_time_to_own_window(peer->self->mac, now_ms);
			pthread_mutex_unlock(&peer->mutex); //unlock
			if(sleep_ms > 0){
				assert(mac_own_window_remaining(peer->self->mac, now_ms) <= 0);
				rsleep_ms(sleep_ms);
			}
			pthread_mutex_lock(&peer->mutex); 	//lock
		}
	}
}




void* ether_cycle(void* arg){
	SkylinkPeer* peer = arg;
	int64_t cycle = -1;
	uint8_t tgt[1000];
	int32_t timeo = 200;
	zmq_setsockopt(peer->rx_socket, ZMQ_RCVTIMEO, &timeo, sizeof(int32_t));
	while (1){
		cycle++;
		if(cycle % 50 == 0){
			PRINTFF(0, "e Cycle %ld.\n", cycle);
		}

		int r = zmq_recv(peer->rx_socket, tgt, 1000, 0);

		if(r>=4){
			if(r > 255){
				PRINTFF(0,"\n=== ERROR! TOO ONG RX-PACKET! (%d) ===\n",r);
				quick_exit(3);
			}

			pthread_mutex_lock(&peer->mutex); 	//lock
			PRINTFF(0,"#3   %d bytes rx'ed.\n", r);
			if(peer->physicalParams.RADIO_MODE == RX_MODE){
				memcpy(&peer->rcvFrame->radioFrame, &tgt[4], r-4);
				peer->rcvFrame->radioFrame.length = r-4;
				int rxr = sky_rx(peer->self, peer->rcvFrame, 1);
				PRINTFF(0,"#3.5 bytes successfully rx'ed. rx ret: %d\n", rxr);
				peer_packets_from_ring_to_zmq(peer);
			}
			pthread_mutex_unlock(&peer->mutex); //unlock
		}
	}
}




void tx_rx_zmq_test(int argc, char *argv[]){
	if(argc != 2){
		PRINTFF(0,"INVALID ARGUMENTS (%d)!\n", argc);
		return;
	}
	int ID = argv[1][0] - 48;
	PRINTFF(0, "Starting peer cycle with ID=%d \n",ID);
	SkylinkPeer* peer = new_peer(ID, 4440, 4441, 4442, 4443, 0.2, 10024, 0.0 );
	relative_time_speed = peer->physicalParams.relative_speed;

	pthread_create(&peer->thread1, NULL, tx_cycle, peer);
	pthread_create(&peer->thread2, NULL, ether_cycle, peer);
	pthread_create(&peer->thread3, NULL, write_to_send_cycle, peer);

	for (int i = 0; i < 10000; ++i) {
		sleepms(1000);
	}

}








