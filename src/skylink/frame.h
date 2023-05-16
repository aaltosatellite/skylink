#ifndef __SKYLINK_FRAME_H__
#define __SKYLINK_FRAME_H__

#include "skylink/skylink.h"
#include "sky_platform.h"

/*
 * Number of bytes in frame identity field
 */
#define SKY_IDENTITY_LEN                6

/*
 * All packets start with this.
 * ("encoded" protocol + version identifier)
 */
#define SKYLINK_START_BYTE              ((0b01100 << 3) | (0x7 & SKY_IDENTITY_LEN))

/*
 * Frame header flags
 */
#define SKY_FLAG_AUTHENTICATED          0b00001
#define SKY_FLAG_ARQ_ON                 0b00010
#define SKY_FLAG_HAS_PAYLOAD            0b00100


// Extensions start at this byte index. At the same time the minimum length of a healthy frame.
#define EXTENSION_START_IDX             (SKY_IDENTITY_LEN + 5)

// Extension header type IDs
#define EXTENSION_ARQ_SEQUENCE          0
#define EXTENSION_ARQ_REQUEST           1
#define EXTENSION_ARQ_CTRL              2
#define EXTENSION_ARQ_HANDSHAKE         3
#define EXTENSION_MAC_TDD_CONTROL       4
#define EXTENSION_HMAC_SEQUENCE_RESET   5


// The maximum payload size that fits a worst case frame with all extensions.
#define SKY_MAX_PAYLOAD_LEN             177
#define SKY_PLAIN_FRAME_MIN_LENGTH      (EXTENSION_START_IDX)
#define SKY_ENCODED_FRAME_MIN_LENGTH    (EXTENSION_START_IDX + RS_PARITYS)
#define SKY_FRAME_MAX_LEN               (0x100)



/* frames ========================================================================================== */
struct sky_radio_frame {

	// Ticks when the start of the frame was detected
	sky_tick_t rx_time_ticks;

	// Length of the raw frame
	unsigned int length;

	union {

		uint8_t raw[SKY_FRAME_MAX_LEN + 6];

		struct __attribute__((__packed__)) {
			uint8_t start_byte;
			uint8_t identity[SKY_IDENTITY_LEN];
			uint8_t vc : 3;
			uint8_t flags : 5;
			uint8_t ext_length;
			uint16_t auth_sequence;
		};
	};
};
/* frames ========================================================================================== */


/* ARQ Sequence */
typedef struct __attribute__((__packed__)) {
	sky_arq_sequence_t sequence;
} ExtARQSeq;

/* ARQ Retransmit Request */
typedef struct __attribute__((__packed__)) {
	sky_arq_sequence_t sequence;
	uint16_t mask;
} ExtARQReq;

/* ARQ control sequence */
typedef struct __attribute__((__packed__)) {
	sky_arq_sequence_t tx_sequence;
	sky_arq_sequence_t rx_sequence;
} ExtARQCtrl;

/* ARQ state initializer */
typedef struct __attribute__((__packed__)) {
	uint8_t  peer_state;
	uint32_t identifier;
} ExtARQHandshake;

/* TDD MAC Control  */
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
	uint8_t type    : 4;
	uint8_t length  : 4;
	union {
		ExtARQSeq ARQSeq;
		ExtARQReq ARQReq;
		ExtARQCtrl ARQCtrl;
		ExtARQHandshake ARQHandshake;
		ExtTDDControl TDDControl;
		ExtHMACSequenceReset HMACSequenceReset;
	};
} SkyHeaderExtension;

/* Struct to hold pointers to parsed header extensions. */
typedef struct {
	SkyHeaderExtension* arq_sequence;
	SkyHeaderExtension* arq_request;
	SkyHeaderExtension* arq_ctrl;
	SkyHeaderExtension* arq_handshake;
	SkyHeaderExtension* mac_tdd;
	SkyHeaderExtension* hmac_reset;
} SkyParsedExtensions;

/* extensions ====================================================================================== */




/*
 * Allocate a new radio frame object
 */
SkyRadioFrame* sky_frame_create();

/*
 * Destroy frame
 */
void sky_frame_destroy(SkyRadioFrame* frame);

/*
 * Clear the radio frame
 */
void sky_frame_clear(SkyRadioFrame* frame);

/*
 * Add ARQ sequence number to the frame.
 */
int sky_frame_add_extension_arq_sequence(SkyRadioFrame* frame, sky_arq_sequence_t sequence);

/*
 * Add ARQ Retransmit Request header to the frame.
 */
int sky_frame_add_extension_arq_request(SkyRadioFrame* frame, sky_arq_sequence_t sequence, uint16_t mask);

/*
 * Add ARQ Control header to the frame.
 */
int sky_frame_add_extension_arq_ctrl(SkyRadioFrame* frame, sky_arq_sequence_t tx_head_sequence, sky_arq_sequence_t rx_head_sequence);

/*
 * Add ARQ Handshake header to the frame.
 */
int sky_frame_add_extension_arq_handshake(SkyRadioFrame* frame, uint8_t state_flag, uint32_t identifier);

/*
 * Add MAC TDD control header to the frame.
 */
int sky_frame_add_extension_mac_tdd_control(SkyRadioFrame* frame, uint16_t window, uint16_t remaining);

/*
 * Add HMAC sequence reset header to the frame.
 */
int sky_frame_add_extension_hmac_sequence_reset(SkyRadioFrame* frame, uint16_t sequence);

/* 
 * Fill the rest of the frame with given payload data.
 */
int sky_frame_extend_with_payload(SkyRadioFrame *frame, void *pl, unsigned int length);

/*
 * Get number of bytes left in the frame.
 */
int sky_frame_get_space_left(const SkyRadioFrame* radioFrame);

/*
 * Parse and validate all header extensions inside the frame.
 */
int sky_frame_parse_extension_headers(const SkyRadioFrame *frame, SkyParsedExtensions *exts);


#endif // __SKYLINK_FRAME_H__
