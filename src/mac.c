
// MAC (Medium Access Control) and TDD (Time Division Duplexing) implementation

#include "skylink/diag.h"
#include "skylink/mac.h"
#include "skylink/conf.h"
#include "skylink/frame.h"
#include "skylink/utilities.h"
#include "sky_platform.h"


/*
 * Calculate the total length of TDD cycle in ticks
 */
static sky_tick_t get_mac_cycle(SkyMAC* mac)
{
	return mac->my_window_length + mac->config->gap_constant_ticks + mac->peer_window_length + mac->config->tail_constant_ticks;
}


// Positive modulo function to allow tdd cycle to wrap around when going over the maximum value.
static sky_tick_t wrap_tdd_cycle(SkyMAC* mac, sky_tick_t ticks)
{
#if 1
	// Get cycle length and the positive module between it and ticks.
	sky_tick_t mod = get_mac_cycle(mac);
	return positive_modulo(ticks, mod);
#else
	return ((ticks % mod) + mod) % mod;
#endif
}







// === PUBLIC FUNCTIONS ================================================================================================

// Create a new MAC struct and initialize it with the given configuration.
SkyMAC* sky_mac_create(SkyMACConfig* config)
{
	//Check that values are within allowed parameters, if not set them to default.

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

	//Allocate memory for the MAC struct.
	SkyMAC* mac = SKY_MALLOC(sizeof(SkyMAC));

	//Initialize the MAC struct.
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


// Free a MAC struct.
void sky_mac_destroy(SkyMAC* mac) {
	SKY_FREE(mac);
}

/*
 *	Moves the cycle startpoint by 't_shift' ticks.
 *	This is mainly useful if there is reason to suspect that gs and satellite are in lockstep and talk over
 *	each other. Preferably use the current own window length with random sign to shift by.
 */
void mac_shift_windowing(SkyMAC* mac, sky_tick_t t_shift) {
	// T0 should always stay behind current time. Otherwise time-deltas wrap around the wrong way.
	t_shift = (t_shift > 0) ? t_shift : -t_shift;
	mac->T0 = wrap_time_ticks(mac->T0 - t_shift);
}


// Expand/extend own TDD window by window_adjust_increment_ticks.
void mac_expand_window(SkyMAC* mac, sky_tick_t now)
{
	// Extend window.
	mac->my_window_length += mac->config->window_adjust_increment_ticks;

	// Check if window is too long. If so, shrink it to maximum.
	if (mac->my_window_length > mac->config->maximum_window_length_ticks)
		mac->my_window_length = mac->config->maximum_window_length_ticks;

	// Set new value for beginning of a cycle.
	mac->T0 = wrap_time_ticks(now - get_mac_cycle(mac));
}


// Shrink own TDD window by window_adjust_increment_ticks.
void mac_shrink_window(SkyMAC* mac, sky_tick_t now)
{
	// Shrink window.
	mac->my_window_length -= mac->config->window_adjust_increment_ticks;

	// Check if window is too short. If so, expand it to minimum.
	if (mac->my_window_length < mac->config->minimum_window_length_ticks)
		mac->my_window_length = mac->config->minimum_window_length_ticks;

	// Set new value for beginning of a cycle.
	mac->T0 = wrap_time_ticks(now - get_mac_cycle(mac));
}

// Return positive number of ticks to own transmit window opening. 0 if it is already open.
int32_t mac_time_to_own_window(SkyMAC* mac, sky_tick_t now)
{
	// Get delta time between now and the beginning of our window.
	int32_t dt = wrap_tdd_cycle(mac, wrap_time_ticks(now - mac->T0));

	// Our window is already open
	if (dt < mac->my_window_length)
		return 0;

	// Peer's windows is open. Return number of ticks to own transmit window opening.
	return get_mac_cycle(mac) - dt; 
}


/*
 *	Returns positive integer number of ticks of own transmit window remaining,
 *	or negative integer signaling the number of ticks past the window closure.
 */
int32_t mac_own_window_remaining(SkyMAC* mac, sky_tick_t now)
{
	// Get delta time between now and the beginning of our window.
	int32_t dt = wrap_tdd_cycle(mac, wrap_time_ticks(now - mac->T0));

	// Return number of ticks of own transmit window remaining, If dt > my_window_length, return negative number. (window is closed)
	return mac->my_window_length - dt;
}


int32_t mac_peer_window_remaining(SkyMAC* mac, sky_tick_t now)
{
	sky_tick_t peer_window_starts = mac->T0 + mac->my_window_length + mac->config->gap_constant_ticks;
	int32_t dt = wrap_tdd_cycle(mac, wrap_time_ticks(now - peer_window_starts));
	return mac->peer_window_length - dt;
}

// Returns boolean whether MAC thinks it is our time to speak now.
bool mac_can_send(SkyMAC* mac, sky_tick_t now)
{
	// If there is still time left in our window, we can send.
	return mac_own_window_remaining(mac, now) > 0;
}

/*
 *	Updates the MAC-system's belief of the current status of the windowing:
 *	"peer_mac_length" is sent by the peer, and is simply the total length of the window it wishes to transmit in, in ticks.
 *	"peer_mac_remaining" is sent by the peer, and indicates how many ticks the peer thinks is remaining of it's window.
 *	With this information we can formulate a picture of the situatuion that will be reasonably accurate for several cycles.
 */
void mac_update_belief(SkyMAC* mac, const sky_tick_t now, sky_tick_t receive_time, sky_tick_t peer_mac_length, sky_tick_t peer_mac_remaining)
{

	// Limit the window length inside limits.
	// Don't fail completely here and incapacitate the MAC logic, because this can be a result of minor missconfiguration.
	if (peer_mac_length < mac->config->minimum_window_length_ticks)
		peer_mac_length = mac->config->minimum_window_length_ticks;
	else if (peer_mac_length > mac->config->maximum_window_length_ticks)
		peer_mac_length = mac->config->maximum_window_length_ticks;

	if (peer_mac_remaining > peer_mac_length)
		peer_mac_remaining = peer_mac_length;


	sky_tick_t now_ = now;

	//If now and receive_time *happen* to be on different sides of modulo, do this. Rare.
	if (now < receive_time)
		now_ = receive_time + wrap_time_ticks(now - receive_time);

	// Update MAC state
	mac->last_belief_update = now;
	mac->peer_window_length = peer_mac_length;

	const int32_t cycle = get_mac_cycle(mac);

	// Calculate implied value for T0 based on recieve time and peer_mac remaining and a minimum value for T0.
	sky_tick_t implied_t0 = receive_time + peer_mac_remaining + (mac->config->tail_constant_ticks - cycle);
	sky_tick_t minimum_t0 = now_ + (mac->config->tail_constant_ticks - cycle);

	// Choose the one that is furthest in the future.
	if (implied_t0 < minimum_t0)
		mac->T0 = wrap_time_ticks(minimum_t0);
	else
		mac->T0 = wrap_time_ticks(implied_t0);

	// Debug print
	SKY_PRINTF(SKY_DIAG_MAC | SKY_DIAG_DEBUG, "MAC Update belief: peer window length: %d, remaining %d, time to t0: %d",
		peer_mac_length, peer_mac_remaining, mac_time_to_own_window(mac, now))

	// Reset frame counts
	mac->total_frames_sent_in_current_window = 0;
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		mac->frames_sent_in_current_window_per_vc[i] = 0;
	}

}

