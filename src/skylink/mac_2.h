//
// Created by elmore on 28.10.2021.
//

#ifndef SKYLINK_CMAKE_MAC_2_H
#define SKYLINK_CMAKE_MAC_2_H

#include "skylink.h"


int mac_valid_window_length(SkyMACConfig* config, int32_t length);

int mac_valid_gap_length(SkyMACConfig* config, int32_t length);

int32_t mac_set_my_window_length(MACSystem* macSystem, int32_t new_length);

int32_t mac_set_peer_window_length(MACSystem* macSystem, int32_t new_length);

int32_t mac_set_gap_constant(MACSystem* macSystem, int32_t new_gap_constant);



// The obvious...
MACSystem* new_mac_system(SkyMACConfig* config);
void destroy_mac_system(MACSystem* macSystem);


// Returns positive integer number of milliseconds of own transmit window remaining,
// or negative integer signaling the number of milliseconds past the window closure.
int32_t mac_own_window_remaining(MACSystem* macSystem, int32_t now_ms);


// Returns positive integer number of milliseconds of peer transmit window remaining,
// or negative integer signaling the number of milliseconds past the window closure.
int32_t mac_peer_window_remaining(MACSystem* macSystem, int32_t now_ms);


// Returns boolean (1/0) wether MAC thinks it is our time to speak at now_ms.
int mac_can_send(MACSystem* macSystem, int32_t now_ms);


// Updates the MAC-system's belief of the current status of the windowing:
// "peer mac window" is sent by the peer, and is simply the total length of the window it wishes to transmit in, in milliseconds.
// "peer mac remaining" is sent by the peer, and indicates how many milliseconds the peer thinks is remaining of it's window.
// With this information we can formulate a picture of the situatuion that will be reasonably accurate for several cycles.
int mac_update_belief(MACSystem* macSystem, SkyMACConfig* config, int32_t now_ms, int32_t peer_mac_length, int32_t peer_mac_remaining);


// If fleeting transmission is detected, but not fully received, this cues the MAC-system, and updates belief by remaining=1ms.
// BE CAREFUL: mac_update_belief is generally invoked only by authenticated messages to prevent shut-up-attack.
// This function in principle gets invoked before any authentication can take place. Therefore use sparingly.
int mac_carrier_sensed(MACSystem* macSystem, SkyMACConfig* config, int32_t now_ms);


// Writes out the two uint16 values to the provided spot in buffer.
int mac_stamp_packet_bytes(MACSystem* macSystem, uint8_t* tgt, int32_t now_ms);


// Writes out the two uint16 values to the provided spot in buffer.
int mac_set_frame_fields(MACSystem* macSystem, SkyRadioFrame* frame, int32_t now_ms);

#endif //SKYLINK_CMAKE_MAC_2_H
