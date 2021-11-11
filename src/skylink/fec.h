#ifndef __SKYLINK_FEC_H__
#define __SKYLINK_FEC_H__

/*
 * Skylink protocol Forward Error Correction (FEC) defines.
 */

#include "skylink.h"
#include "skylink/diag.h"
#include <string.h>

/*
 */
#define RS_MSGLEN       223
#define RS_PARITYS      32


/**
 * PHY header high bit definitions
 *
 * Remarks:
 *   These follow the Mode-5 definitions.
 */
#define SKY_GOLAY_PAYLOAD_LENGTH_MASK  0x0FF
#define SKY_GOLAY_VITERBI_ENABLED      0x800
#define SKY_GOLAY_RANDOMIZER_ENABLED   0x400
#define SKY_GOLAY_RS_ENABLED           0x200



int sky_fec_decode(SkyRadioFrame *frame, SkyDiagnostics *diag);

int sky_fec_encode(SkyRadioFrame *frame);


/*
 * Decode Forward error correcting code on the received frame.
 * Randomizer + Reed-solomon.
 *
 * params:
 *   frame: Frame received over the physical radio link.
 *   diag: Diagnostics telemetry struct.
 *
 * returns:
 *   Returns the number of errors corrected.
 *   On error returns a negative return code.
 */
//int sky_fec_decode(SkyRadioFrame_t *frame, SkyDiagnostics *diag);


/*
 * Encode Forward error correction code on the frame to be transmit.
 *
 * params:
 *   frame: Frame to be encoded.
 *
 * returns:
 *   O on success. Negative return code on error.
 */
//int sky_fec_encode(SkyRadioFrame_t *frame)



#define SKY_INCLUDE_DEPENDENCIES

#ifdef SKY_INCLUDE_DEPENDENCIES

#include "../ext/libfec/fec.h"
#include "../ext/cifra/sha2.h"
#include "../ext/cifra/hmac.h"
#include "../ext/gr-satellites/golay24.h"

#define SKY_HMAC_CTX_SIZE  (sizeof(cf_hmac_ctx))

#else


#include <stdint.h>
#include <stddef.h>

typedef uint8_t data_t;
//typedef struct  cf_hmac_ctx;
//typedef int cf_chash;

/*
 */
int decode_golay24(uint32_t* data);
int encode_golay24(uint32_t* data);



int decode_rs_8(data_t *data, int *eras_pos, int no_eras, int pad);
void encode_rs_8(data_t *data, data_t *parity, int pad);


#define SKY_HMAC_CTX_SIZE  (768)


typedef void cf_hmac_ctx;
typedef void cf_chash;

extern cf_chash cf_sha256;

/* Set up ctx for computing a HMAC using the given hash and key. */
void cf_hmac_init(cf_hmac_ctx *ctx,
                 const cf_chash *hash,
                 const uint8_t *key, size_t nkey);

/* Input data. */
void cf_hmac_update(cf_hmac_ctx *ctx,
                   const void *data, size_t ndata);

/* Finish and compute HMAC.
 * `ctx->hash->hashsz` bytes are written to `out`. */
void cf_hmac_finish(cf_hmac_ctx *ctx, uint8_t *out);


#endif

#endif /* __SKYLINK_FEC_H__ */
