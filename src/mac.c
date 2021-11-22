//
// Created by elmore on 19.10.2021.
//

#include "skylink/mac.h"
#include "skylink/conf.h"
#include "skylink/frame.h"
#include "skylink/utilities.h"


static int32_t wrap_ms(int32_t time_ms, SkyMAC* mac){ //This mess is a conversion from C-modulo, to always-positive-modulo.
	int32_t mod = mac->my_window_length + mac->gap_constant + mac->peer_window_length + mac->tail_constant;
	return ((time_ms % mod) + mod) % mod;
}


int mac_valid_window_length(SkyMACConfig* config, int32_t length){
	if(length < config->minimum_window_length){
		return 0;
	}
	if(length > config->maximum_window_length){
		return 0;
	}
	return 1;
}






// === PUBLIC FUNCTIONS ================================================================================================
SkyMAC* sky_mac_create(SkyMACConfig* config){
	SkyMAC* mac = SKY_MALLOC(sizeof(SkyMAC));
	mac->T0_ms = 0;
	mac->my_window_length = config->default_window_length;
	mac->peer_window_length = config->default_window_length;
	mac->gap_constant = config->default_gap_length;
	mac->tail_constant = config->default_tail_length;
	mac->last_belief_update = -1;
	mac->total_frames_sent_in_current_window = 0;
	mac->vc_round_robin_start = 0;
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		mac->frames_sent_in_current_window_per_vc[i] = 0;
	}
	return mac;
}


void sky_mac_destroy(SkyMAC* mac){
	free(mac);
}


void mac_shift_windowing(SkyMAC* mac, int32_t t_shift){
	mac->T0_ms = wrap_ms(mac->T0_ms + t_shift, mac);
}


int32_t mac_set_my_window_length(SkyMAC* mac, int32_t new_length){
	mac->my_window_length = new_length;
	return 0;
}


int32_t mac_set_peer_window_length(SkyMAC* mac, int32_t new_length){
	mac->peer_window_length = new_length;
	return 0;
}


int32_t mac_set_gap_constant(SkyMAC* mac, int32_t new_gap_constant){
	mac->gap_constant = new_gap_constant;
	return 0;
}


int32_t mac_time_to_own_window(SkyMAC* mac, int32_t now_ms){
	int32_t dt = wrap_ms(mac->T0_ms - now_ms, mac);
	int32_t length_of_rest = mac->gap_constant + mac->peer_window_length + mac->tail_constant;
	if(dt > length_of_rest){
		return 0;
	}
	return dt;
}


int32_t mac_own_window_remaining(SkyMAC* mac, int32_t now_ms){
	int32_t dt = wrap_ms(now_ms - mac->T0_ms, mac);
	return mac->my_window_length - dt;
}


int32_t mac_peer_window_remaining(SkyMAC* mac, int32_t now_ms){
	int32_t dt = wrap_ms(now_ms - (mac->T0_ms + mac->my_window_length + mac->gap_constant), mac);
	return mac->peer_window_length - dt;
}


void mac_silence_shift_check(SkyMAC* mac, SkyMACConfig* config, int32_t now_ms){
	if(wrap_time_ms(now_ms - mac->last_belief_update) > config->shift_threshold_ms){
		mac->T0_ms = rand() & 0xFFF;
		mac->last_belief_update = now_ms;
	}
}


int mac_can_send(SkyMAC* mac, int32_t now_ms){
	return mac_own_window_remaining(mac, now_ms) > 0;
}


int mac_update_belief(SkyMAC* mac, SkyMACConfig* config, int32_t now_ms, int32_t peer_mac_length, int32_t peer_mac_remaining){
	if(!mac_valid_window_length(config, peer_mac_length)){
		return SKY_RET_INVALID_MAC_WINDOW_SIZE;
	}
	if(peer_mac_length != mac->peer_window_length){
		mac_set_peer_window_length(mac, peer_mac_length);
	}
	int32_t implied_t0_for_me = wrap_ms(now_ms + peer_mac_remaining + mac->tail_constant, mac);
	mac->T0_ms = implied_t0_for_me;
	mac->last_belief_update = now_ms;
	mac->total_frames_sent_in_current_window = 0;
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		mac->frames_sent_in_current_window_per_vc[i] = 0;
	}
	return 0;
}


int sky_mac_carrier_sensed(SkyMAC* mac, SkyMACConfig* config, int32_t now_ms){
	int32_t peer_remaining_priori = mac_peer_window_remaining(mac, now_ms);
	if(peer_remaining_priori > 0){
		return 0;
	}
	mac_update_belief(mac, config, now_ms, mac->peer_window_length, 1);
	return 1;
}


int mac_set_frame_fields(SkyMAC* mac, SkyRadioFrame* frame, int32_t now_ms){
	uint16_t w = (uint16_t)mac->my_window_length;
	int32_t R = mac_own_window_remaining(mac, now_ms);
	R = (R < 1) ? 1 : R;
	uint16_t r = (uint16_t)R;
	sky_packet_add_extension_mac_tdd_control(frame, w, r);
	return 0;
}
