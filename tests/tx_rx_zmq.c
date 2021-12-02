//
// Created by elmore on 4.11.2021.
//

#include <zmq.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "skylink/reliable_vc.h"
#include "skylink/skylink.h"
#include "skylink/utilities.h"
#include "skylink/frame.h"

#include "tst_utilities.h"
#include "tools/tools.h"





//extern double relative_time_speed;


typedef struct physical_params {
	double relative_speed;
	double send_speed_bps;
	uint8_t RADIO_MODE;
} PhysicalParams;


typedef struct skylink_peer {
	pthread_mutex_t mutex;
	pthread_t thread1;
	pthread_t thread2;
	pthread_t thread3;
	int ID;
	PhysicalParams physicalParams;
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




double relative_time_speed = 0.2;

int32_t rget_time_ms(){
	int64_t X = real_microseconds();
	double ms = (double)(X / 1000);
	ms = ms * relative_time_speed;
	X = (int64_t) ms;
	X = X % 60000000;
	int32_t rms = (int32_t) X;
	rms = rms % MOD_TIME_MS;
	return rms;
}


void rsleep_us(int64_t us){
	double ft_us = (double)us;
	ft_us = ft_us / relative_time_speed;
	us = (int64_t)ft_us;
	sleepus(us);
}



SkylinkPeer* new_peer(int ID, int tx_port, int rx_port, int pl_write_port, int pl_read_port, double relative_speed, double send_speed_bps){
	char url[64];
	SkylinkPeer* peer = malloc(sizeof(SkylinkPeer));
	pthread_mutex_init(&peer->mutex, NULL);
	peer->failed_send_pushes = 0;
	peer->ID = ID;
	peer->physicalParams.relative_speed = relative_speed;
	peer->physicalParams.send_speed_bps = send_speed_bps;


	peer->rcvFrame = new_frame();
	peer->sendFrame = new_frame();
	peer->zmq_context = zmq_ctx_new();
	SkyConfig* config = new_vanilla_config();
	memcpy(config->identity, &ID, 4);
	peer->self = new_handle(config);
	peer->tx_socket = zmq_socket(peer->zmq_context, ZMQ_PUB);
	peer->rx_socket = zmq_socket(peer->zmq_context, ZMQ_SUB);
	peer->pl_write_socket = zmq_socket(peer->zmq_context, ZMQ_SUB);
	peer->pl_read_socket = zmq_socket(peer->zmq_context, ZMQ_PUB);

	mac_shift_windowing(peer->self->mac, randint_i32(100, 10000) );
	peer->self->mac->last_belief_update = rget_time_ms();


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



void say_rx(SkylinkPeer* peer){
	if(peer->physicalParams.RADIO_MODE != MODE_RX){
		peer->physicalParams.RADIO_MODE = MODE_RX;
		PRINTFF(0,"\n=== RX ================================\n");
		uint8_t msg[6];
		memcpy(msg, &peer->ID, 4);
		msg[4] = 0;
		zmq_send(peer->tx_socket, msg, 5, 0);
	}
}



void say_tx(SkylinkPeer* peer){
	if(peer->physicalParams.RADIO_MODE != MODE_TX){
		peer->physicalParams.RADIO_MODE = MODE_TX;
		PRINTFF(0,"\n=== TX ================================\n");
		uint8_t msg[6];
		memcpy(msg, &peer->ID, 4);
		msg[4] = 1;
		zmq_send(peer->tx_socket, msg, 5, 0);
	}
}















// == CYCLE 1: WRITE TO SENDING RING BUFFER ============================================================================
// =====================================================================================================================
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
			int ret = sky_vc_push_packet_to_send(peer->self->arrayBuffers[vc], &tgt[5], r - 5);
			if(ret < 0){
				peer->failed_send_pushes++;
			}
			PRINTFF(0, "#1 payload written.\n");
			pthread_mutex_unlock(&peer->mutex);    //unlock
		}
	}
}
// =====================================================================================================================
// =====================================================================================================================




