//
// Created by elmore on 4/3/22.
//

#include "zmq_endpoint.h"



int32_t local_time0 = 0;
double relative_time_speed = 0.25;
int tx_baudrate = 9600*2;

static void xassert(int x, int errcode){
	if(!x){
		exit(errcode);
	}
}


static int32_t rget_time_ms(){
	int64_t X = real_microseconds();
	double ms = (double)(X / 1000);
	ms = ms + (double)local_time0;
	ms = ms * relative_time_speed;
	X = (int64_t) ms;
	X = X % 60000000;
	int32_t rms = (int32_t) X;
	rms = rms % MOD_TIME_TICKS;
	return rms;
}

static int rel_sky_tick(){
	int32_t rnow = rget_time_ms();
	return sky_tick(rnow);
}

static void rsleep_us(double us){
	us = us / relative_time_speed;
	us = (int64_t)us;
	sleepus(us);
}




static SkylinkPeer* new_peer(int ID, SkyConfig* config){
	char url[64];
	SkylinkPeer* peer = malloc(sizeof(SkylinkPeer));
	peer->on = 1;
	pthread_mutex_init(&peer->mutex, NULL);
	peer->failed_send_pushes = 0;
	peer->ID = ID;
	peer->rcvFrame = new_frame();
	peer->sendFrame = new_frame();
	peer->zmq_context = zmq_ctx_new();
	peer->self = new_handle(config);
	peer->tx_socket = zmq_socket(peer->zmq_context, ZMQ_PUB);
	peer->rx_socket = zmq_socket(peer->zmq_context, ZMQ_SUB);
	peer->pl_write_socket = zmq_socket(peer->zmq_context, ZMQ_SUB);
	peer->pl_read_socket = zmq_socket(peer->zmq_context, ZMQ_PUB);
	peer->physical_state = STATE_RX;

	mac_shift_windowing(peer->self->mac, randint_i32(100, 10000) );
	peer->self->mac->last_belief_update = sky_get_tick_time();

	sprintf(url, "tcp://127.0.0.1:%d", PL_WRITE_PORT);
	zmq_connect(peer->pl_write_socket, url);
	sleepms(50);
	zmq_setsockopt(peer->pl_write_socket, ZMQ_SUBSCRIBE, &ID, 4);

	sprintf(url, "tcp://127.0.0.1:%d", TX_PORT);
	zmq_connect(peer->tx_socket, url);

	sprintf(url, "tcp://127.0.0.1:%d", PL_READ_PORT);
	zmq_connect(peer->pl_read_socket, url);

	sprintf(url, "tcp://127.0.0.1:%d", RX_PORT);
	zmq_connect(peer->rx_socket, url);
	sleepms(50);
	zmq_setsockopt(peer->rx_socket, ZMQ_SUBSCRIBE, &ID, 4);
	sleepms(50);
	return peer;
}







static void* rx_cycle(void* arg){
	SkylinkPeer* peer = (SkylinkPeer*) arg;
	int32_t timeo = 200;
	uint8_t* tgt = malloc(1000);
	zmq_setsockopt(peer->rx_socket, ZMQ_RCVTIMEO, &timeo, 4);
	while (peer->on){
		int r = zmq_recv(peer->rx_socket, tgt, 1000, 0);
		if((r < 12) || (r > (int)(sizeof(SkyRadioFrame)+4))){
			continue;
		}
		double x = (double)(r-4);
		x = 1000000.0 * x * 8.0 / (double)tx_baudrate;


		int ok = 1;
		if(peer->physical_state != STATE_RX){
			ok = 0;
		}
		for (int i = 0; i < 4; ++i) {
			rsleep_us(x/4 - 10);
			if(peer->physical_state != STATE_RX){
				ok = 0;
			}
		}
		if(!ok){
			//PRINTFF(0,"(%d RCV-miss)\n", peer->ID);
			continue;
		}
		pthread_mutex_lock(&peer->mutex);
		rel_sky_tick();
		memcpy(peer->rcvFrame->raw, tgt+4, r-4);
		peer->rcvFrame->length = r - 4;
		peer->rcvFrame->rx_time_ticks = sky_get_tick_time();
		int rr = sky_rx(peer->self, peer->rcvFrame, 0);
		//PRINTFF(0,"(%d RCV-got: %d)\n", peer->ID, rr);
		if(peer->pipe_down){
			for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
				while(sky_vc_count_readable_rcv_packets(peer->self->virtual_channels[i]) > 0){
					memcpy(tgt, &peer->ID, 4);
					memcpy(tgt+4, &i, 4);
					int r2 = sky_vc_read_next_received(peer->self->virtual_channels[i], tgt+8, 300);
					if(r2 >= 0){
						zmq_send(peer->pl_read_socket, tgt, r2+8, 0);
					}
				}
			}
		}
		pthread_mutex_unlock(&peer->mutex);
	}
	free(tgt);
	return NULL;
}

