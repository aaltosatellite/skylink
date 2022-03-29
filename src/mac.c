//
// Created by elmore on 19.10.2021.
//

#include "skylink/diag.h"
#include "skylink/mac.h"
#include "skylink/conf.h"
#include "skylink/frame.h"
#include "skylink/utilities.h"
#include "skylink/platform.h"


/*
 * Calculate the total length of TDD cycle in ticks
 */
static tick_t get_mac_cycle(SkyMAC* mac){
	return mac->my_window_length + mac->peer_window_length + mac->config->tail_constant_ticks;
}

/*
 * Calculate
 */
static tick_t wrap_tdd_cycle(SkyMAC* mac, tick_t ticks) {
	// This mess is a conversion from C-modulo, to always-positive-modulo.
#if 1
	tick_t mod = get_mac_cycle(mac);
	return positive_modulo_true(ticks, mod);
#else
	return ((ticks % mod) + mod) % mod;
#endif
}


int mac_valid_window_length(SkyMAC* mac, tick_t length){
	if(length < mac->config->minimum_window_length_ticks){
		return 0;
	}
	if(length > mac->config->maximum_window_length_ticks){
		return 0;
	}
	return 1;
}






// === PUBLIC FUNCTIONS ================================================================================================
SkyMAC* sky_mac_create(SkyMACConfig* config) {

	// Make sure the min/max windows size make sense.
	if (config->minimum_window_length_ticks > config->maximum_window_length_ticks)
		config->maximum_window_length_ticks = config->minimum_window_length_ticks;

	// Limit number of MAC idle frames
	if (config->idle_frames_per_window > 3)
		config->idle_frames_per_window = 3;

	// Limit MAC timeout time
	if (config->idle_timeout_ticks < 1000 || config->idle_timeout_ticks > 2000)
		config->idle_timeout_ticks = 10000;

	if (config->carrier_sense_ticks > 250)
		config->carrier_sense_ticks = 250;

	// Limit window adjusting period
	if (config->window_adjustment_period < 1)
		config->window_adjustment_period = 1;
	if (config->window_adjustment_period > 4)
		config->window_adjustment_period = 4;


	SkyMAC* mac = SKY_MALLOC(sizeof(SkyMAC));
	mac->config = config;
	mac->T0 = 0;
	mac->window_adjust_counter = 0;
	mac->window_on = 0;
	mac->my_window_length = config->minimum_window_length_ticks;
	mac->peer_window_length = config->minimum_window_length_ticks;
	mac->last_belief_update = 0;
	mac->total_frames_sent_in_current_window = 0;
	mac->vc_round_robin_start = 0;
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		mac->frames_sent_in_current_window_per_vc[i] = 0;
	}
	return mac;
}


void sky_mac_destroy(SkyMAC* mac) {
	SKY_FREE(mac);
}


void mac_shift_windowing(SkyMAC* mac, tick_t t_shift) {
	// T0 should always stay behind current time. Otherwise time-deltas wrap around the wrong way.
	t_shift = (t_shift > 0) ? t_shift : -t_shift;
	mac->T0 = wrap_time_ticks(mac->T0 - t_shift);
}



void mac_expand_window(SkyMAC* mac) {
	mac->my_window_length += mac->config->window_adjust_increment_ticks;
	if (mac->my_window_length < mac->config->maximum_window_length_ticks)
		mac->my_window_length = mac->config->maximum_window_length_ticks;
}


void mac_shrink_window(SkyMAC* mac) {
	mac->my_window_length -= mac->config->window_adjust_increment_ticks;
	if (mac->my_window_length > mac->config->minimum_window_length_ticks)
		mac->my_window_length = mac->config->minimum_window_length_ticks;
}


int32_t mac_time_to_own_window(SkyMAC* mac, tick_t now) {
	int32_t dt = wrap_tdd_cycle(mac, wrap_time_ticks(now - mac->T0));
	if (dt < mac->my_window_length) // Our window is already open
		return 0;
	return get_mac_cycle(mac) - dt; // Peer's windows is open. Return time the end of window
}


int32_t mac_own_window_remaining(SkyMAC* mac, tick_t now) {
	int32_t dt = wrap_tdd_cycle(mac, wrap_time_ticks(now - mac->T0));
	return mac->my_window_length - dt;
}


