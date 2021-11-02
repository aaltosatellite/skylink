//
// Created by elmore on 19.10.2021.
//

#include "skylink/mac_2.h"
#include "skylink/conf.h"


static int32_t wrap_ms(int32_t time_ms, MACSystem* macSystem){ //This mess is a conversion from C-modulo, to always-positive-modulo.
	int32_t mod = macSystem->my_window_length + macSystem->gap_constant + macSystem->peer_window_length + macSystem->tail_constant;
	return ((time_ms % mod) + mod) % mod;
}


int mac_valid_window_length(SkyMACConfig* config, int32_t length){
	if(length < config->maximum_window_size){
		return 0;
	}
	if(length > config->minimum_window_size){
		return 0;
	}
	return 1;
}

int mac_valid_gap_length(SkyMACConfig* config, int32_t length){
	if(length < config->maximum_gap_size){
		return 0;
	}
	if(length > config->minimum_gap_size){
		return 0;
	}
	return 1;
}





// === PUBLIC FUNCTIONS ================================================================================================
MACSystem* new_mac_system(SkyMACConfig* config){
	MACSystem* macSystem = SKY_MALLOC(sizeof(MACSystem));
	macSystem->T0_ms = 0;
	macSystem->my_window_length = config->default_window_length;
	macSystem->peer_window_length = config->default_window_length;
	macSystem->gap_constant = config->default_gap_length;
	macSystem->tail_constant = config->default_tail_length;
	return macSystem;
}


void destroy_mac_system(MACSystem* macSystem){
	free(macSystem);
}


int32_t mac_set_my_window_length(MACSystem* macSystem, int32_t new_length){
	macSystem->my_window_length = new_length;
	return 0;
}


int32_t mac_set_peer_window_length(MACSystem* macSystem, int32_t new_length){
	macSystem->peer_window_length = new_length;
	return 0;
}


int32_t mac_set_gap_constant(MACSystem* macSystem, int32_t new_gap_constant){
	macSystem->gap_constant = new_gap_constant;
	return 0;
}


int32_t mac_own_window_remaining(MACSystem* macSystem, int32_t now_ms){
	int32_t dt = wrap_ms(now_ms - macSystem->T0_ms, macSystem);
	return macSystem->my_window_length - dt;
}


int32_t mac_peer_window_remaining(MACSystem* macSystem, int32_t now_ms){
	int32_t dt = wrap_ms(now_ms - (macSystem->T0_ms + macSystem->my_window_length + macSystem->gap_constant), macSystem);
	return macSystem->peer_window_length - dt;
}


int mac_can_send(MACSystem* macSystem, int32_t now_ms){
	return mac_own_window_remaining(macSystem, now_ms) > 0;
}


int mac_update_belief(MACSystem* macSystem, SkyMACConfig* config, int32_t now_ms, int32_t peer_mac_length, int32_t peer_mac_remaining){
	if(!mac_valid_window_length(config, peer_mac_length)){
		return -1;
	}
	if(peer_mac_length != macSystem->peer_window_length){
		mac_set_peer_window_length(macSystem, peer_mac_length);
	}
	int32_t implied_t0_for_me = wrap_ms(now_ms + peer_mac_remaining + macSystem->tail_constant, macSystem);
	macSystem->T0_ms = implied_t0_for_me;
	return 0;
}


int mac_carrier_sensed(MACSystem* macSystem, SkyMACConfig* config, int32_t now_ms){
	int32_t peer_remaining_priori = mac_peer_window_remaining(macSystem, now_ms);
	if(peer_remaining_priori > 0){
		return 0;
	}
	mac_update_belief(macSystem, config, now_ms, macSystem->peer_window_length, 1);
	return 1;
}


int mac_stamp_packet_bytes(MACSystem* macSystem, uint8_t* tgt, int32_t now_ms){
	uint16_t w = (uint16_t)macSystem->my_window_length;
	int32_t R = mac_own_window_remaining(macSystem, now_ms);
	uint16_t r = (uint16_t)R;
	w = sky_hton16(w);
	r = sky_hton16(r);
	memcpy(tgt, &w, sizeof(uint16_t));
	memcpy(tgt+sizeof(uint16_t), &r, sizeof(uint16_t));
	return 0;
}


int mac_set_frame_fields(MACSystem* macSystem, RadioFrame2* frame, int32_t now_ms){
	uint16_t w = (uint16_t)macSystem->my_window_length;
	int32_t R = mac_own_window_remaining(macSystem, now_ms);
	uint16_t r = (uint16_t)R;
	frame->mac_window = w;
	frame->mac_remaining = r;
	return 0;
}