static void* tx_cycle(void* arg){
	SkylinkPeer* peer = (SkylinkPeer*) arg;
	int32_t timeo = 200;
	uint8_t* tgt = malloc(1000);
	zmq_setsockopt(peer->rx_socket, ZMQ_RCVTIMEO, &timeo, 4);
	int slp = 10;
	while (peer->on){
		rsleep_us(slp * 1000);
		slp = 10;

		pthread_mutex_lock(&peer->mutex);
		rel_sky_tick();
		tick_t now = sky_get_tick_time();
		int r = sky_tx(peer->self, peer->sendFrame, 0);
		if(r){
			peer->physical_state = STATE_TX;
			pthread_mutex_unlock(&peer->mutex);

			double x = peer->sendFrame->length;
			x = 1000000.0 * x * 8.0 / (double)tx_baudrate;
			rsleep_us(x);
			memcpy(tgt, &peer->ID, 4);
			memcpy(tgt+4, peer->sendFrame->raw, peer->sendFrame->length);
			zmq_send(peer->tx_socket, tgt, 4 + peer->sendFrame->length, 0);
			//PRINTFF(0,"(%d SENT)\n", peer->ID);
			pthread_mutex_lock(&peer->mutex);
			peer->physical_state = STATE_RX;

		}
		rel_sky_tick();
		tick_t time_to = mac_time_to_own_window(peer->self->mac, now);
		if(time_to < 10){
			slp = (time_to + 2);
		}
		pthread_mutex_unlock(&peer->mutex);
	}
	free(tgt);
	return NULL;
}



static void* pl_up_cycle(void* arg){
	SkylinkPeer* peer = (SkylinkPeer*) arg;
	int32_t timeo = 200;
	uint8_t* tgt = malloc(1000);
	zmq_setsockopt(peer->pl_write_socket, ZMQ_RCVTIMEO, &timeo, 4);
	while (peer->on){
		int r = zmq_recv(peer->pl_write_socket, tgt, 1000, 0);
		if(r < 5){
			continue;
		}
		uint8_t vc = tgt[4];
		int pl_len = r - 5;
		if(vc > SKY_NUM_VIRTUAL_CHANNELS){
			continue;
		}
		ep_vc_push_pl(peer, vc, tgt+5, pl_len);
	}
	free(tgt);
	return NULL;
}



SkylinkPeer* ep_init_peer(int32_t ID, double relspeed, int32_t baudrate, uint8_t pipe_up, uint8_t pipe_down, uint8_t* cfg, int32_t cfg_len){
	local_time0 = randint_i32(0, 9000000);

	relative_time_speed = relspeed;
	xassert(relative_time_speed > 0.01, 1);
	xassert(relative_time_speed < 3.0, 2);

	tx_baudrate = baudrate;
	xassert(tx_baudrate > 6000, 3);
	xassert(tx_baudrate < 500000, 4);

	xassert(pipe_up < 2, 5);
	xassert(pipe_down < 2, 6);

	if(cfg_len != sizeof(SkyConfig)){
		PRINTFF(0, "Conf packet of incorrect length: %d instead of %d\n", cfg_len, sizeof(SkyConfig));
		exit(5);
	}
	SkyConfig* conf = new_vanilla_config();
	memcpy(conf, cfg, sizeof(SkyConfig));
	memset(conf->identity, 0, sizeof(conf->identity));
	memcpy(conf->identity, &ID, sizeof(ID));
	xassert(conf->mac.maximum_window_length_ticks > 800, 8);
	xassert(conf->mac.minimum_window_length_ticks < 1000, 9);
	xassert(conf->mac.tail_constant_ticks < 200, 10);
	xassert(conf->mac.gap_constant_ticks > 400, 11);
	xassert(conf->mac.unauthenticated_mac_updates < 2, 12);
	xassert(conf->mac.window_adjustment_period < 10, 13);
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		xassert(conf->vc[i].require_authentication <= 0b111, 14);
		xassert(conf->vc[i].rcv_ring_len > 12, 15);
		xassert(conf->vc[i].send_ring_len > 12, 16);
		xassert(conf->vc[i].horizon_width > 1, 17);
		xassert(conf->vc[i].element_size > 40, 18);
	}

	xassert(conf->arq_timeout_ticks > 1000, 19);
	xassert(conf->arq_timeout_ticks < 60000, 20);
	xassert(conf->arq_idle_frame_threshold < conf->arq_timeout_ticks, 21);
	xassert(conf->arq_idle_frame_threshold > 100, 22);
	xassert(conf->arq_idle_frames_per_window < 5, 23);
	xassert(conf->hmac.maximum_jump < 1000, 24);
	xassert(conf->hmac.key_length == 16, 25);


	PRINTFF(0, "Starting peer cycle with ID=%d \n",ID);
	SkylinkPeer* peer = new_peer(ID, conf);
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		peer->self->hmac->sequence_tx[i] = randint_i32(0, HMAC_CYCLE_LENGTH-10);
		peer->self->hmac->sequence_rx[i] = randint_i32(0, HMAC_CYCLE_LENGTH-10);
	}


	peer->pipe_up = pipe_up;
	peer->pipe_down = pipe_down;

	pthread_create(&peer->thread1, NULL, tx_cycle, peer);
	pthread_create(&peer->thread2, NULL, rx_cycle, peer);
	if(peer->pipe_up){
		pthread_create(&peer->thread3, NULL, pl_up_cycle, peer);
	}
	return peer;
}


