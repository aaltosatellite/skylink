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


#define SKY_PLAIN_FRAME_MIN_LENGTH		15
#define SKYLINK_START_BYTE				'S' 	//all packets start with this
#define EXTENSION_ARQ_RESEND_REQ		1
#define EXTENSION_ARQ_SEQ_RESET			2
#define EXTENSION_MAC_PARAMETERS		3
#define EXTENSION_HMAC_ENFORCEMENT		4


//bytearray indexes of specific segments of a packed message.
#define I_PK_IDENTITY					(2)
#define I_PK_FLAG						(2+5)
#define I_PK_VC_N_EXT					(2+5+1)
#define I_PK_HMAC						(2+5+1+1)
#define I_PK_MAC_LENGTH					(2+5+1+1+2)
#define I_PK_MAC_LEFT					(2+5+1+1+2+2)
#define I_PK_ARQ_SEQUENCE				(2+5+1+1+2+2+2)
#define I_PK_EXTENSIONS					(1+5+1+1+2+2+2+1)



//SkyRadioFrame* new_frame();
//void destroy_frame(SkyRadioFrame* frame);


SendFrame* new_send_frame();
RCVFrame* new_receive_frame();
void destroy_receive_frame(RCVFrame* frame);
void destroy_send_frame(SendFrame* frame);

/*

// encoding ============================================================================================================
int sky_packet_add_extension_mac_params(SkyRadioFrame* frame, int default_window_size, int gap_size);

int sky_packet_add_extension_arq_setup(SkyRadioFrame* frame, int new_sequence, uint8_t toggle);

int sky_packet_add_extension_arq_resend_request(SkyRadioFrame* frame, int sequence, uint8_t mask1, uint8_t mask2);

int sky_packet_add_extension_hmac_enforcement(SkyRadioFrame* frame, uint16_t hmac_sequence);

int encode_skylink_packet_extensions(SkyRadioFrame* frame);

int encode_skylink_packet_header(SkyRadioFrame* frame);

int sky_packet_available_payload_space(SkyRadioFrame* frame);

int sky_packet_extend_with_payload(SkyRadioFrame* frame, void* payload, int length);

int sky_packet_stamp_arq(SkyRadioFrame* frame, uint8_t arq_sequence);
// =====================================================================================================================



// decoding ============================================================================================================
void initialize_for_decoding(SkyRadioFrame* frame);



int decode_skylink_packet(SkyRadioFrame* frame);
// =====================================================================================================================
*/


int sky_packet_add_extension_arq_rr(SendFrame* frame, uint8_t sequence, uint8_t mask1, uint8_t mask2);

int sky_packet_add_extension_arq_enforce(SendFrame* frame, uint8_t toggle, uint8_t sequence);

int sky_packet_add_extension_hmac_enforce(SendFrame* frame, uint16_t sequence);

int sky_packet_add_extension_mac_params(SendFrame* frame, uint16_t gap_size, uint16_t window_size);

int available_payload_space(RadioFrame2* radioFrame);

int sky_packet_extend_with_payload(SendFrame* frame, void* pl, int32_t length);

int interpret_extension(void* ptr, int max_length, SkyPacketExtension* extension);

#endif //SKYLINK_CMAKE_SKYPACKET_H
