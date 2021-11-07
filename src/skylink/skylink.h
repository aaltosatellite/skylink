//
// Created by elmore on 18.10.2021.
//

#ifndef SKYLINK_SKYLINK_NEW_H
#define SKYLINK_SKYLINK_NEW_H


#include "diag.h"
#include "platform.h"
#include "arq_ring.h"
#include "conf.h"





/*
 * Return codes
 */
#define SKY_RET_OK                  		0

//RX
#define SKY_RET_INVALID_ENCODED_LENGTH     	(-2)
#define SKY_RET_INVALID_PLAIN_LENGTH     	(-3)
#define SKY_RET_INVALID_VC      			(-5)
#define SKY_RET_INVALID_EXT_LENGTH  		(-6)

// FEC
#define SKY_RET_GOLAY_FAILED       			(-10)
#define SKY_RET_GOLAY_MISCONFIGURED       	(-11)
#define SKY_RET_RS_FAILED          			(-12)
#define SKY_RET_RS_INVALID_LENGTH  			(-13)

// MAC
#define SKY_RET_MAC                			(-20)
#define SKY_RET_INVALID_MAC_WINDOW_SIZE		(-21)

// AUTH
#define SKY_RET_AUTH_FAILED        			(-30)
#define SKY_RET_AUTH_MISSING       			(-31)
#define SKY_RET_NO_MAC_SEQUENCE     		(-32)
#define SKY_RET_EXCESSIVE_HMAC_JUMP 		(-33)
#define SKY_RET_FRAME_TOO_LONG_FOR_HMAC 	(-34)
#define SKY_RET_FRAME_TOO_SHORT_FOR_HMAC 	(-35)

// PACKET
#define SKY_RET_NO_SPACE_FOR_PAYLOAD   		(-40)
#define SKY_RET_UNKNOWN_EXTENSION   		(-41)
#define SKY_RET_EXT_DECODE_FAIL   			(-42)

//ARQ RING
#define RING_RET_EMPTY						(-50)
#define RING_RET_INVALID_SEQUENCE			(-51)
#define RING_RET_ELEMENTBUFFER_FAULT		(-52)
#define RING_RET_BUFFER_FULL				(-53)
#define RING_RET_RING_FULL					(-54)
#define RING_RET_PACKET_ALREADY_IN			(-55)
#define RING_RET_CANNOT_RECALL				(-56)
#define RING_RET_RESEND_FULL				(-57)

//SYSTEM
#define SKY_RET_MALLOC_FAILED      			(-70)





//================================================================================================================================
//============ STRUCTS ===========================================================================================================
//================================================================================================================================
/* extensions ====================================================================================== */
struct __attribute__((__packed__)) extension_typemask {
	uint8_t type	:4;
	uint8_t length	:4;
};
typedef struct extension_typemask ExtensionTypemask;


struct __attribute__((__packed__)) extension_arq_req  {
	uint8_t type	:4;
	uint8_t length	:4;
	uint8_t sequence;
	uint8_t mask1;
	uint8_t mask2;
};
typedef struct extension_arq_req ExtArqReq;


struct __attribute__((__packed__)) extension_arq_setup {
	uint8_t type	:4;
	uint8_t length	:4;
	uint8_t toggle;
	uint8_t enforced_sequence;
};
typedef struct extension_arq_setup ExtArqSeqReset;


struct __attribute__((__packed__)) extension_mac_spec {
	uint8_t type	:4;
	uint8_t length	:4;
	uint16_t window_size;
	uint16_t gap_size;
};
typedef struct extension_mac_spec ExtMACSpec;


struct __attribute__((__packed__)) extension_hmac_tx_reset {
	uint8_t type	:4;
	uint8_t length	:4;
	uint16_t correct_tx_sequence;
};
typedef struct extension_hmac_tx_reset ExtHMACTxReset;


union extension_union {
	ExtArqReq  ArqReq;
	ExtArqSeqReset  ArqSeqReset;
	ExtMACSpec MACSpec;
	ExtHMACTxReset HMACTxReset;
};
typedef union extension_union unifExt;


struct skylink_packet_extension_s {
	int type;
	unifExt ext_union;
};
typedef struct skylink_packet_extension_s SkyPacketExtension;
/* extensions ====================================================================================== */



/* frames ========================================================================================== */
typedef union {
	struct __attribute__((__packed__)) {
		uint8_t raw[SKY_FRAME_MAX_LEN + 6];
		int32_t length;
	};
	struct __attribute__((__packed__)) {
		uint8_t start_byte;
		uint8_t identity[SKY_IDENTITY_LEN];
		uint8_t vc 		: 3;
		uint8_t flags 	: 5;
		uint8_t ext_length;
		uint16_t auth_sequence;
		uint16_t mac_window;
		uint16_t mac_remaining;
		uint8_t arq_sequence;
	};
} RadioFrame;

typedef struct {
	int32_t rx_time_ms;
	uint8_t auth_verified;
	RadioFrame radioFrame;
} RCVFrame;

typedef struct {
	RadioFrame radioFrame;
} SendFrame;
/* frames ========================================================================================== */



/* MAC-system */
struct sky_mac_s {
	int32_t T0_ms;
	int32_t my_window_length;
	int32_t peer_window_length;
	int32_t gap_constant;
	int32_t tail_constant;
};
typedef struct sky_mac_s MACSystem;




/* HMAC runtime state */
struct sky_hmac {
	uint8_t* key;
	int32_t key_len;
	int32_t sequence_tx[SKY_NUM_VIRTUAL_CHANNELS];
	int32_t sequence_rx[SKY_NUM_VIRTUAL_CHANNELS];
	uint8_t vc_enfocement_need[SKY_NUM_VIRTUAL_CHANNELS];
	void* ctx;
};
typedef struct sky_hmac SkyHMAC;





/*
 * Protocol configuration struct.
 *
 * Some of the parameters can be changed while the link is running.
 * Where feasible, sublayer implementations should read their parameters
 * directly from here, allowing configuration changes.
 */
typedef struct sky_conf {
	SkyPHYConfig phy;
	SkyMACConfig mac;
	HMACConfig	hmac;
	SkyArrayConfig array[SKY_NUM_VIRTUAL_CHANNELS];
	SkyVCConfig vc[SKY_NUM_VIRTUAL_CHANNELS];
	uint8_t vc_priority[SKY_NUM_VIRTUAL_CHANNELS];
	uint8_t identity[SKY_IDENTITY_LEN];
} SkyConfig;





/*
 * Struct to store pointers to all the data structures related to a
 * protocol instance.
 * SkyPhyConfig_t phy_conf;
 * Having these in one place makes it easier to use them from
 * different places, which is particularly useful for MUX,
 * since it ties several different blocks togehter.
 */
struct sky_all {
	SkyConfig*		conf;                 					// Configuration
	SkyDiagnostics*	diag;                 					// Diagnostics
	SkyArqRing* 	arrayBuffers[SKY_NUM_VIRTUAL_CHANNELS]; //ARQ capable buffers.
	MACSystem* 		mac;                    				// MAC state
	SkyHMAC* 		hmac;
};
typedef struct sky_all* SkyHandle;
//================================================================================================================================
//============ STRUCTS ===========================================================================================================
//================================================================================================================================


int content_to_send(SkyHandle self, uint8_t vc);

int any_content_to_send(SkyHandle self);

int sky_tx(SkyHandle self, SendFrame* frame, uint8_t vc, int insert_golay);

int sky_rx(SkyHandle self, RCVFrame* frame, int contains_golay);




#endif //SKYLINK_SKYLINK_NEW_H
