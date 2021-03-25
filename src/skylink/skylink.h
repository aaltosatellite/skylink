#ifndef __SKYLINK_H__
#define __SKYLINK_H__

#include "skylink/buf.h"
#include "skylink/diag.h"
#include "skylink/arq.h"
#include "platform/timestamp.h"

/* --------------------------
 * Compile-time configuration
 * -------------------------- */

#define SKY_NUM_VIRTUAL_CHANNELS  4

/**/
typedef enum {
	VC0 = 0,
	VC1,
	VC2,
	VC3
} SkyVirtualChannel_t;


// Number of receive packet buffers
#define AP_RX_BUFS 4
// Number of transmit packet buffers
#define AP_TX_BUFS 4
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
// FEC
#define SKY_RET_GOLAY_FAILED       -1
#define SKY_RET_RS_FAILED          -2
#define SKY_RET_RS_INVALID_LENGTH  -3



/*
 * Physical layer radio frame structure.
 */
#define RADIOFRAME_MAXLEN 0x100
#define RADIOFRAME_RX 0
#define RADIOFRAME_TX 1


typedef struct {

	/* Frame metadata */
	int16_t rssi;            //
	int32_t freq;            // Frequency estimate (applicable only for the received frames)

} SkyRadioFrameMetadata_t;


#define SKY_FRAME_AUTHENTICATED 0x0001

/*
 * Struct to store raw radio frame to be transmitted over the radio or frame which was received
 */
typedef struct radioframe {

	uint8_t direction;       //
	timestamp_t timestamp;   //
	SkyRadioFrameMetadata_t meta;

	/* Data */
	uint16_t length;  // Decoded length
	uint16_t flags;

	union {
		struct {
			uint8_t phy_header[3];
			uint8_t phy_flags[3];

		};

		uint8_t raw[RADIOFRAME_MAXLEN];
	};

} SkyRadioFrame_t;


// MUX stores no state, so TODO remove this?
struct ap_mux;
// State of the MAC/TDD sublayer (forward declaration)
struct ap_mac;


#include "skylink/conf.h"



/* Protocol configuration struct.
 *
 * Some of the parameters can be changed while the link is running.
 * Where feasible, sublayer implementations should read their parameters
 * directly from here, allowing configuration changes.
 */
struct ap_conf {

	SkyMACConfig_t mac;
	SkyPHYConfig_t phy;

	char tdd_slave; // 1 if operating as a TDD slave, 0 if master
	timestamp_t initial_time; // TODO: move to init function parameter or something

/*
	struct ap_mux_conf lmux_conf;
	struct ap_mux_conf umux_conf[AP_N_ARQS];
	struct ap_arqtx_conf arqtx_conf;
	struct ap_arqrx_conf arqrx_conf;
*/
};

typedef struct ap_conf SkyConfig_t;


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
struct ap_all {
	SkyConfig_t    *conf;                 // Configuration
	SkyDiagnostics_t *diag;                 // Diagnostics
	struct ap_buf *rxbuf[SKY_NUM_VIRTUAL_CHANNELS]; // Receive buffers
	struct ap_buf *txbuf[SKY_NUM_VIRTUAL_CHANNELS]; // Transmit buffers
	struct ap_mac *mac;                   // MAC state
	//struct ap_mux *lmux;                  // Lower MUX state
	//struct ap_mux *umux[AP_N_ARQS];       // Upper MUX state for each ARQ process
	//struct ap_arqrx *arqrx[AP_N_ARQS];    // Receiving ARQ process states
	//struct ap_arqtx *arqtx[AP_N_ARQS];    // Sending ARQ process states

	void* hmac_ctx;
};

typedef struct ap_all* SkyHandle_t;


/* -------------------------------------
 * Interfaces between protocol sublayers
 * ------------------------------------- */

//int ap_mux_rx(struct ap_all *ap, const struct ap_mux_conf *conf, const uint8_t *data, int datalen);
//int ap_mux_tx(struct ap_all *ap, const struct ap_mux_conf *conf, uint8_t *data, int maxlen);

struct ap_arq *ap_arq_init(const struct ap_conf *conf);
int ap_arq_rx(struct ap_arq *self, const uint8_t *data, int length, const ap_arq_sdu_rx_cb cb, void *const cb_arg);
int ap_arq_tx(struct ap_arq *self, uint8_t *data, int maxlength, const ap_arq_sdu_tx_cb cb, void *const cb_arg);
int ap_arq_reset(struct ap_arq *self);

struct ap_mac *ap_mac_init(struct ap_all *ap/*, const struct ap_conf *conf*/);
int ap_mac_rx(struct ap_all *ap, SkyRadioFrame_t *frame);
int ap_mac_tx(struct ap_all *ap, SkyRadioFrame_t *frame, timestamp_t current_time);

int ap_fec_encode(SkyRadioFrame_t *frame);
int ap_fec_decode(SkyRadioFrame_t *frame, struct ap_diag *diag);



/* -------------------------
 * Interface to the protocol
 * ------------------------- */

/*
 * Allocate and initialize data structures to store protocol state
 */
SkyHandle_t sky_init(struct ap_all *ap, struct ap_conf *conf);

/*
 * Process a received frame
 */
int sky_rx(struct ap_all *ap,  SkyRadioFrame_t *frame);
int sky_rx_raw(struct ap_all *ap, struct radioframe *frame);

/*
 * Request a frame to be transmitted
 */
int sky_tx(struct ap_all *ap, SkyRadioFrame_t *frame, timestamp_t current_time);

/*
 * Write to Virtual Channel transmit buffer.
 */
int sky_vc_write(struct ap_all *ap, SkyVirtualChannel_t vc, const uint8_t *data, unsigned datalen, unsigned flags);

/*
 * Indicate the protocol logic that a carrier has been sensed.
 */
int sky_mac_carrier_sensed(timestamp_t t);

/*
 * Print diagnostics of the protocol
 */
int sky_print_diag(struct ap_all *ap);

// TODO: Return some kind of a status for housekeeping and OBC interfacing
//int sky_status(struct ap_all *ap, struct ap_status *s);


int sky_init_hmac(SkyHandle_t self, const char* key, unsigned int key_len);
int sky_authenticate(SkyHandle_t self, SkyRadioFrame_t* frame);
int sky_check_authentication(SkyHandle_t self, SkyRadioFrame_t* frame);


#endif /* __SKYLINK_H__ */
