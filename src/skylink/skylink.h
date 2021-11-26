#ifndef __SKYLINK_H__
#define __SKYLINK_H__


#include "diag.h"
#include "platform.h"
#include "arq_ring.h"
#include "conf.h"
#include "frame.h"





/*
 * Return codes
 */
#define SKY_RET_OK                  		0

//RX
#define SKY_RET_INVALID_ENCODED_LENGTH     	(-2)
#define SKY_RET_INVALID_PLAIN_LENGTH     	(-3)
#define SKY_RET_INVALID_VC      			(-5)
#define SKY_RET_INVALID_EXT_LENGTH  		(-6)
#define SKY_RET_OWN_TRANSMISSION  			(-7)

// FEC
#define SKY_RET_GOLAY_FAILED       			(-10)
#define SKY_RET_GOLAY_MISCONFIGURED       	(-11)
#define SKY_RET_RS_FAILED          			(-12)
#define SKY_RET_RS_INVALID_LENGTH  			(-13)

// MAC
#define SKY_RET_INVALID_MAC_WINDOW_SIZE		(-21)

// AUTH
#define SKY_RET_AUTH_FAILED        			(-30)
#define SKY_RET_AUTH_MISSING       			(-31)
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
#define RING_RET_INVALID_ACKNOWLEDGE		(-58)
#define RING_RET_SEQUENCES_DETACHED			(-59)
#define RING_RET_SEQUENCES_OUT_OF_SYNC		(-60)
#define RING_RET_SEQUENCES_IN_SYNC			0

//SYSTEM
#define SKY_RET_MALLOC_FAILED      			(-70)





//================================================================================================================================
//============ STRUCTS ===========================================================================================================
//================================================================================================================================





typedef struct sky_mac_s SkyMAC;




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





typedef struct {


} SkylinkChannelState;

/* A state information struct provided for higher level software stack. */
typedef struct {
	SkylinkChannelState v_channels[SKY_NUM_VIRTUAL_CHANNELS];


} SkylinkState;






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
	SkyMAC* 		mac;                    				// MAC state
	SkyHMAC* 		hmac;
};
typedef struct sky_all* SkyHandle;
//================================================================================================================================
//============ STRUCTS ===========================================================================================================
//================================================================================================================================

int sky_tx(SkyHandle self, SkyRadioFrame* frame, int insert_golay, int32_t now_ms);

int sky_rx(SkyHandle self, SkyRadioFrame* frame, int contains_golay);




#endif // __SKYLINK_H__
