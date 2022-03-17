//
// Created by elmore on 29.11.2021.
//

#include "../src/skylink/mac.h"
#include "../src/skylink/skylink.h"
#include "../src/skylink/conf.h"
#include "../src/skylink/utilities.h"
#include "tools/tools.h"
#include <assert.h>

void tst_v1(SkyMAC* mac, SkyMACConfig* config);
void mac_test_cycle();

static int get_cycle(SkyMAC* mac){
	return mac->my_window_length + mac->gap_constant + mac->peer_window_length + mac->tail_constant;
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
	SkyMAC* mac = sky_mac_create(&config);

	//randomize state variables.
	mac->total_frames_sent_in_current_window = 0;
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		mac->frames_sent_in_current_window_per_vc[i] = randint_i32(0,4);
		mac->total_frames_sent_in_current_window += mac->frames_sent_in_current_window_per_vc[i];
	}

	mac->last_belief_update = randint_i32(0, MOD_TIME_TICKS - 1);
	mac->T0 = randint_i32(0, MOD_TIME_TICKS - 1);

	for (int i = 0; i < 40; ++i) {
		tst_v1(mac, &config);
	}
	sky_mac_destroy(mac);
}





void tst_v1(SkyMAC* mac, SkyMACConfig* config){
	int t0 = randint_i32(0, MOD_TIME_TICKS - 1);
	int peer_w = randint_i32(config->minimum_window_length_ticks, config->maximum_window_length_ticks);
	int peer_r = randint_i32(0, peer_w);
	mac_update_belief(mac, config, t0, peer_w, peer_r);

	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		assert(mac->frames_sent_in_current_window_per_vc[i] == 0);
		assert(mac->total_frames_sent_in_current_window == 0);
	}

	assert(mac->last_belief_update == t0);
	assert(mac->peer_window_length == peer_w);
	int cycle = get_cycle(mac);
	for (int i = 0; i < 20; ++i) {

		int s = randint_i32(0, peer_r-1);
		int t1 = wrap_time_ticks(t0 + (randint_i32(0, 1000) * cycle) + s);
		assert(!mac_can_send(mac, t1));
		assert(mac_own_window_remaining(mac, t1) < 0);
		assert(mac_time_to_own_window(mac, t1) > 0);
		assert(mac_peer_window_remaining(mac, t1) >= 0);

		s = randint_i32(peer_r, peer_r + mac->tail_constant -1);
		int t2 = wrap_time_ticks(t0 + (randint_i32(0, 2000) * cycle) + s);
		assert(!mac_can_send(mac, t2));
		assert(mac_own_window_remaining(mac, t2) < 0);
		assert(mac_time_to_own_window(mac, t2) > 0);
		assert(mac_peer_window_remaining(mac, t2) <= 0);

		s = randint_i32(peer_r + mac->tail_constant, peer_r + mac->tail_constant + mac->my_window_length -1);
		int t3 = wrap_time_ticks(t0 + (randint_i32(0, 2000) * cycle) + s);
		assert(mac_can_send(mac, t3));
		assert(mac_own_window_remaining(mac, t3) >= 0);
		assert(mac_time_to_own_window(mac, t3) == 0);
		assert(mac_peer_window_remaining(mac, t3) < 0);

		s = randint_i32(peer_r + mac->tail_constant + mac->my_window_length, peer_r + mac->tail_constant + mac->my_window_length + mac->gap_constant -1);
		int t4 = wrap_time_ticks(t0 + (randint_i32(0, 2000) * cycle) + s);
		assert(!mac_can_send(mac, t4));
		assert(mac_own_window_remaining(mac, t4) <= 0);
		assert(mac_time_to_own_window(mac, t4) > 0);
		assert(mac_peer_window_remaining(mac, t4) < 0);
	}

	for (int i = 0; i < 200; ++i) {
		int t1 = wrap_time_ticks(t0 + randint_i32(0, cycle* 100));
		mac_reset(mac, t1);
		assert(mac_can_send(mac, t1));
		mac_shift_windowing(mac, randint_i32(0, 300));
	}
}






