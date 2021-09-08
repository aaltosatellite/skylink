#ifndef __SKYLINK_ARQ_H__
#define __SKYLINK_ARQ_H__
/*
 * Skylink Protocol: ARQ sublayer
 *
 * Functions declared here implement selective-reject automatic
 * repeat-request (ARQ) functionality in the protocol.
 *
 * The ARQ implementation does not depend on other parts of the protocol
 * code, so it can be tested separately if desired.
 *
 * Implementation is split into a sending end and a receiving end.
 * Data structures are defined to store the internal state of each end:
 * struct ap_arqtx_state and struct ap_arqrx_state.
 *
 * Configuration TODO.
 * Some of the configuration parameters may be changed in between calls.
 * Thread safety, however, is not guaranteed (since it's not needed,
 * so why spend time testing it), so please don't modify the
 * configuration from another thread while an ARQ function is running.
 */

#include <stdint.h>

#include "skylink/conf.h"

/* -------------------------------------
 * Compile-time configuration parameters
 * ------------------------------------- */

// Maximum length of an ARQ SDU
#define AP_ARQ_SDU_LEN 100


/* -------------------------
 * Data structures and types
 * ------------------------- */

// Run-time configuration parameters of sending end
struct ap_arqtx_conf {
	// TODO
};

// Run-time configuration parameters of receiving end
struct ap_arqrx_conf {
	// TODO
};

// Diagnostics of sending end
struct ap_arqtx_diag {
};

// Diagnostics of receiving end
struct ap_arqrx_diag {
};

// State of transmitting end of an ARQ process
struct ap_arqtx;

// State of receiving end of an ARQ process
struct ap_arqrx;


/* -------------------------------
 * Interface to a transmitting end
 * ------------------------------- */

/* SDU request callback.
 *
 * Called by ap_arqtx_tx to request SDUs from an upper layer.
 */
typedef int (*ap_arq_sdu_tx_cb) (void *arg, uint8_t *data, int maxlen);

/* Request an ARQ PDU to be transmitted.
 *
 * Update the sender state.
 * Whenever it's possible to transmit a new PDU, the function requests one
 * by calling the given callback.
 *
 * Return value is length of the data if there is a PDU
 * to transmit, -1 if not.
 */
int ap_arqtx_tx(struct ap_arqtx *state, const SkyARQConfig_t *conf, uint8_t *data, int maxlen, const ap_arq_sdu_tx_cb cb, void *const cb_arg);

/* Process a received acknowledgement PDU.
 */
int ap_arqtx_rx_ack(struct ap_arqtx *state, const SkyARQConfig_t *conf, const uint8_t *data, int length);

/* Reset a transmitter state.
 */
int ap_arqtx_reset(struct ap_arqtx *self, const SkyARQConfig_t *conf);

/* Allocate and initialize a transmitter state struct.
 */
struct ap_arqtx *ap_arqtx_init(const SkyARQConfig_t *conf);

/* Print the state for diagnostics */
int ap_arqtx_print(struct ap_arqtx *state);


/* ----------------------------
 * Interface to a receiving end
 * ---------------------------- */

/* SDU received callback.
 *
 * Called by ap_arqrx_rx to pass SDUs to an upper layer.
 */
typedef int (*ap_arq_sdu_rx_cb) (void *arg, const uint8_t *data, int datalen);

/* Process a received ARQ PDU.
 *
 * Update the receiver state.
 * Whenever a new SDU has been successfully received (in order),
 * the function calls the given callback.
 */
int ap_arqrx_rx(struct ap_arqrx *state, const SkyARQConfig_t *conf, const uint8_t *data, int length, const ap_arq_sdu_rx_cb cb, void *const cb_arg);

/* Request an acknowledgment PDU to be transmitted.
 *
 * Return value is length of the data (always 3) if there is an ACK
 * to transmit, -1 if not.
 */
int ap_arqrx_tx_ack(struct ap_arqrx *state, const SkyARQConfig_t *conf, uint8_t *data, int maxlen);

/* Reset a receiver state.
 */
int ap_arqrx_reset(struct ap_arqrx *self, const SkyARQConfig_t *conf);

/* Allocate and initialize a receiver state struct.
 */
struct ap_arqrx *ap_arqrx_init(const SkyARQConfig_t *conf);

/* Print the state for diagnostics */
int ap_arqrx_print(struct ap_arqrx *state);


#endif /* __SKYLINK_ARQ_H__ */
