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
 * Physical layer radio frame structure.
 */
#define RADIOFRAME_MAXLEN 0x100
#define RADIOFRAME_RX 0
#define RADIOFRAME_TX 1

// typedef SkyRadioFrame_t
struct radioframe {

	/* Frame metadata */
	uint8_t direction;       //
	int16_t rssi;            //
	timestamp_t timestamp;   //
	int32_t freq;            // Frequency estimate (applicable only for the received frames)

	/* Data */
	uint16_t length;
	uint8_t data[RADIOFRAME_MAXLEN];
};


// MUX stores no state, so TODO remove this?
struct ap_mux;
// State of the MAC/TDD sublayer (forward declaration)
struct ap_mac;

// Configuration for a particular MUX instance
struct ap_mux_conf {
	uint32_t enabled_channels; // Bitmap of virtual channels enabled
};

/* Protocol configuration struct.
 *
 * Some of the parameters can be changed while the link is running.
 * Where feasible, sublayer implementations should read their parameters
 * directly from here, allowing configuration changes.
 */
struct ap_conf {
	char tdd_slave; // 1 if operating as a TDD slave, 0 if master
	timestamp_t initial_time; // TODO: move to init function parameter or something
	struct ap_mux_conf lmux_conf;
	struct ap_mux_conf umux_conf[AP_N_ARQS];
	struct ap_arqtx_conf arqtx_conf;
	struct ap_arqrx_conf arqrx_conf;
};

/* Protocol diagnostic information.
 */
struct ap_diag {
	// Counters

	uint16_t rx_frames;      // Total number of received frames
	uint16_t rx_fec_ok;      // Number of successfully decoded codewords
	uint16_t rx_fec_fail;    // Number of failed decodes
	uint16_t rx_fec_octs;    // Total number of octets succesfully decoded
	uint16_t rx_fec_errs;    // Number of octet errors corrected

	uint16_t tx_frames;      // Total number of transmitted frames
};

/* Struct to store pointers to all the data structures related to a
 * protocol instance.
 *
 * Having these in one place makes it easier to use them from
 * different places, which is particularly useful for MUX,
 * since it ties several different blocks togehter.
 */
struct ap_all {
	struct ap_conf *conf;             // Configuration
	struct ap_diag *diag;             // Diagnostics
	struct ap_buf *rxbuf[AP_RX_BUFS]; // Receive buffers
	struct ap_buf *txbuf[AP_TX_BUFS]; // Transmit buffers
	struct ap_mac *mac;               // MAC state
	struct ap_mux *lmux;              // Lower MUX state
	struct ap_mux *umux[AP_N_ARQS];   // Upper MUX state for each ARQ process
	struct ap_arqrx *arqrx[AP_N_ARQS];    // Receiving ARQ process states
	struct ap_arqtx *arqtx[AP_N_ARQS];    // Sending ARQ process states
};


/* -------------------------------------
 * Interfaces between protocol sublayers
 * ------------------------------------- */

int ap_mux_rx(struct ap_all *ap, const struct ap_mux_conf *conf, const uint8_t *data, int datalen);
int ap_mux_tx(struct ap_all *ap, const struct ap_mux_conf *conf, uint8_t *data, int maxlen);

struct ap_arq *ap_arq_init(const struct ap_conf *conf);
int ap_arq_rx(struct ap_arq *self, const uint8_t *data, int length, const ap_arq_sdu_rx_cb cb, void *const cb_arg);
int ap_arq_tx(struct ap_arq *self, uint8_t *data, int maxlength, const ap_arq_sdu_tx_cb cb, void *const cb_arg);
int ap_arq_reset(struct ap_arq *self);

struct ap_mac *ap_mac_init(struct ap_all *ap/*, const struct ap_conf *conf*/);
int ap_mac_rx(struct ap_all *ap, struct radioframe *frame);
int ap_mac_tx(struct ap_all *ap, struct radioframe *frame, timestamp_t current_time);

int ap_fec_encode(struct radioframe *frame);
int ap_fec_decode(struct radioframe *frame, struct ap_diag *diag);



/* -------------------------
 * Interface to the protocol
 * ------------------------- */

/*
 * Allocate and initialize data structures to store protocol state
 */
struct ap_all * ap_init(struct ap_all *ap, struct ap_conf *conf);

/*
 * Process a received frame
 */
int ap_rx(struct ap_all *ap, struct radioframe *frame);

/*
 * Request a frame to be transmitted
 */
int ap_tx(struct ap_all *ap, struct radioframe *frame, timestamp_t current_time);

/*
 * Print diagnostics of the protocol
 */
int ap_print_diag(struct ap_all *ap);

// TODO: Return some kind of a status for housekeeping and OBC interfacing
int ap_status(struct ap_all *ap, struct ap_status *s);


#endif /* __SKYLINK_H__ */