// Reset mac into a state where it can immediately send.
void mac_reset(SkyMAC* mac, sky_tick_t now)
{
	// Reset window length to minimum and zero it's counter.
	mac->window_adjust_counter = 0;
	mac->my_window_length = mac->config->minimum_window_length_ticks;

	// Set value of start of cycle/own window.
	mac->T0 = wrap_time_ticks(now - get_mac_cycle(mac));
}

/*
 * If fleeting transmission is detected, but not fully received, this cues the MAC-system, and updates belief by remaining = 1 tick.
 * BE CAREFUL: mac_update_belief is generally invoked only by authenticated messages to prevent 'shut-up-attack'.
 * This function in principle gets invoked before any authentication can take place. Therefore use sparingly.
 */
void sky_mac_carrier_sensed(SkyMAC* mac, sky_tick_t now)
{
	// Get ticks to own window opening.
	int32_t ticks_to_own_window_priori = mac_time_to_own_window(mac, now);

	// Is the time to window opening less then the fallback of carrier sense ticks?
	if(ticks_to_own_window_priori <= mac->config->carrier_sense_ticks) {
		// Update cycle and its startpoint.
		int32_t cycle = get_mac_cycle(mac);
		mac->T0 = wrap_time_ticks((now - cycle) + mac->config->carrier_sense_ticks);
	}
}

// Returns boolean 1/0 whether an idle frame should be sent to sync the peer side.
bool mac_idle_frame_needed(SkyMAC* mac, sky_tick_t now)
{

	// Is MAC idle frame transmissions set up?
	if (mac->config->idle_frames_per_window == 0)
		return false;

	// Is MAC logic active (not timed out)?
	if (wrap_time_ticks(now - mac->last_belief_update) > mac->config->idle_timeout_ticks)
		return false;

	// Not enough transmission on the window.
	return (mac->total_frames_sent_in_current_window < mac->config->idle_frames_per_window);
}


int mac_set_frame_fields(SkyMAC* mac, SkyTransmitFrame* tx_frame, sky_tick_t now)
{
	// Get how much is remaining of own window.
	int32_t remaining = mac_own_window_remaining(mac, now);

	// Give at least 1 tick of window remaining.
	remaining = (remaining < 1) ? 1 : remaining;

	// Add MAC/TDD control extension with window length and remaining ticks to tx_frame.
	return sky_frame_add_extension_mac_tdd_control(tx_frame, (uint16_t)mac->my_window_length, (uint16_t)remaining);
}
