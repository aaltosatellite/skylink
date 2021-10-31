//
// Created by elmore on 27.10.2021.
//

#ifndef SKYLINK_CMAKE_SKYPACKET_H
#define SKYLINK_CMAKE_SKYPACKET_H

#include <string.h>
#include "skylink.h"
#include "hmac.h"
//#include "utilities.h"


#define SKY_PLAIN_FRAME_MIN_LENGTH		15
#define SKYLINK_START_BYTE				'S' 	//all packets start with this
#define SKYLINK_VERSION_BYTE			'1'		//the version id of the protocol.
#define EXTENSION_ARQ_RESEND_REQ		1
#define EXTENSION_ARQ_SEQ_RESET			2
#define EXTENSION_MAC_PARAMETERS		3
#define EXTENSION_HMAC_INVALID_SEQ		4


//bytearray indexes of specific segments of a packed message.
#define I_PK_IDENTITY					(2)
#define I_PK_FLAG						(2+5)
#define I_PK_VC_N_EXT					(2+5+1)
#define I_PK_HMAC						(2+5+1+1)
#define I_PK_MAC_LENGTH					(2+5+1+1+2)
#define I_PK_MAC_LEFT					(2+5+1+1+2+2)
#define I_PK_ARQ_SEQUENCE				(2+5+1+1+2+2+2)
#define I_PK_EXTENSIONS					(2+5+1+1+2+2+2+1)



SkyRadioFrame* new_frame();

void destroy_frame(SkyRadioFrame* frame);




// encoding ============================================================================================================
int sky_packet_add_extension_mac_params(SkyRadioFrame* frame, int default_window_size, int gap_size);

int sky_packet_add_extension_arq_setup(SkyRadioFrame* frame, int new_sequence, uint8_t toggle);

int sky_packet_add_extension_arq_resend_request(SkyRadioFrame* frame, int sequence, uint8_t mask1, uint8_t mask2);

int sky_packet_assign_hmac_sequence(SkyHandle self, SkyRadioFrame* frame);

int encode_skylink_packet(SkyRadioFrame* frame);

int sky_packet_available_payload_space(SkyRadioFrame* frame);

int sky_packet_extend_with_payload(SkyRadioFrame* frame, void* payload, int length);

int sky_packet_stamp_arq(SkyRadioFrame* frame, uint8_t arq_sequence);
// =====================================================================================================================



// decoding ============================================================================================================
int decode_skylink_packet(SkyRadioFrame* frame);
// =====================================================================================================================



#endif //SKYLINK_CMAKE_SKYPACKET_H