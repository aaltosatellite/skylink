//
// Created by elmore on 19.10.2021.
//

#include "skylink/skylink.h"



static int32_t wrap_ms(int32_t time_ms, int32_t mod){ //This mess is a conversion from C-modulo, to always-positive-modulo.
	return ((time_ms % mod) + mod) % mod;
}


static int valid_window_length(int32_t length){
	if(length < MINIMUM_WINDOW_LENGTH){
		return 0;
	}
	if(length > MAXIMUM_WINDOW_LENGTH){
		return 0;
	}
	return 1;
}


static int32_t set_my_window_length(MACSystem* macSystem, int32_t new_length){
	macSystem->my_window_length = new_length;
	macSystem->cycle_time = macSystem->my_window_length + macSystem->peer_window_length + macSystem->gap_constant*2;
	return 0;
}


static int32_t set_peer_window_length(MACSystem* macSystem, int32_t new_length){
	macSystem->peer_window_length = new_length;
	macSystem->cycle_time = macSystem->my_window_length + macSystem->peer_window_length + macSystem->gap_constant*2;
	return 0;
}


static int32_t set_gap_constant(MACSystem* macSystem, int32_t new_gap_constant){
	macSystem->gap_constant = new_gap_constant;
	macSystem->cycle_time = macSystem->my_window_length + macSystem->peer_window_length + macSystem->gap_constant*2;
	return 0;
}


/*
 * Returns positive integer number of milliseconds of own transmit window remaining,
 * or negative integer signaling the number of milliseconds past the closure.
 */
static int32_t ms_own_window_remaining(MACSystem* macSystem, int32_t now_ms){
	int32_t dt = wrap_ms(now_ms - macSystem->T0_ms, macSystem->cycle_time);
	return macSystem->my_window_length - dt;
}


static int32_t ms_peer_window_remaining(MACSystem* macSystem, int32_t now_ms){
	int32_t dt = wrap_ms(now_ms - (macSystem->T0_ms + macSystem->my_window_length + macSystem->gap_constant), macSystem->cycle_time);
	return macSystem->peer_window_length - dt;
}


int can_send(MACSystem* macSystem, int32_t now_ms){
	return ms_own_window_remaining(macSystem, now_ms) > 0;
}


int mac_update_belief(MACSystem* macSystem, int32_t now_ms, uint16_t mac_length_, uint16_t mac_remaining_){
	int32_t mac_length = (int32_t) mac_length_;
	int32_t mac_remaining = (int32_t) mac_remaining_;
	if(mac_length != macSystem->peer_window_length){
		if (valid_window_length(mac_length)){
			set_peer_window_length(macSystem, mac_length);
		}
		//todo: handle invalid window length assignment.
	}
	int32_t implied_t0_for_me = wrap_ms(now_ms + mac_remaining + macSystem->gap_constant, macSystem->cycle_time);
	macSystem->T0_ms = implied_t0_for_me;
	return 0;
}







