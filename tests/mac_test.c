//
// Created by elmore on 29.11.2021.
//

#include "../src/skylink/mac.h"
#include "../src/skylink/skylink.h"
#include "../src/skylink/conf.h"
#include "../src/skylink/utilities.h"
#include "tools/tools.h"
#include <assert.h>

void tst_carrier_sense(SkyMAC* mac, SkyMACConfig* config);
void tst_update_belief(SkyMAC* mac, SkyMACConfig* config);
void mac_test_cycle();

static int get_cycle(SkyMAC* mac){
	return mac->my_window_length + mac->config->gap_constant_ticks + mac->peer_window_length + mac->config->tail_constant_ticks;
}



void mac_test(int load){
	PRINTFF(0, "[MAC TEST]\n");
	for (int i = 0; i < (load*1000+1); ++i) {
		mac_test_cycle();
		if(i % 1000 == 0){
			PRINTFF(0,"\ti=%d\n", i);
		}
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}


void mac_test_cycle(){
	//initialize config and mac
	SkyMACConfig config;
	config.tail_constant_ticks = randint_i32(3, 1000);
	config.gap_constant_ticks = randint_i32(3, 2000);
	config.minimum_window_length_ticks = randint_i32(3, 300);
	config.maximum_window_length_ticks = randint_i32(config.minimum_window_length_ticks+50, config.minimum_window_length_ticks+ 2000);
	config.shift_threshold_ticks = randint_i32(3, 100000);
	config.unauthenticated_mac_updates = randint_i32(0,1);
	config.carrier_sense_ticks = randint_i32(1,210);
	SkyMAC* mac = sky_mac_create(&config);

	//randomize state variables.
	mac->total_frames_sent_in_current_window = 0;
	mac->vc_round_robin_start = randint_i32(0, SKY_NUM_VIRTUAL_CHANNELS-1);
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		mac->frames_sent_in_current_window_per_vc[i] = randint_i32(0,4);
		mac->total_frames_sent_in_current_window += mac->frames_sent_in_current_window_per_vc[i];
	}

	mac->last_belief_update = randint_i32(0, MOD_TIME_TICKS - 1);
	mac->T0 = randint_i32(0, MOD_TIME_TICKS - 1);
	mac->my_window_length = randint_i32(config.minimum_window_length_ticks, config.maximum_window_length_ticks);
	mac->peer_window_length = randint_i32(config.minimum_window_length_ticks, config.maximum_window_length_ticks);

	for (int i = 0; i < 40; ++i) {
		tst_carrier_sense(mac, &config);
	}

	for (int i = 0; i < 40; ++i) {
		tst_update_belief(mac, &config);
	}
	sky_mac_destroy(mac);
}





