//
// Created by elmore on 18.10.2021.
//

#ifndef SKYLINK_SKYLINK_NEW_H
#define SKYLINK_SKYLINK_NEW_H


//#include "skylink/buf.h"
#include "skylink/diag.h"
//#include "skylink/arq.h"
#include "skylink/platform.h"
#include "skylink/elementbuffer.h"
#include "skylink/skylink_array.h"
#include "skylink/conf.h"





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
#define SKY_RET_AUTH_UNINITIALIZED 	(-30)
#define SKY_RET_AUTH_FAILED        	(-31)
#define SKY_RET_AUTH_MISSING       	(-32)

// PACKET
#define SKY_RET_INVALID_PACKET		(-40)
#define SKY_RET_PACKET_TOO_LONG		(-41)
#define SKY_RET_INVALID_EXTENSION   (-42)



//HMAC
#define SKY_HMAC_LENGTH 8
#define SKY_FRAME_AUTHENTICATED 	0x0001

//Physical layer radio frame structure.
#define SKY_NUM_VIRTUAL_CHANNELS  	4
#define SKY_FRAME_MAX_LEN       	0x100
#define SKY_MAX_EXTENSION_COUNT		8
#define SKY_IDENTITY_LEN			5

//MAC-system definitions
#define MAXIMUM_WINDOW_LENGTH			3000	//todo: probably should be in config.
#define MINIMUM_WINDOW_LENGTH			300		//todo: probably should be in config.




//================================================================================================================================
//============ STRUCTS ===========================================================================================================
//================================================================================================================================
typedef struct {
	/* Frame metadata */
	int16_t rssi;            //
	int16_t freq;            // Frequency estimate (applicable only for the received frames)
} SkyRadioFrameMetadata_t;


struct extension_arq_num {
	uint8_t sequence;
};
typedef struct extension_arq_num ExtArqNum;


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
typedef struct extension_arq_setup ExtArqSetup;


struct extension_mac_spec {
	uint16_t default_window_size;
	uint16_t gap_size;
};
typedef struct extension_mac_spec ExtMACSpec;


union extension_union {
	ExtArqNum  ArqNum;
	ExtArqReq  ArqReq;
	ExtArqSetup  ArqSetup;
	ExtMACSpec MACSpec;
};
typedef union extension_union unifExt;


struct skylink_packet_extension_s {
	uint8_t type;
	unifExt extension;
};
typedef struct skylink_packet_extension_s SkyPacketExtension;


//MAC-system
struct sky_mac_s {
	int32_t T0_ms;
	int32_t my_window_length;
	int32_t peer_window_length;
	int32_t gap_constant;
	int32_t cycle_time;
};
typedef struct sky_mac_s MACSystem;



/*
 * Struct to store raw radio frame to be transmitted over the radio or frame which was received
 */
struct radioframe {
	uint8_t raw[SKY_FRAME_MAX_LEN];
	uint16_t length;

	uint8_t flags;

	timestamp_t timestamp;
	uint8_t identity[SKY_IDENTITY_LEN];
	uint8_t vc;
	uint16_t hmac_sequence;
	uint8_t arq_sequence;
	uint16_t mac_length;
	uint16_t mac_left;

	SkyPacketExtension extensions[SKY_MAX_EXTENSION_COUNT];
	uint8_t n_extensions;

	uint16_t payload_length;
	uint8_t* payload;

};
typedef struct radioframe SkyRadioFrame;


/*
 * HMAC runtime state
 */
struct sky_hmac {
	const uint8_t* key;
	unsigned int key_len;
	unsigned int seq_num[SKY_NUM_VIRTUAL_CHANNELS];
	void* ctx;
};
typedef struct sky_hmac SkyHMAC_t;





/* Protocol configuration struct.
 *
 * Some of the parameters can be changed while the link is running.
 * Where feasible, sublayer implementations should read their parameters
 * directly from here, allowing configuration changes.
 */
