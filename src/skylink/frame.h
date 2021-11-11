#ifndef __SKYLINK_FRAME_H__
#define __SKYLINK_FRAME_H__

#include <string.h>
#include "skylink.h"



/* extensions ====================================================================================== */

#define SKYLINK_START_BYTE              's' 	//all packets start with this
#define EXTENSION_ARQ_SEQUENCE          1
#define EXTENSION_ARQ_REQUEST           2
#define EXTENSION_ARQ_RESET             3
#define EXTENSION_MAC_PARAMETERS        4
#define EXTENSION_MAC_TDD_CONTROL       5
#define EXTENSION_HMAC_SEQUENCE_RESET   6


/* ARQ Sequence */
typedef struct __attribute__((__packed__)) {
	uint8_t sequence;
} ExtARQSeq;

/* ARQ Retransmit Request */
typedef struct __attribute__((__packed__)) {
	uint8_t sequence;
	uint16_t mask;
} ExtARQReq;

/* ARQ Reset */
typedef struct __attribute__((__packed__)) {
	uint8_t toggle;
	uint8_t enforced_sequence;
} ExtARQReset;

/* TDD MAC Control  */
typedef struct __attribute__((__packed__)) {
	uint16_t window_size;
	uint16_t gap_size;
} ExtTDDParams;

typedef struct __attribute__((__packed__)) {
	uint16_t window;
	uint16_t remaining;
} ExtTDDControl;

/* HMAC Sequence Correction */
typedef struct __attribute__((__packed__)) {
	/* New sequence number to be started from */
	uint16_t sequence;
} ExtHMACSequenceReset;

/* General Extension Header struct */
typedef struct __attribute__((__packed__)) {
	unsigned int type    : 4;
	unsigned int length  : 4;
	union {
		ExtARQSeq ARQSeq;
		ExtARQReq ARQReq;
		ExtARQReset ARQReset;
		ExtTDDParams TDDParams;
		ExtTDDControl TDDControl;
		ExtHMACSequenceReset HMACSequenceReset;
	};
} SkyPacketExtension; // SkyHeaderExtension

/* extensions ====================================================================================== */



//extensions start at this byte index. At the same time the minimum length of a healthy frame.
#define EXTENSION_START_IDX				15
#define SKY_PLAIN_FRAME_MIN_LENGTH		EXTENSION_START_IDX
#define SKY_ENCODED_FRAME_MIN_LENGTH	(EXTENSION_START_IDX + RS_PARITYS)




SkyRadioFrame* new_send_frame();
SkyRadioFrame* new_receive_frame();
void destroy_receive_frame(SkyRadioFrame* frame);
void destroy_send_frame(SkyRadioFrame* frame);



// encoding ============================================================================================================
int sky_packet_add_extension_arq_sequence(SkyRadioFrame* frame, uint8_t sequence);

int sky_packet_add_extension_arq_request(SkyRadioFrame* frame, uint8_t sequence, uint16_t mask);

int sky_packet_add_extension_arq_reset(SkyRadioFrame* frame, uint8_t toggle, uint8_t sequence);

int sky_packet_add_extension_mac_params(SkyRadioFrame* frame, uint16_t gap_size, uint16_t window_size);

int sky_packet_add_extension_hmac_sequence_reset(SkyRadioFrame* frame, uint16_t sequence);

int available_payload_space(SkyRadioFrame* radioFrame);

int sky_packet_extend_with_payload(SkyRadioFrame* frame, void* pl, int32_t length);
// encoding ============================================================================================================



// decoding ============================================================================================================
//int interpret_extension(void* ptr, int max_length, SkyPacketExtension* extension);
// decoding ============================================================================================================



#endif // __SKYLINK_FRAME_H__
