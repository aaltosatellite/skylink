#ifndef __SKYLINK_H__
#define __SKYLINK_H__

#include "skylink/buf.h"
#include "skylink/diag.h"
#include "skylink/arq.h"
#include "platform/timestamp.h"

#include "skylink/platform.h"



/*
 * Virtual channel type define..
 */
typedef enum {
	VC0 = 0,
	VC1,
	VC2,
	VC3
} SkyVirtualChannel_t;

#define SKY_NUM_VIRTUAL_CHANNELS  4



// Number of ARQ processes
#define AP_N_ARQS 1


/* ----------
 * Data types
 * ---------- */

#define LINK_MESSAGE_MAXLEN 0x40

struct link_message {
	int len;
	uint8_t data[LINK_MESSAGE_MAXLEN];
};


/*
 * Return codes
 */
#define SKY_RET_OK                  0
#define SKY_RET_INVALID_LENGTH     -1
#define SKY_RET_MALLOC_FAILED      -2

// FEC
#define SKY_RET_GOLAY_FAILED       -10
#define SKY_RET_RS_FAILED          -11
#define SKY_RET_RS_INVALID_LENGTH  -12

// MAC
#define SKY_RET_MAC                -20

// AUTH
#define SKY_RET_AUTH_UNINITIALIZED -30
#define SKY_RET_AUTH_FAILED        -31
//



/*
 * Physical layer radio frame structure.
 */
//#define RADIOFRAME_MAXLEN 0x100
#define RADIOFRAME_RX 0
#define RADIOFRAME_TX 1

#define SKY_FRAME_MAX_LEN       0x100

typedef struct {

	/* Frame metadata */
	int16_t rssi;            //
	int16_t freq;            // Frequency estimate (applicable only for the received frames)

} SkyRadioFrameMetadata_t;



#define SKY_FRAME_AUTHENTICATED 0x0001


/*
 * Struct to store raw radio frame to be transmitted over the radio or frame which was received
 */
typedef struct radioframe {

	uint8_t direction;       //
	timestamp_t timestamp;   //
	SkyRadioFrameMetadata_t meta;

	uint16_t length;
	uint16_t raw_length;

	union {
	struct {

	/**
	 * PHY header  (3 bytes)
	 *
	 * Remarks: For alignment reason there's a single reserved by in the
	 */
	struct __attribute__((packed)) {
		uint8_t _res;
		uint8_t flags;
		uint16_t length;
	} phy;

	//uint32_t phy_header;

	/**
	 *  Data link layer header
	 */
	struct __attribute__((packed)) {
		uint8_t version;
		uint8_t flags;
		uint32_t apid;
	} hdr;

	uint8_t payload[0];

	};
	uint8_t raw[SKY_FRAME_MAX_LEN];
	};

} SkyRadioFrame_t;






// State of the MAC/TDD sublayer (forward declaration)
struct ap_mac;
struct sky_hmac;

typedef struct sky_hmac SkyHMAC_t;

#include "skylink/conf.h"



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

} SkyConfig_t;


/* Protocol diagnostic information.
 */
typedef struct ap_diag {

	uint16_t rx_frames;      // Total number of received frames
	uint16_t rx_fec_ok;      // Number of successfully decoded codewords
	uint16_t rx_fec_fail;    // Number of failed decodes
	uint16_t rx_fec_octs;    // Total number of octets successfully decoded
	uint16_t rx_fec_errs;    // Number of octet errors corrected

	uint16_t tx_frames;      // Total number of transmitted frames

} SkyDiagnostics_t;



/*
 * Struct to store pointers to all the data structures related to a
 * protocol instance.
 *	SkyPhyConfig_t phy_conf;
 * Having these in one place makes it easier to use them from
 * different places, which is particularly useful for MUX,
 * since it ties several different blocks togehter.
 */
struct sky_all {
	SkyConfig_t      *conf;                 // Configuration
	SkyDiagnostics_t *diag;                 // Diagnostics
	SkyBuffer_t *rxbuf[SKY_NUM_VIRTUAL_CHANNELS]; // Receive buffers
	SkyBuffer_t *txbuf[SKY_NUM_VIRTUAL_CHANNELS]; // Transmit buffers
	struct ap_mac *mac;                    // MAC state

	SkyHMAC_t* hmac;
};

typedef struct sky_all* SkyHandle_t;


/* -------------------------
 * Interface to the protocol
 * ------------------------- */

/*
 * Allocate and initialize data structures to store protocol state
 */
SkyHandle_t sky_init(SkyHandle_t self, SkyConfig_t *conf);

/*
 * Process a received frame
 */
int sky_rx(SkyHandle_t self,  SkyRadioFrame_t *frame);
int sky_rx_raw(SkyHandle_t self, SkyRadioFrame_t *frame);

/*
 * Request a frame to be transmitted
 */
int sky_tx(SkyHandle_t self, SkyRadioFrame_t *frame, timestamp_t current_time);
int sky_tx_raw(SkyHandle_t self, SkyRadioFrame_t *frame, timestamp_t current_time);

/*
 * Write to Virtual Channel transmit buffer.
 */
int sky_vc_write(SkyHandle_t self, SkyVirtualChannel_t vc, const uint8_t *data, unsigned datalen, unsigned flags);

/*
 * Indicate the protocol logic that a carrier has been sensed.
 */
int sky_mac_carrier_sensed(timestamp_t t);

/*
 * Print diagnostics of the protocol
 */
int sky_print_diag(SkyHandle_t self);
int sky_clear_stats(SkyHandle_t self);


// TODO: Return some kind of a status for housekeeping and OBC interfacing
//int sky_status(struct ap_all *ap, struct ap_status *s);



int sky_set_config(SkyHandle_t self, int cfg, unsigned int val);
int sky_get_config(SkyHandle_t self, int cfg, unsigned int* val);

#endif /* __SKYLINK_H__ */
