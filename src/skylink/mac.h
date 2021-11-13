#ifndef __SKYLINK_MAC_H__
#define __SKYLINK_MAC_H__

#include <stdint.h>
#include "skylink/conf.h"
#include "skylink/skylink.h"


#define MODE_RX		11
#define MODE_TX		22


/* MAC-system */
struct sky_mac_s {
	int32_t T0_ms;
	int32_t my_window_length;
	int32_t peer_window_length;
	int32_t gap_constant;
	int32_t tail_constant;
	int32_t last_belief_update;

	uint8_t radio_mode;
	uint16_t frames_sent_in_current_window_per_vc[SKY_NUM_VIRTUAL_CHANNELS];
	uint16_t total_frames_sent_in_current_window;
};


int mac_valid_window_length(SkyMACConfig* config, int32_t length);

int mac_valid_gap_length(SkyMACConfig* config, int32_t length);

int32_t mac_set_my_window_length(SkyMAC* mac, int32_t new_length);

int32_t mac_set_peer_window_length(SkyMAC* mac, int32_t new_length);

int32_t mac_set_gap_constant(SkyMAC* mac, int32_t new_gap_constant);



// The obvious...
SkyMAC* sky_mac_create(SkyMACConfig* config);
void sky_mac_destroy(SkyMAC* mac);


// Recalibrates the cycle startpoint by 't_shift' milliseconds.
// This is mainly useful if there is reason to suspect that gs and satellite are in lockstep and talk over
// each other. Preferably use the current own window length with random sign to shift by.
void mac_shift_windowing(SkyMAC* mac, int32_t t_shift);


// Returns positive integer number of milliseconds to own transmit window opening,
// or zero, if the window is open.
int32_t mac_time_to_own_window(SkyMAC* mac, int32_t now_ms);


// Returns positive integer number of milliseconds of own transmit window remaining,
// or negative integer signaling the number of milliseconds past the window closure.
int32_t mac_own_window_remaining(SkyMAC* mac, int32_t now_ms);


// Returns positive integer number of milliseconds of peer transmit window remaining,
// or negative integer signaling the number of milliseconds past the window closure.
int32_t mac_peer_window_remaining(SkyMAC* mac, int32_t now_ms);


// Checks if time elapsed since last mac belief update exceeds a configured value.
// If so, shifts the windowing by random step. This is used to break out of equal sync situation.
void mac_silence_shift_check(SkyMAC* mac, SkyMACConfig* config, int32_t now_ms);


// Returns boolean (1/0) wether MAC thinks it is our time to speak at now_ms.
int mac_can_send(SkyMAC* mac, int32_t now_ms);


// Updates the MAC-system's belief of the current status of the windowing:
// "peer mac window" is sent by the peer, and is simply the total length of the window it wishes to transmit in, in milliseconds.
// "peer mac remaining" is sent by the peer, and indicates how many milliseconds the peer thinks is remaining of it's window.
// With this information we can formulate a picture of the situatuion that will be reasonably accurate for several cycles.
int mac_update_belief(SkyMAC* mac, SkyMACConfig* config, int32_t now_ms, int32_t peer_mac_length, int32_t peer_mac_remaining);


// If fleeting transmission is detected, but not fully received, this cues the MAC-system, and updates belief by remaining=1ms.
// BE CAREFUL: mac_update_belief is generally invoked only by authenticated messages to prevent 'shut-up-attack'.
// This function in principle gets invoked before any authentication can take place. Therefore use sparingly.
int sky_mac_carrier_sensed(SkyMAC* mac, SkyMACConfig* config, int32_t now_ms);


// Writes out the two uint16 values to the provided spot in buffer.
int mac_set_frame_fields(SkyMAC* mac, SkyRadioFrame* frame, int32_t now_ms);

#endif // __SKYLINK_MAC_H__
