#ifndef __SKYLINK_MAC_H__
#define __SKYLINK_MAC_H__

#include <stdbool.h>
#include <stdint.h>

#include "skylink/skylink.h"
#include "skylink/conf.h"


/*
 * TDD MAC state structure
 */
struct sky_mac_s {

	// Pointer to original configuration struct
	const SkyMACConfig* config;

	// Tick of the beginning of a cycle. Not necessarily the last cycle. Also beginning of one of our windows.
	sky_tick_t T0;

	// Length of our own transmission window.
	sky_tick_t my_window_length;

	// Length of the peer's transmission window. (Or our belief of what it is)
	sky_tick_t peer_window_length;

	// Tick of last time we updated our belief of T0 (and peer window length).
	sky_tick_t last_belief_update;

	// Counter to monitor is the current window lenght too larger or small
	int32_t window_adjust_counter;

	// The virtual channel that takes presence on next window.
	uint8_t vc_round_robin_start;

	// Is our window open?
	bool window_on;

	// Was there ability to send, without content to send?
	bool unused_window_time;

	// Number of frames transmitted in this
	uint16_t frames_sent_in_current_window_per_vc[SKY_NUM_VIRTUAL_CHANNELS];

	// Total number of frames sent in this windows
	uint16_t total_frames_sent_in_current_window;
};




/*
 * Create new TDD MAC instance.
 *
 * params:
 *    config: Pointer to configuration struct. The allocation must always be available.
 */
SkyMAC* sky_mac_create(SkyMACConfig* config);


/*
 * Destroy TDD MAC instance
 *
 * params:
 *    mac: Pointer to MAC instance.
 */
void sky_mac_destroy(SkyMAC* mac);


/*
 * Moves the cycle startpoint by 't_shift' ticks.
 * This is mainly useful if there is reason to suspect that gs and satellite are in lockstep and talk over
 * each other. Preferably use the current own window length with random sign to shift by.
 */
void mac_shift_windowing(SkyMAC* mac, sky_tick_t t_shift);


/*
 * Expand/extend own TDD window by `window_adjust_increment_ticks`.
 * New length is limited `maximum_window_length_ticks` config.
 *
 * params:
 *    mac: Pointer to MAC instance.
 */
void mac_expand_window(SkyMAC* mac, sky_tick_t now); // TODO: extend / shorten


/*
 * Shrink/shorten own TDD window by `window_adjust_increment_ticks`.
 * New length is limited `minimum_window_length_ticks` config.
 *
 * params:
 *    mac: Pointer to MAC instance.
 */
void mac_shrink_window(SkyMAC* mac, sky_tick_t now);


/*
 * Returns positive integer number of ticks to own transmit window opening,
 * or zero, if the window is open.
 */
int32_t mac_time_to_own_window(SkyMAC* mac, sky_tick_t now);


/*
 * Returns positive integer number of ticks of own transmit window remaining,
 * or negative integer signaling the number of ticks past the window closure.
 */
int32_t mac_own_window_remaining(SkyMAC* mac, sky_tick_t now);


/*
 * Returns positive integer number of ticks of peer transmit window remaining,
 * or negative integer signaling the number of ticks past the window closure.
 */
int32_t mac_peer_window_remaining(SkyMAC* mac, sky_tick_t now);


/*
 * Returns true whether MAC thinks it is our time to speak at now.
 */
bool mac_can_send(SkyMAC* mac, sky_tick_t now);


/*
 * Updates the MAC-system's belief of the current status of the windowing:
 * "peer_mac_window" is sent by the peer, and is simply the total length of the window it wishes to transmit in, in ticks.
 * "peer_mac_remaining" is sent by the peer, and indicates how many ticks the peer thinks is remaining of it's window.
 * With this information we can formulate a picture of the situatuion that will be reasonably accurate for several cycles.
 */
void mac_update_belief(SkyMAC* mac, const sky_tick_t now, sky_tick_t receive_time, sky_tick_t peer_mac_length, sky_tick_t peer_mac_remaining);


/*
 * Resets mac into a state where it can immediately send.
 */
void mac_reset(SkyMAC* mac, sky_tick_t now);


/*
 * If fleeting transmission is detected, but not fully received, this cues the MAC-system, and updates belief by remaining = 1 tick.
 * BE CAREFUL: mac_update_belief is generally invoked only by authenticated messages to prevent 'shut-up-attack'.
 * This function in principle gets invoked before any authentication can take place. Therefore use sparingly.
 */
void sky_mac_carrier_sensed(SkyMAC* mac, sky_tick_t now);


/*
 * Returns boolean 1/0 wether an idle frame should be sent to synch the peer side.
 */
bool mac_idle_frame_needed(SkyMAC* mac, sky_tick_t now);


/*
 * Writes out the two uint16 values to the provided spot in buffer.
 */
int mac_set_frame_fields(SkyMAC* mac, SkyRadioFrame* frame, sky_tick_t now);

#endif // __SKYLINK_MAC_H__