void ep_close(SkylinkPeer* peer){
	peer->on = 0;
	uint8_t xx[10];
	void* x = &xx;
	pthread_join(peer->thread1, &x);
	pthread_join(peer->thread2, &x);
	if(peer->pipe_up){
		pthread_join(peer->thread3, &x);
	}
	destroy_config(peer->self->conf);
	destroy_handle(peer->self);
	free(peer);
}


int ep_vc_push_pl(SkylinkPeer* peer, int32_t vc, uint8_t* pl, int32_t pl_len){
	if(vc >= SKY_NUM_VIRTUAL_CHANNELS)
		return -1;
	pthread_mutex_lock(&peer->mutex);
	int r = sky_vc_push_packet_to_send(peer->self->virtual_channels[vc], pl, pl_len);
	pthread_mutex_unlock(&peer->mutex);
	return r;
}


int ep_vc_read_pl(SkylinkPeer* peer, int32_t vc, uint8_t* pl, int32_t maxlen){
	if(vc >= SKY_NUM_VIRTUAL_CHANNELS)
		return -1;
	pthread_mutex_lock(&peer->mutex);
	int r = sky_vc_read_next_received(peer->self->virtual_channels[vc], pl, maxlen);
	pthread_mutex_unlock(&peer->mutex);
	return r;
}


int ep_vc_init_arq(SkylinkPeer* peer, int32_t vc){
	if(vc >= SKY_NUM_VIRTUAL_CHANNELS)
		return -1;
	pthread_mutex_lock(&peer->mutex);
	int r = sky_vc_wipe_to_arq_init_state(peer->self->virtual_channels[vc]);
	pthread_mutex_unlock(&peer->mutex);
	return r;
}


static int ep_get_vc_status(SkylinkPeer* peer, int32_t vc, VCStatus* status){
	if(vc >= SKY_NUM_VIRTUAL_CHANNELS)
		return -1;
	SkyVirtualChannel* vchannel = peer->self->virtual_channels[vc];
	status->arq_state = vchannel->arq_state_flag;
	status->arq_identity = vchannel->arq_session_identifier;
	status->n_to_read = sky_vc_count_readable_rcv_packets(vchannel);
	status->n_to_tx = sky_vc_count_packets_to_tx(vchannel, 1);
	status->n_to_resend = status->n_to_tx - sky_vc_count_packets_to_tx(vchannel, 1);
	status->tx_full = sky_vc_send_buffer_is_full(vchannel);
	status->hmac_tx_seq = peer->self->hmac->sequence_tx[vc];
	status->hmac_rx_seq = peer->self->hmac->sequence_rx[vc];
	return 0;
}


int ep_get_skylink_status(SkylinkPeer* peer, SkylinkStatus* status){
	pthread_mutex_lock(&peer->mutex);
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		VCStatus* vcstatus = &status->vcs[i];
		ep_get_vc_status(peer, i, vcstatus);
	}
	status->my_window = peer->self->mac->my_window_length;
	status->peer_window = peer->self->mac->peer_window_length;
	status->own_remaining = mac_own_window_remaining(peer->self->mac, sky_get_tick_time());
	status->peer_remaining = mac_peer_window_remaining(peer->self->mac, sky_get_tick_time());
	status->last_mac_update = peer->self->mac->last_belief_update;
	status->tick = sky_get_tick_time();
	status->radio_state = peer->physical_state;
	pthread_mutex_unlock(&peer->mutex);
	return sizeof(SkylinkStatus);
}






