#ifndef __SKYLINK_FRAME_H__
#define __SKYLINK_FRAME_H__

#include "skylink/skylink.h"
#include "sky_platform.h"

/* Maximum number of bytes in frame identity field. */
#define SKY_MAX_IDENTITY_LEN            (7)

/*
 * All packets start with this.
 * ("encoded" protocol + version identifier)
 */
#define SKYLINK_FRAME_VERSION_BYTE      (0b01100 << 3)
#define SKYLINK_FRAME_VERSION_MASK      (0xF8)
#define SKYLINK_FRAME_IDENTITY_MASK     (0x07)

/*
 * Frame header flags
 * - vc: 2 bits
 * - has_auth: 1 bit
 * - has_crypt: 1 bit
 * - arq: 1 bit
 * - has_payload (turha): 1 bit
 * - sequence control: 2 bits
 */
#define SKY_FLAG_AUTHENTICATED          (0b00001)
#define SKY_FLAG_ARQ_ON                 (0b00010)
#define SKY_FLAG_HAS_PAYLOAD            (0b00100)
//#define SKY_FLAG_CRYPT

typedef enum {
	FragmentFirst  = 0,
	FragmentMiddle = 1,
	FragmentLast = 2,
	FragmentStandalone = 3,
} FragmentControl;


// The maximum payload size that fits a worst case frame with all extensions.
#define SKY_PAYLOAD_MAX_LEN             (181)

#define SKY_FRAME_MIN_LEN               (1 + 2 + 4)
#define SKY_FRAME_MAX_LEN               (223) // Limited by Reed-Solomon message length

//#define SKY_PAYLOAD_MAX_LEN             (SKY_FRAME_MAX_LEN - (1 + SKY_MAX_IDENTITY_LEN + SKY_HMAC_LENGTH) )
// (1 + sizeof(ExtTDDControl)) + (1 + sizeof(ExtARQReq) + (1 + sizeof(ExtARQSeq)) + (1 + sizeof(ExtARQCtrl)))


/* frames ========================================================================================== */
struct sky_radio_frame
{
	// Ticks when the start of the frame was detected
	sky_tick_t rx_time_ticks;

	// Length of the raw frame
	unsigned int length;

	//
	uint8_t raw[SKY_FRAME_MAX_LEN + 6];
};

/* frames ========================================================================================== */


/*
 * Extension header Type IDs
 */
#define EXTENSION_ARQ_SEQUENCE          0
#define EXTENSION_ARQ_REQUEST           1
#define EXTENSION_ARQ_CTRL              2
#define EXTENSION_ARQ_HANDSHAKE         3
#define EXTENSION_MAC_TDD_CONTROL       4
#define EXTENSION_HMAC_SEQUENCE_RESET   5


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

/* extensions ====================================================================================== */


typedef struct __attribute__((__packed__)) {
	// Variable length source identity is handled separately

	// TODO: Endianess issue????
	// https://stackoverflow.com/questions/6043483/why-bit-endianness-is-an-issue-in-bitfields

	union {
		struct {
			/* Virtual channel number */
			unsigned int vc : 2;

			/* Flags */
			unsigned int flag_arq_on : 1; // Avoid confusion with packets around ARQ disconnect event.
			unsigned int flag_authenticated : 1;
			unsigned int flag_has_payload : 1;
			unsigned int sequence_control : 2;
			unsigned int reserved : 1;
		};
		uint8_t flags;
	};

	/* Frame sequence number used in authentication */
	unsigned int frame_sequence : 16;

	/* Extension header length */
	unsigned int extension_length : 8;

} SkyStaticHeader;


/* Struct to hold parsed frame information */
typedef struct {
	const uint8_t* identity;
	unsigned int identity_len;
	SkyStaticHeader hdr;
	const SkyHeaderExtension* arq_sequence;
	const SkyHeaderExtension* arq_request;
	const SkyHeaderExtension* arq_ctrl;
	const SkyHeaderExtension* arq_handshake;
	const SkyHeaderExtension* mac_tdd;
	const SkyHeaderExtension* hmac_reset;
	const uint8_t* payload;
	unsigned int payload_len;
} SkyParsedFrame;


/* Struct to hold */
typedef struct {
	SkyStaticHeader* hdr;
	SkyRadioFrame* frame;
	uint8_t *ptr; // write pointer
} SkyTransmitFrame;

/*
 * Allocate a new radio frame object
 * Returns:
 *    Pointer to newly allocated frame struct.
 */
SkyRadioFrame* sky_frame_create();

/*
 * Destroy frame.
 * Args:
 *   frame: Pointer to
 */
void sky_frame_destroy(SkyRadioFrame* frame);

/*
 * Clear the radio frame
 */
void sky_frame_clear(SkyRadioFrame* frame);

/*
 * (internal)
 * Add ARQ sequence number to the frame.
 */
int sky_frame_add_extension_arq_sequence(SkyTransmitFrame *tx_frame, sky_arq_sequence_t sequence);

/*
 * (internal)
 * Add ARQ Retransmit Request header to the frame.
 */
int sky_frame_add_extension_arq_request(SkyTransmitFrame *tx_frame, sky_arq_sequence_t sequence, uint16_t mask);

/*
 * (internal)
 * Add ARQ Control header to the frame.
 */
int sky_frame_add_extension_arq_ctrl(SkyTransmitFrame *tx_frame, sky_arq_sequence_t tx_sequence, sky_arq_sequence_t rx_sequence);

/*
 * (internal)
 * Add ARQ Handshake header to the frame.
 */
int sky_frame_add_extension_arq_handshake(SkyTransmitFrame *tx_frame, uint8_t state_flag, uint32_t identifier);

/*
 * (internal)
 * Add MAC TDD control header to the frame.
 */
int sky_frame_add_extension_mac_tdd_control(SkyTransmitFrame *tx_frame, uint16_t window, uint16_t remaining);

/*
 * (internal)
 * Add HMAC sequence reset header to the frame.
 */
int sky_frame_add_extension_hmac_sequence_reset(SkyTransmitFrame *tx_frame, uint16_t sequence);

/*
 * (internal)
 * Fill the rest of the frame with given payload data.
 */
int sky_frame_extend_with_payload(SkyTransmitFrame *tx_frame, const uint8_t *payload, unsigned int payload_length);

/*
 * Get number of bytes left in the frame.
 */
int sky_frame_get_space_left(const SkyRadioFrame* radioFrame);

/*
 * (internal)
 * Parse and validate all header extensions inside the frame.
 */
int sky_frame_parse_extension_headers(const SkyRadioFrame *frame, SkyParsedFrame *parsed);


#endif // __SKYLINK_FRAME_H__
