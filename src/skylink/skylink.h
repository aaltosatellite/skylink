//
// Created by elmore on 18.10.2021.
//

#ifndef SKYLINK_SKYLINK_NEW_H
#define SKYLINK_SKYLINK_NEW_H



#include "diag.h"
#include "platform.h"
#include "arq_ring.h"
//#include "skypacket.h"
#include "conf.h"





/*
 * Return codes
 */
#define SKY_RET_OK                  0
#define SKY_RET_INVALID_LENGTH     	(-1)
#define SKY_RET_MALLOC_FAILED      	(-2)

// FEC
#define SKY_RET_GOLAY_FAILED       	(-10)
#define SKY_RET_RS_FAILED          	(-11)
#define SKY_RET_RS_INVALID_LENGTH  	(-12)

// MAC
#define SKY_RET_MAC                	(-20)

// AUTH
#define SKY_RET_AUTH_FAILED        	(-30)
#define SKY_RET_AUTH_MISSING       	(-31)
#define SKY_RET_NO_MAC_SEQUENCE     (-32)
#define SKY_RET_EXCESSIVE_HMAC_JUMP (-33)

// PACKET
#define SKY_RET_INVALID_PACKET		(-40)
#define SKY_RET_PACKET_TOO_LONG		(-41)
#define SKY_RET_INVALID_EXTENSION   (-42)


//FEC
#define SKY_FEC_MARGIN					32


//HMAC
#define SKY_HMAC_LENGTH 				8
#define SKY_FLAG_FRAME_AUTHENTICATED 	0b00000001
#define SKY_FLAG_ARQ_ON 				0b00000010
#define SKY_FLAG_HAS_PAYLOAD 			0b00000100

//Physical layer radio frame structure.
#define SKY_NUM_VIRTUAL_CHANNELS  		4
#define SKY_FRAME_MAX_LEN       		0x100
#define SKY_MAX_EXTENSION_COUNT			8
#define SKY_IDENTITY_LEN				5





//================================================================================================================================
//============ STRUCTS ===========================================================================================================
//================================================================================================================================
/* extensions ====================================================================================== */
struct extension_arq_req {
	uint8_t sequence;
	uint8_t mask1;
	uint8_t mask2;
};
typedef struct extension_arq_req ExtArqReq;


struct extension_arq_setup {
	uint8_t toggle;
	uint8_t enforced_sequence;
};
typedef struct extension_arq_setup ExtArqSeqReset;


struct extension_mac_spec {
	uint16_t window_size;
	uint16_t gap_size;
};
typedef struct extension_mac_spec ExtMACSpec;

struct extension_hmac_tx_reset {
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
	uint8_t type;
	unifExt ext_union;
};
typedef struct skylink_packet_extension_s SkyPacketExtension;
/* extensions ====================================================================================== */


struct frame_metadata {
	int payload_read_length;
	uint8_t* payload_read_start;
	uint8_t auth_verified;
	int32_t rx_time_ms;
};
typedef struct frame_metadata FrameMetadata;

/* Struct to store raw radio frame to be transmitted over the radio or frame which was received */
struct radioframe {
	FrameMetadata metadata;
	uint8_t raw[SKY_FRAME_MAX_LEN + 3 + 1];
	uint16_t length;

	uint8_t identity[SKY_IDENTITY_LEN];
	uint8_t hmac_on;
	uint8_t arq_on;
	uint8_t vc;
	uint8_t n_extensions;
	uint16_t hmac_sequence;
	uint16_t mac_length;
	uint16_t mac_remaining;
	uint8_t arq_sequence;
	SkyPacketExtension extensions[SKY_MAX_EXTENSION_COUNT];
};
typedef struct radioframe SkyRadioFrame;




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





/* Protocol configuration struct.
 *
 * Some of the parameters can be changed while the link is running.
 * Where feasible, sublayer implementations should read their parameters
 * directly from here, allowing configuration changes.
 */
typedef struct sky_conf {
	HMACConfig	hmac;
	SkyArrayConfig array;
	SkyMACConfig mac;
	SkyPHYConfig phy;
	SkyVCConfig vc[SKY_NUM_VIRTUAL_CHANNELS];
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












//================================================================================================================================
//============ FUNCTIONS =========================================================================================================
//================================================================================================================================



#endif //SKYLINK_SKYLINK_NEW_H