// == CYCLE 2: TRANSMIT & MOVE BETWEEN STATES ==========================================================================
// =====================================================================================================================
_Noreturn void* tx_cycle(void* arg){
	SkylinkPeer* peer = arg;
	int64_t cycle = -1;
	uint8_t tx_tgt[500];
	memcpy(tx_tgt, &peer->ID, 4);
	pthread_mutex_lock(&peer->mutex); 	//lock
	while (1){
		cycle++;
		if(cycle % 1000 == 0){
			PRINTFF(0,"==========================\n");
			PRINTFF(0,"Identifier: %d\n", peer->self->arrayBuffers[0]->arq_session_identifier);
			PRINTFF(0,"Failed send pushed: %d \n", peer->failed_send_pushes);
			PRINTFF(0,"==========================\n");
			//PRINTFF(0, "s Cycle %ld.\n", cycle);
		}


		int32_t now_ms = rget_time_ms();
		int send = sky_tx(peer->self, peer->sendFrame, 1, now_ms);
		if(send){
			say_tx(peer);
			memcpy(tx_tgt+4, peer->sendFrame->raw, peer->sendFrame->length);
			zmq_send(peer->tx_socket, tx_tgt, 4+peer->sendFrame->length, 0); //todo: DONTWAIT?
			PRINTFF(0,"#2 Transmitted. %dth in this window.\n", peer->self->mac->total_frames_sent_in_current_window);
			int bytes_transmitted = peer->sendFrame->length;
			double sleep_time_s = (double)bytes_transmitted / peer->physicalParams.send_speed_bps;
			double sleep_time_us = sleep_time_s * 1000000.0;
			pthread_mutex_unlock(&peer->mutex); //unlock
			rsleep_us((uint32_t) sleep_time_us);
			pthread_mutex_lock(&peer->mutex); 	//lock
		} else {
			say_rx(peer);
			pthread_mutex_unlock(&peer->mutex); //unlock
			rsleep_us((uint32_t) 1000);
			pthread_mutex_lock(&peer->mutex); 	//lock
		}


	}
}
// =====================================================================================================================
// =====================================================================================================================



// == CYCLE 3: RECEIVE FROM ETHER AND PROCESS ==========================================================================
// =====================================================================================================================
void peer_packets_from_ring_to_zmq(SkylinkPeer* peer){
	uint8_t tgt[700];
	int sequence;
	memcpy(tgt, &peer->ID, 4);
	for (uint8_t vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; ++vc) {
		tgt[4] = vc;
		int n = sky_vc_count_readable_rcv_packets(peer->self->arrayBuffers[vc]);
		while (n > 0){
			int r = sky_vc_read_next_received(peer->self->arrayBuffers[vc], tgt + 5, &sequence);
			zmq_send(peer->pl_read_socket, tgt, r + 5, 0);
			PRINTFF(0,"#4 packets sent to read socket.\n");
			n = sky_vc_count_readable_rcv_packets(peer->self->arrayBuffers[vc]);
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
			//PRINTFF(0, "e Cycle %ld.\n", cycle);
		}

		int r = zmq_recv(peer->rx_socket, tgt, 1000, 0);

		if(r>=4){
			if(r > 255){
				PRINTFF(0,"\n=== ERROR! TOO ONG RX-PACKET! (%d) ===\n",r);
				quick_exit(3);
			}
			//int64_t us1 = real_microseconds();
			pthread_mutex_lock(&peer->mutex); 	//lock
			//int64_t us2 = real_microseconds();
			//PRINTFF(0,"#3   %d bytes rx'ed.  (locked in %ld us)\n", r, us2-us1);
			if(peer->physicalParams.RADIO_MODE == MODE_RX){
				memcpy(peer->rcvFrame->raw, &tgt[4], r-4);
				peer->rcvFrame->length = r-4;
				peer->rcvFrame->rx_time_ms = rget_time_ms();
				int rxr = sky_rx(peer->self, peer->rcvFrame, 1);
				PRINTFF(0,"#3 %d bytes successfully rx'ed. rx ret: %d\n", r, rxr);
				peer_packets_from_ring_to_zmq(peer);
			} else {
				PRINTFF(0,"#3.2 >>>>>>>>>>>>>>>>>> !MISSED RX PACKET! <<<<<<<<<<<<<<<<<<<<<<<\n");
			}
			pthread_mutex_unlock(&peer->mutex); //unlock
		}
	}
}
// =====================================================================================================================
// =====================================================================================================================



int main(int argc, char *argv[]){
	if(argc != 2){
		PRINTFF(0,"INVALID ARGUMENTS (%d)!\n", argc);
		return -1;
	}
	int ID = argv[1][0] - 48;
	PRINTFF(0, "Starting peer cycle with ID=%d \n",ID);
	SkylinkPeer* peer = new_peer(ID, 4440, 4441, 4442, 4443, 0.2, 4*1200);
	if(ID == 5){
		int32_t now_ms = rget_time_ms();
		sky_vc_wipe_to_arq_init_state(peer->self->arrayBuffers[0], now_ms);
	}
	relative_time_speed = peer->physicalParams.relative_speed;

	pthread_create(&peer->thread1, NULL, tx_cycle, peer);
	pthread_create(&peer->thread2, NULL, ether_cycle, peer);
	pthread_create(&peer->thread3, NULL, write_to_send_cycle, peer);

	for (int i = 0; i < 10000; ++i) {
		sleepms(1000);
	}
	return 0;
}