void tst_update_belief(SkyMAC* mac, SkyMACConfig* config){
	//PRINTFF(0,".");
	int now = randint_i32(0, MOD_TIME_TICKS - 1);
	int rcvd = randint_i32(2,300);
	int rcv_t = now - rcvd;
	int peer_w = randint_i32(config->minimum_window_length_ticks, config->maximum_window_length_ticks);
	int peer_r = randint_i32(0, peer_w);
	int peer_already_over = (rcv_t + peer_r) < now;
	mac_update_belief(mac, now, rcv_t, peer_w, peer_r);

	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		assert(mac->frames_sent_in_current_window_per_vc[i] == 0);
		assert(mac->total_frames_sent_in_current_window == 0);
	}
	assert(mac->last_belief_update == now);
	assert(mac->peer_window_length == peer_w);

	int rem;
	if(peer_already_over){
		rem = 0;
		//PRINTFF(0,"%d  %d\n", mac->T0- now, get_cycle(mac));
		//PRINTFF(0,"%d  %d\n", mac_time_to_own_window(mac,now), config->tail_constant_ticks);
		assert(mac_time_to_own_window(mac, now) == config->tail_constant_ticks);
	} else {
		rem = rcv_t + peer_r - now;
		//PRINTFF(0,"%d  %d\n", mac_time_to_own_window(mac,now), config->tail_constant_ticks + rem);
		assert(mac_time_to_own_window(mac, now) == (config->tail_constant_ticks + rem));
	}


	int cycle = get_cycle(mac);
	for (int i = 0; i < 20; ++i) {

		//In peer's window
		int s = randint_i32(0, rem);
		int t1 = wrap_time_ticks(now + (randint_i32(0, 1000) * cycle) + s);
		assert(!mac_can_send(mac, t1));
		assert(mac_own_window_remaining(mac, t1) < 0);
		assert(mac_time_to_own_window(mac, t1) > 0);
		assert(mac_peer_window_remaining(mac, t1) >= 0);

		//In tail section
		s = randint_i32(rem, rem + config->tail_constant_ticks -1);
		int t2 = wrap_time_ticks(now + (randint_i32(0, 2000) * cycle) + s);
		assert(!mac_can_send(mac, t2));
		assert(mac_own_window_remaining(mac, t2) < 0);
		assert(mac_time_to_own_window(mac, t2) > 0);
		assert(mac_peer_window_remaining(mac, t2) <= 0);

		//In own window
		s = randint_i32(rem + config->tail_constant_ticks, rem + config->tail_constant_ticks + mac->my_window_length -1);
		int t3 = wrap_time_ticks(now + (randint_i32(0, 2000) * cycle) + s);
		assert(mac_can_send(mac, t3));
		assert(mac_own_window_remaining(mac, t3) >= 0);
		assert(mac_time_to_own_window(mac, t3) == 0);
		assert(mac_peer_window_remaining(mac, t3) < 0);

		//In gap
		s = randint_i32(rem + config->tail_constant_ticks + mac->my_window_length, rem + config->tail_constant_ticks + mac->my_window_length + config->gap_constant_ticks - 1);
		int t4 = wrap_time_ticks(now + (randint_i32(0, 2000) * cycle) + s);
		assert(!mac_can_send(mac, t4));
		assert(mac_own_window_remaining(mac, t4) <= 0);
		assert(mac_time_to_own_window(mac, t4) > 0);
		assert(mac_peer_window_remaining(mac, t4) < 0);
	}

	for (int i = 0; i < 200; ++i) {
		int t1 = wrap_time_ticks(now + randint_i32(0, cycle* 100));
		mac_reset(mac, t1);
		assert(mac_can_send(mac, t1));
		mac_shift_windowing(mac, randint_i32(0, 300));
	}
}


void tst_carrier_sense(SkyMAC* mac, SkyMACConfig* config){
	int now = randint_i32(0, MOD_TIME_TICKS - 1);
	int cycle = get_cycle(mac);
	int x = positive_modulo( wrap_time_ticks(now - mac->T0), cycle);
	if (x < mac->my_window_length){
		x = 0;
	} else {
		x = cycle - x;
	}
	int rem_to_own = mac_time_to_own_window(mac, now);
	assert(x == rem_to_own);
	int acts = (x < mac->config->carrier_sense_ticks);
	sky_mac_carrier_sensed(mac, now);


	if(acts){
		if(mac_time_to_own_window(mac, now) != config->carrier_sense_ticks){
			PRINTFF(0,"carrier sense ticks: %d \n", config->carrier_sense_ticks);
			PRINTFF(0,"time to own: %d\n", mac_time_to_own_window(mac, now));
			PRINTFF(0,"x: %d\n",  x);
			PRINTFF(0,"cycle: %d\n",  cycle);
			PRINTFF(0,"my window: %d\n",  mac->my_window_length);
			PRINTFF(0,"cycle: - my %d\n", cycle - mac->my_window_length);
		}
		assert(mac_time_to_own_window(mac, now) == config->carrier_sense_ticks );
	} else {
		//PRINTFF(0,"2: %d   %d\n", x, cycle - x);
		//PRINTFF(0,"2: %d   %d\n", mac->my_window_length ,mac->peer_window_length);
		//PRINTFF(0,"2: %d   %d\n", (mac->my_window_length + mac->config->gap_constant_ticks), (mac->my_window_length + mac->config->gap_constant_ticks + mac->peer_window_length));
		//PRINTFF(0,"2: %d   %d\n", mac_time_to_own_window(mac, now), config->tail_constant_ticks);
		//PRINTFF(0,"2: %d \n", now);
		//PRINTFF(0,"2: %d  %d\n\n", t00, mac->T0);
		assert(mac_time_to_own_window(mac, now) == x);
	}
}