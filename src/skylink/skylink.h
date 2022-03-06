#ifndef __SKYLINK_H__
#define __SKYLINK_H__

#include <stdint.h>
//#include "skylink/frame.h"


/*
 * Return codes
 */
#define SKY_RET_OK                          (0)

//RX
#define SKY_RET_INVALID_ENCODED_LENGTH      (-2)
#define SKY_RET_INVALID_PLAIN_LENGTH        (-3)
#define SKY_RET_INVALID_START_BYTE          (-4)
#define SKY_RET_INVALID_VC                  (-5)
#define SKY_RET_INVALID_EXT_LENGTH          (-6)
#define SKY_RET_OWN_TRANSMISSION            (-7)

// FEC
#define SKY_RET_GOLAY_FAILED                (-10)
#define SKY_RET_GOLAY_MISCONFIGURED         (-11)
#define SKY_RET_RS_FAILED                   (-12)
#define SKY_RET_RS_INVALID_LENGTH           (-13)

// MAC
#define SKY_RET_INVALID_MAC_WINDOW_SIZE     (-21)
#define SKY_RET_INVALID_MAC_REMAINING_SIZE  (-22)

// AUTH
#define SKY_RET_AUTH_FAILED                 (-30)
#define SKY_RET_AUTH_MISSING                (-31)
#define SKY_RET_EXCESSIVE_HMAC_JUMP         (-33)
#define SKY_RET_FRAME_TOO_LONG_FOR_HMAC     (-34)
#define SKY_RET_FRAME_TOO_SHORT_FOR_HMAC    (-35)

// PACKET
#define SKY_RET_NO_SPACE_FOR_PAYLOAD        (-40)
#define SKY_RET_UNKNOWN_EXTENSION           (-41)
#define SKY_RET_EXT_DECODE_FAIL             (-42)

// SEQUENCE RING
#define SKY_RET_RING_EMPTY                  (-50)
#define SKY_RET_RING_INVALID_SEQUENCE       (-51)
#define SKY_RET_RING_ELEMENTBUFFER_FAULT    (-52)
#define SKY_RET_RING_BUFFER_FULL            (-53)
#define SKY_RET_RING_RING_FULL              (-54)
#define SKY_RET_RING_PACKET_ALREADY_IN      (-55)
#define SKY_RET_RING_CANNOT_RECALL          (-56)
#define SKY_RET_RING_RESEND_FULL            (-57)
#define SKY_RET_RING_INVALID_ACKNOWLEDGE    (-58)
#define SKY_RET_RING_SEQUENCES_DETACHED     (-59)
#define SKY_RET_RING_SEQUENCES_OUT_OF_SYNC  (-60)
#define SKY_RET_RING_SEQUENCES_IN_SYNC      (0)

// SYSTEM
#define SKY_RET_MALLOC_FAILED               (-70)


#define SKY_NUM_VIRTUAL_CHANNELS            4


//============ STRUCTS ===========================================================================================================
//================================================================================================================================
typedef struct sky_mac_s SkyMAC;

typedef struct sky_hmac SkyHMAC;

typedef struct sky_conf SkyConfig;

typedef struct sky_virtual_channel SkyVirtualChannel;

typedef struct sky_diag SkyDiagnostics;

typedef struct sky_radio_frame SkyRadioFrame;

typedef uint16_t arq_seq_t;


typedef struct __attribute__((__packed__)) {
	/*
	 * ARQ state
	 */
	uint16_t state;

	/*
	 * Amount of free space in bytes in shared element buffer
	 */
	uint16_t buffer_free;

	/*
	 * Number of frames in the buffer waiting to be sent
	 * In realiable mode non-acknownledged frame are included.
	 */
	uint16_t tx_frames;

	/*
	 * Number of frames ready for reading.
	 */
	uint16_t rx_frames;

} SkyVCState;


/* A state information struct provided for higher level software stack. */
typedef struct __attribute__((__packed__)) {
	SkyVCState vc[SKY_NUM_VIRTUAL_CHANNELS];
} SkyState;


/* Struct to store pointers to all the data structures related to a protocol instance. */
struct sky_all {
	SkyConfig*			conf;                 					// Configuration
	SkyDiagnostics*		diag;                 					// Diagnostics
	SkyVirtualChannel* 	virtual_channels[SKY_NUM_VIRTUAL_CHANNELS]; // ARQ capable buffers
	SkyMAC* 			mac;                    				// MAC state
	SkyHMAC* 			hmac;									// HMAC authentication state
};
typedef struct sky_all* SkyHandle;
//============ STRUCTS ===========================================================================================================
//================================================================================================================================



/*
 * Get skylink protocol state
 */
void sky_get_state(SkyHandle self, SkyState* state);

int sky_tx(SkyHandle self, SkyRadioFrame* frame, int insert_golay);

int sky_rx(SkyHandle self, SkyRadioFrame* frame, int contains_golay);

//int sky_transmission_queue_full(SkyHandle self, int vc);

//int sky_push_transmission(SkyHandle self, int vc, uint8_t* paylaod, int length);

//int sky_receive_queue_lengt(SkyHandle self, int vc);

//int sky_receive_transmission(SkyHandle self, int vc, uint8_t* tgt, arq_seq_t* sequence);

//int sky_vc_arq_status(SkyHandle self, int vc);

//int sky_initialze_arq(SkyHandle self, int vc);

//int sky_close_arq(SkyHandle self, int vc);



#endif // __SKYLINK_H__
