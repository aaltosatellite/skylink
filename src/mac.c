#include "skylink/diag.h"
#include "skylink/mac.h"
#include "skylink/conf.h"
#include "skylink/frame.h"
#include "skylink/utilities.h"
#include "sky_platform.h"


/*
 * Calculate the total length of TDD cycle in ticks
 */
static sky_tick_t get_mac_cycle(SkyMAC* mac){
	return mac->my_window_length + mac->config->gap_constant_ticks + mac->peer_window_length + mac->config->tail_constant_ticks;
}

/*
 * Calculate
 */
static sky_tick_t wrap_tdd_cycle(SkyMAC* mac, sky_tick_t ticks) {
	// This mess is a conversion from C-modulo, to always-positive-modulo.
#if 1
	sky_tick_t mod = get_mac_cycle(mac);
	return positive_modulo(ticks, mod);
#else
	return ((ticks % mod) + mod) % mod;
#endif
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
	if (config->idle_timeout_ticks < 5000 || config->idle_timeout_ticks > 90000)
		config->idle_timeout_ticks = 25000;

	// A necessary limit: Otherwise carrier_sensed will loop back to own window from the other side.
	if (config->carrier_sense_ticks >= (config->minimum_window_length_ticks + config->gap_constant_ticks))
		config->carrier_sense_ticks = config->minimum_window_length_ticks + config->gap_constant_ticks;

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
	mac->unused_window_time = 0;
	mac->my_window_length = config->minimum_window_length_ticks;
	mac->peer_window_length = config->minimum_window_length_ticks;
	mac->last_belief_update = MOD_TIME_TICKS - 300000;
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


void mac_shift_windowing(SkyMAC* mac, sky_tick_t t_shift) {
	// T0 should always stay behind current time. Otherwise time-deltas wrap around the wrong way.
	t_shift = (t_shift > 0) ? t_shift : -t_shift;
	mac->T0 = wrap_time_ticks(mac->T0 - t_shift);
}



void mac_expand_window(SkyMAC* mac, sky_tick_t now) {
	mac->my_window_length += mac->config->window_adjust_increment_ticks;
	if (mac->my_window_length > mac->config->maximum_window_length_ticks)
		mac->my_window_length = mac->config->maximum_window_length_ticks;
	mac->T0 = wrap_time_ticks(now - get_mac_cycle(mac));
}


void mac_shrink_window(SkyMAC* mac, sky_tick_t now) {
	mac->my_window_length -= mac->config->window_adjust_increment_ticks;
	if (mac->my_window_length < mac->config->minimum_window_length_ticks)
		mac->my_window_length = mac->config->minimum_window_length_ticks;
	mac->T0 = wrap_time_ticks(now - get_mac_cycle(mac));
}


int32_t mac_time_to_own_window(SkyMAC* mac, sky_tick_t now) {
	int32_t dt = wrap_tdd_cycle(mac, wrap_time_ticks(now - mac->T0));
	if (dt < mac->my_window_length) // Our window is already open
		return 0;
	return get_mac_cycle(mac) - dt; // Peer's windows is open. Return time the end of window
}


int32_t mac_own_window_remaining(SkyMAC* mac, sky_tick_t now) {
	int32_t dt = wrap_tdd_cycle(mac, wrap_time_ticks(now - mac->T0));
	return mac->my_window_length - dt;
}


int32_t mac_peer_window_remaining(SkyMAC* mac, sky_tick_t now) {
	sky_tick_t peer_window_starts = mac->T0 + mac->my_window_length + mac->config->gap_constant_ticks;
	int32_t dt = wrap_tdd_cycle(mac, wrap_time_ticks(now - peer_window_starts));
	return mac->peer_window_length - dt;
}


bool mac_can_send(SkyMAC* mac, sky_tick_t now) {
	return mac_own_window_remaining(mac, now) > 0;
}


void mac_update_belief(SkyMAC* mac, const sky_tick_t now, sky_tick_t receive_time, sky_tick_t peer_mac_length, sky_tick_t peer_mac_remaining) {

	// Limit the window length inside limits.
	// Don't fail completely here and incapacitate the MAC logic, because this can be a result of minor missconfiguration.
	if (peer_mac_length < mac->config->minimum_window_length_ticks)
		peer_mac_length = mac->config->minimum_window_length_ticks;
	else if (peer_mac_length > mac->config->maximum_window_length_ticks)
		peer_mac_length = mac->config->maximum_window_length_ticks;

	if (peer_mac_remaining > peer_mac_length)
		peer_mac_remaining = peer_mac_length;


	sky_tick_t now_ = now;
	if (now < receive_time) //If now and receive_time *happen* to be on different sides of modulo, do this. Rare.
		now_ = receive_time + wrap_time_ticks(now - receive_time);

	mac->last_belief_update = now;
	mac->peer_window_length = peer_mac_length;

	const int32_t cycle = get_mac_cycle(mac);

	sky_tick_t implied_t0 = receive_time + peer_mac_remaining + (mac->config->tail_constant_ticks - cycle);
	sky_tick_t minimum_t0 = now_ + (mac->config->tail_constant_ticks - cycle);
	if (implied_t0 < minimum_t0)
		mac->T0 = wrap_time_ticks(minimum_t0);
	else
		mac->T0 = wrap_time_ticks(implied_t0);


	SKY_PRINTF(SKY_DIAG_MAC | SKY_DIAG_DEBUG, "MAC Update belief: peer window length: %d, remaining %d, time to t0: %d",
		peer_mac_length, peer_mac_remaining, mac_time_to_own_window(mac, now))

	// Reset frame counts
	mac->total_frames_sent_in_current_window = 0;
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		mac->frames_sent_in_current_window_per_vc[i] = 0;
	}

}


void mac_reset(SkyMAC* mac, sky_tick_t now) {
	mac->window_adjust_counter = 0;
	mac->my_window_length = mac->config->minimum_window_length_ticks;
	mac->T0 = wrap_time_ticks(now - get_mac_cycle(mac));
}


void sky_mac_carrier_sensed(SkyMAC* mac, sky_tick_t now) {
	int32_t ticks_to_own_window_priori = mac_time_to_own_window(mac, now);
	if(ticks_to_own_window_priori <= mac->config->carrier_sense_ticks) {
		int32_t cycle = get_mac_cycle(mac);
		mac->T0 = wrap_time_ticks((now - cycle) + mac->config->carrier_sense_ticks);
	}
}


bool mac_idle_frame_needed(SkyMAC* mac, sky_tick_t now) {

	// Is MAC idle frame transmissions set up?
	if (mac->config->idle_frames_per_window == 0)
		return false;

	// Is MAC logic active (not timed out)?
	if (wrap_time_ticks(now - mac->last_belief_update) > mac->config->idle_timeout_ticks)
		return false;

	// Not enough transmission on the window.
	return (mac->total_frames_sent_in_current_window < mac->config->idle_frames_per_window);
}


int mac_set_frame_fields(SkyMAC* mac, SkyRadioFrame* frame, sky_tick_t now){
	uint16_t w = (uint16_t)mac->my_window_length;
	int32_t R = mac_own_window_remaining(mac, now);
	R = (R < 1) ? 1 : R;
	uint16_t r = (uint16_t)R;
	sky_frame_add_extension_mac_tdd_control(frame, w, r);
	return 0;
}
