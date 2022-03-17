//
// Created by elmore on 19.10.2021.
//

#include "skylink/mac.h"
#include "skylink/conf.h"
#include "skylink/frame.h"
#include "skylink/utilities.h"
#include "skylink/platform.h"


static int32_t get_mac_cycle(SkyMAC* mac){
	return mac->my_window_length + mac->gap_constant + mac->peer_window_length + mac->tail_constant;
}

static int32_t wrap_tdd_cycle(tick_t ticks, SkyMAC* mac){ //This mess is a conversion from C-modulo, to always-positive-modulo.
	int32_t mod = get_mac_cycle(mac);
	return positive_modulo_true(ticks, mod);
	//return ((ticks % mod) + mod) % mod;
}


int mac_valid_window_length(SkyMACConfig* config, tick_t length){
	if(length < config->minimum_window_length_ticks){
		return 0;
	}
	if(length > config->maximum_window_length_ticks){
		return 0;
	}
	return 1;
}






// === PUBLIC FUNCTIONS ================================================================================================
SkyMAC* sky_mac_create(SkyMACConfig* config){
	SkyMAC* mac = SKY_MALLOC(sizeof(SkyMAC));
	mac->T0 = 0;
	mac->window_adjust_plan = 0;
	mac->window_on = 0;
	mac->my_window_length = config->minimum_window_length_ticks;
	mac->peer_window_length = config->minimum_window_length_ticks;
	mac->gap_constant = config->gap_constant_ticks;
	mac->tail_constant = config->tail_constant_ticks;
	mac->last_belief_update = 0;
	mac->total_frames_sent_in_current_window = 0;
	mac->vc_round_robin_start = 0;
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		mac->frames_sent_in_current_window_per_vc[i] = 0;
	}
	return mac;
}


void sky_mac_destroy(SkyMAC* mac){
	SKY_FREE(mac);
}


void mac_shift_windowing(SkyMAC* mac, tick_t t_shift){
	t_shift = (t_shift > 0) ? t_shift : -t_shift; //T0 should always stay behind current time. Otherwise time-deltas wrap around the wrong way.
	mac->T0 = wrap_time_ticks(mac->T0 - t_shift);
}



int32_t mac_expand_window(SkyMAC* mac, SkyMACConfig* config){
	mac->my_window_length = mac->my_window_length + config->window_adjust_increment_ticks;
	mac->my_window_length = MIN(mac->my_window_length, config->maximum_window_length_ticks);
	return 0;
}


int32_t mac_shrink_window(SkyMAC* mac, SkyMACConfig* config){
	mac->my_window_length = mac->my_window_length - config->window_adjust_increment_ticks;
	mac->my_window_length = MAX(mac->my_window_length, config->minimum_window_length_ticks);
	return 0;
}


int32_t mac_time_to_own_window(SkyMAC* mac, tick_t now){
	int32_t dt = wrap_tdd_cycle(wrap_time_ticks(now - mac->T0), mac);
	if(dt < mac->my_window_length){
		return 0;
	}
	return get_mac_cycle(mac) - dt;
}


int32_t mac_own_window_remaining(SkyMAC* mac, tick_t now){
	int32_t dt = wrap_tdd_cycle(wrap_time_ticks(now - mac->T0), mac);
	return mac->my_window_length - dt;
}


int32_t mac_peer_window_remaining(SkyMAC* mac, tick_t now){
	int32_t dt = wrap_tdd_cycle(wrap_time_ticks(now - (mac->T0 + mac->my_window_length + mac->gap_constant)), mac);
	return mac->peer_window_length - dt;
}


void mac_silence_shift_check(SkyMAC* mac, SkyMACConfig* config, tick_t now){
	if(wrap_time_ticks(now - mac->last_belief_update) > config->shift_threshold_ticks){
		tick_t shift = ((now & 0b100) != 0) + 1;
		shift = shift * mac->my_window_length;
		mac_shift_windowing(mac, shift);
		mac->last_belief_update = now;
	}
}


int mac_can_send(SkyMAC* mac, tick_t now){
	return mac_own_window_remaining(mac, now) > 0;
}


int mac_update_belief(SkyMAC* mac, SkyMACConfig* config, tick_t now, tick_t peer_mac_length, tick_t peer_mac_remaining){
	if(!mac_valid_window_length(config, peer_mac_length)){
		return SKY_RET_INVALID_MAC_WINDOW_SIZE;
	}
	if(peer_mac_remaining > peer_mac_length){
		return SKY_RET_INVALID_MAC_REMAINING_SIZE;
	}
	if(peer_mac_length != mac->peer_window_length){
		mac->peer_window_length = peer_mac_length;
	}
	int32_t cycle = get_mac_cycle(mac);
	int32_t implied_t0_for_me = wrap_time_ticks(now + peer_mac_remaining + mac->tail_constant - cycle);
	mac->T0 = implied_t0_for_me;
	mac->last_belief_update = now;
	mac->total_frames_sent_in_current_window = 0;
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		mac->frames_sent_in_current_window_per_vc[i] = 0;
	}
	return 0;
}


int sky_mac_carrier_sensed(SkyMAC* mac, SkyMACConfig* config){
	int32_t peer_remaining_priori = mac_peer_window_remaining(mac, sky_get_tick_time());
	if(peer_remaining_priori > 0){
		return 0;
	}
	mac_update_belief(mac, config, sky_get_tick_time(), mac->peer_window_length, 1);
	return 1;
}


int mac_idle_frame_needed(SkyMAC* mac, SkyMACConfig* config, tick_t now){
	int mac_active = wrap_time_ticks(now - mac->last_belief_update) < config->idle_timeout_ticks;
	int idle_frame_needed = mac->total_frames_sent_in_current_window < config->idle_frames_per_window;
	if(mac_active && idle_frame_needed){
		return 1;
	}
	return 0;
}


int mac_set_frame_fields(SkyMAC* mac, SkyRadioFrame* frame, tick_t now){
	uint16_t w = (uint16_t)mac->my_window_length;
	int32_t R = mac_own_window_remaining(mac, now);
	R = (R < 1) ? 1 : R;
	uint16_t r = (uint16_t)R;
	sky_packet_add_extension_mac_tdd_control(frame, w, r);
	return 0;
}