typedef struct sky_conf {

	SkyMACConfig_t mac;
	SkyPHYConfig_t phy;
	SkyVCConfig_t vc[SKY_NUM_VIRTUAL_CHANNELS];
	SkyARQConfig_t arq;
	uint8_t identity[SKY_IDENTITY_LEN];

} SkyConfig_t;


/*
 * Protocol diagnostic information.
 */
typedef struct sky_diag {

	uint16_t rx_frames;      // Total number of received frames
	uint16_t rx_fec_ok;      // Number of successfully decoded codewords
	uint16_t rx_fec_fail;    // Number of failed decodes
	uint16_t rx_fec_octs;    // Total number of octets successfully decoded
	uint16_t rx_fec_errs;    // Number of octet errors corrected

	uint16_t tx_frames;      // Total number of transmitted frames

} SkyDiagnostics_t;


/*
 * Buffer state response struct
 */
typedef struct {
	uint8_t state[SKY_NUM_VIRTUAL_CHANNELS];
	uint16_t rx_free[SKY_NUM_VIRTUAL_CHANNELS];
	uint16_t tx_avail[SKY_NUM_VIRTUAL_CHANNELS];
} SkyBufferState_t;

/*
 * Struct to store pointers to all the data structures related to a
 * protocol instance.
 *	SkyPhyConfig_t phy_conf;
 * Having these in one place makes it easier to use them from
 * different places, which is particularly useful for MUX,
 * since it ties several different blocks togehter.
 */
struct sky_all {
	SkyConfig_t*		conf;                 // Configuration
	SkyDiagnostics_t*	diag;                 // Diagnostics
	SkylinkArray* 		arayBuffers[SKY_NUM_VIRTUAL_CHANNELS]; //ARQ capable buffers.
	MACSystem* 			mac;                    // MAC state
	SkyHMAC_t* 			hmac;
};
typedef struct sky_all* SkyHandle_t;


struct sky_instance {
	SkyConfig_t			conf;
	SkyDiagnostics_t*	diag;
};
//================================================================================================================================
//============ STRUCTS ===========================================================================================================
//================================================================================================================================












//================================================================================================================================
//============ FUNCTIONS =========================================================================================================
//================================================================================================================================
/*
 * Allocate and initialize data structures to store protocol state
 */
SkyHandle_t sky_init(SkyHandle_t self, SkyConfig_t *conf);


/*
 * Process a received frame
 */
int sky_rx(SkyHandle_t self,  SkyRadioFrame *frame);
int sky_rx_raw(SkyHandle_t self, SkyRadioFrame *frame);


/*
 * Request a frame to be transmitted
 */
int sky_tx(SkyHandle_t self, SkyRadioFrame *frame, timestamp_t current_time);
int sky_tx_raw(SkyHandle_t self, SkyRadioFrame *frame, timestamp_t current_time);


/*
 * Indicate the protocol logic that a carrier has been sensed.
 */
int sky_mac_carrier_sensed(timestamp_t t);


/*
 * Print diagnostics of the protocol
 */
int sky_print_diag(SkyHandle_t self);
int sky_clear_stats(SkyHandle_t self);


int sky_get_buffer_status(SkyHandle_t self, SkyBufferState_t* state);
int sky_flush_buffers(SkyHandle_t self);


// TODO: Return some kind of a status for housekeeping and OBC interfacing
//int sky_status(struct ap_all *ap, struct ap_status *s);


/*
 * Packet decode and encode from bytearray to individual fields. Does NOT contian Golay24 or FEC
 */
int decode_skylink_packet(SkyRadioFrame* frame);
int encode_skylink_packet(SkyHandle_t self, SkyRadioFrame* frame);

/*
 * Configuartions.
 */
int sky_set_config(SkyHandle_t self, unsigned int cfg, unsigned int val);
int sky_get_config(SkyHandle_t self, unsigned int cfg, unsigned int* val);


#endif //SKYLINK_SKYLINK_NEW_H
