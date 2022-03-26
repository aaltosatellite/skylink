#ifndef __SKYLINK_MAC_H__
#define __SKYLINK_MAC_H__

#include <stdint.h>
#include "skylink/conf.h"
#include "skylink/skylink.h"



/* TDD MAC state structure */
struct sky_mac_s {

	// Pointer to original configuration struct
	const SkyMACConfig* config;

	// Tick of the beginning of a cycle. Not necessarily the last cycle. Also beginning of one of our windows.
	tick_t T0;

	// Length of our own transmission window.
	tick_t my_window_length;

	// Length of the peer's transmission window. (Or our belief of what it is)
	tick_t peer_window_length;

	// Tick of last time we updated our belief of T0 (and peer window length).
	tick_t last_belief_update;

	// The virtual channel that takes presence on next window.
	uint8_t vc_round_robin_start;

	// Counter
	int32_t window_adjust_plan;

	// Is our window open?
	uint8_t window_on; // TODO: bool

	// Number of frames transmitted in this
	uint16_t frames_sent_in_current_window_per_vc[SKY_NUM_VIRTUAL_CHANNELS];

	// Total number of frames sent in this windows
	uint16_t total_frames_sent_in_current_window;
};


/*
 */
int mac_valid_window_length(SkyMAC* mac, tick_t length);


// The obvious...
SkyMAC* sky_mac_create(SkyMACConfig* config);
void sky_mac_destroy(SkyMAC* mac);


// Recalibrates the cycle startpoint by 't_shift' ticks.
// This is mainly useful if there is reason to suspect that gs and satellite are in lockstep and talk over
// each other. Preferably use the current own window length with random sign to shift by.
void mac_shift_windowing(SkyMAC* mac, tick_t t_shift);


// Expand own window, capped by config maximum
int32_t mac_expand_window(SkyMAC* mac);


// Shrink own window, capped by config minimum
int32_t mac_shrink_window(SkyMAC* mac);


// Returns positive integer number of ticks to own transmit window opening,
// or zero, if the window is open.
int32_t mac_time_to_own_window(SkyMAC* mac, tick_t now);


// Returns positive integer number of ticks of own transmit window remaining,
// or negative integer signaling the number of ticks past the window closure.
int32_t mac_own_window_remaining(SkyMAC* mac, tick_t now);


// Returns positive integer number of ticks of peer transmit window remaining,
// or negative integer signaling the number of ticks past the window closure.
int32_t mac_peer_window_remaining(SkyMAC* mac, tick_t now);


// Checks if time elapsed since last mac belief update exceeds a configured value.
// If so, shifts the windowing by random step. This is used to break out of equal sync situation.
void mac_silence_shift_check(SkyMAC* mac, tick_t now);


// Returns boolean (1/0) wether MAC thinks it is our time to speak at now.
int mac_can_send(SkyMAC* mac, tick_t now);


// Updates the MAC-system's belief of the current status of the windowing:
// "peer mac window" is sent by the peer, and is simply the total length of the window it wishes to transmit in, in ticks.
// "peer mac remaining" is sent by the peer, and indicates how many ticks the peer thinks is remaining of it's window.
// With this information we can formulate a picture of the situatuion that will be reasonably accurate for several cycles.
int mac_update_belief(SkyMAC* mac, tick_t now, tick_t peer_mac_length, tick_t peer_mac_remaining);


// Resets mac into a state where it can immediately send.
void mac_reset(SkyMAC* mac, tick_t now);


// If fleeting transmission is detected, but not fully received, this cues the MAC-system, and updates belief by remaining = 1 tick.
// BE CAREFUL: mac_update_belief is generally invoked only by authenticated messages to prevent 'shut-up-attack'.
// This function in principle gets invoked before any authentication can take place. Therefore use sparingly.
int sky_mac_carrier_sensed(SkyMAC* mac, tick_t now);


// Returns boolean 1/0 wether an idle frame should be sent to synch the peer side.
int mac_idle_frame_needed(SkyMAC* mac, tick_t now);


// Writes out the two uint16 values to the provided spot in buffer.
int mac_set_frame_fields(SkyMAC* mac, SkyRadioFrame* frame, tick_t now);

#endif // __SKYLINK_MAC_H__
