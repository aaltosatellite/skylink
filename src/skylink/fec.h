#ifndef __SKYLINK_FEC_H__
#define __SKYLINK_FEC_H__

/*
 * Skylink protocol Forward Error Correction (FEC) defines.
 */



#define RS_MSGLEN 223
#define RS_PARITYS 32

#define SKY_INCLUDE_DEPENDENCIES
#ifdef SKY_INCLUDE_DEPENDENCIES

#include "ext/libfec/fec.h"
#include "ext/cifra/hmac.h"
#include "ext/gr-satellites/golay24.h"


#else


#include <stdint.h>
#include <stddef.h>



/*
 */
int decode_golay24(uint32_t* data);
int encode_golay24(uint32_t* data);



int decode_rs_8(data_t *data, int *eras_pos, int no_eras, int pad);
void encode_rs_8(data_t *data, data_t *parity, int pad);

//typedef cf_hmac_ctx;
//typedef cf_chash;

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

/*
 * One shot interface: compute `HMAC_hash(key, msg)`, writing the
 * answer (which is `hash->hashsz` long) to `out`.
 *
 * This function does not fail. */
void cf_hmac(const uint8_t *key, size_t nkey,
            const uint8_t *msg, size_t nmsg,
            uint8_t *out,
            const cf_chash *hash);


#endif

#endif /* __SKYLINK_FEC_H__ */