int32_t mac_peer_window_remaining(SkyMAC* mac, tick_t now) {
	tick_t peer_window_starts = mac->T0 + mac->my_window_length + mac->config->gap_constant_ticks;
	int32_t dt = wrap_tdd_cycle(mac, wrap_time_ticks(now - peer_window_starts));
	return mac->peer_window_length - dt;
}


void mac_silence_shift_check(SkyMAC* mac, tick_t now) {
	if (mac->config->shift_threshold_ticks != 0 &&
		wrap_time_ticks(now - mac->last_belief_update) > mac->config->shift_threshold_ticks)
	{
		/*
		 * The MAC has been open too long shift the window by random
		 * amount to make sure windows are not overlapping.
		 */
		tick_t shift = ((now & 0b100) != 0) + 1; // "Random" shift based on the tick count
		shift = shift * mac->my_window_length;
		mac_shift_windowing(mac, shift);
		mac->last_belief_update = now;
	}
}


bool mac_can_send(SkyMAC* mac, tick_t now) {
	return mac_own_window_remaining(mac, now) > 0;
}


void mac_update_belief(SkyMAC* mac, tick_t receive_time, tick_t peer_mac_length, tick_t peer_mac_remaining) {

	// Limit the window length inside limits.
	// Don't fail completely here and incapacitate the MAC logic, because this can be a result of minor missconfiguration.
	if (peer_mac_length < mac->config->minimum_window_length_ticks)
		peer_mac_length = mac->config->minimum_window_length_ticks;
	else if (peer_mac_length > mac->config->maximum_window_length_ticks)
		peer_mac_length = mac->config->maximum_window_length_ticks;

	if (peer_mac_remaining > peer_mac_length)
		peer_mac_remaining = peer_mac_length;


	const int32_t cycle = get_mac_cycle(mac);
	const tick_t now = sky_get_tick_time();

	tick_t implied_t0_for_me = wrap_time_ticks(receive_time + peer_mac_remaining + mac->config->tail_constant_ticks - cycle);
	tick_t minimum_t0 = wrap_time_ticks(now + mac->config->tail_constant_ticks);
	mac->T0 = MAX(implied_t0_for_me, minimum_t0);
	mac->last_belief_update = now;
	mac->peer_window_length = peer_mac_length;

	SKY_PRINTF(SKY_DIAG_MAC | SKY_DIAG_DEBUG, "MAC Update belief: peer window length: %d, remaining %d, time to t0: %d",
		peer_mac_length, peer_mac_remaining, mac_time_to_own_window(mac, sky_get_tick_time()));

	// Reset frame counts
	mac->total_frames_sent_in_current_window = 0;
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		mac->frames_sent_in_current_window_per_vc[i] = 0;
	}

}


void mac_reset(SkyMAC* mac, tick_t now) {
	mac->T0 = wrap_time_ticks(now - get_mac_cycle(mac));
	mac->window_adjust_counter = 0;
	mac->my_window_length = mac->config->minimum_window_length_ticks;
}


void sky_mac_carrier_sensed(SkyMAC* mac, tick_t now) {
#if 1
	tick_t minimum_t0 = wrap_time_ticks(now + mac->config->carrier_sense_ticks);
	if (minimum_t0 > mac->T0)
		mac->T0 = minimum_t0;
#else
	int32_t peer_remaining_priori = mac_peer_window_remaining(mac, now);
	if(peer_remaining_priori <= 0)
		mac_update_belief(mac, now, mac->peer_window_length, 1);
#endif
}


bool mac_idle_frame_needed(SkyMAC* mac, tick_t now) {

	// Is MAC idle frame transmissions set up?
	if (mac->config->idle_frames_per_window == 0)
		return false;

	// Is MAC logic active (not timed out)?
	if (wrap_time_ticks(now - mac->last_belief_update) > mac->config->idle_timeout_ticks)
		return false;

	// Not enough transmission on the window.
	return (mac->total_frames_sent_in_current_window < mac->config->idle_frames_per_window);
}


int mac_set_frame_fields(SkyMAC* mac, SkyRadioFrame* frame, tick_t now){
	uint16_t w = (uint16_t)mac->my_window_length;
	int32_t R = mac_own_window_remaining(mac, now);
	R = (R < 1) ? 1 : R;
	uint16_t r = (uint16_t)R;
	sky_packet_add_extension_mac_tdd_control(frame, w, r);
	return 0;
}
