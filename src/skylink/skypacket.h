//
// Created by elmore on 27.10.2021.
//

#ifndef SKYLINK_CMAKE_SKYPACKET_H
#define SKYLINK_CMAKE_SKYPACKET_H

#include <string.h>
#include "skylink.h"
#include "hmac.h"
#include "fec.h"
//#include "utilities.h"



#define SKYLINK_START_BYTE				'S' 	//all packets start with this
#define EXTENSION_ARQ_RESEND_REQ		1
#define EXTENSION_ARQ_SEQ_RESET			2
#define EXTENSION_MAC_PARAMETERS		3
#define EXTENSION_HMAC_ENFORCEMENT		4


//extensions start at this byte index. At the same time the minimum length of a healthy frame.
#define EXTENSION_START_IDX				15
#define SKY_PLAIN_FRAME_MIN_LENGTH		EXTENSION_START_IDX


//SkyRadioFrame* new_frame();
//void destroy_frame(SkyRadioFrame* frame);


SendFrame* new_send_frame();
RCVFrame* new_receive_frame();
void destroy_receive_frame(RCVFrame* frame);
void destroy_send_frame(SendFrame* frame);



// encoding ============================================================================================================
int sky_packet_add_extension_arq_rr(SendFrame* frame, uint8_t sequence, uint8_t mask1, uint8_t mask2);

int sky_packet_add_extension_arq_enforce(SendFrame* frame, uint8_t toggle, uint8_t sequence);

int sky_packet_add_extension_hmac_enforce(SendFrame* frame, uint16_t sequence);

int sky_packet_add_extension_mac_params(SendFrame* frame, uint16_t gap_size, uint16_t window_size);

int available_payload_space(RadioFrame2* radioFrame);

int sky_packet_extend_with_payload(SendFrame* frame, void* pl, int32_t length);
// encoding ============================================================================================================



// decoding ============================================================================================================
int interpret_extension(void* ptr, int max_length, SkyPacketExtension* extension);
// decoding ============================================================================================================



#endif //SKYLINK_CMAKE_SKYPACKET_H
