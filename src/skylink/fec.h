#ifndef __SKYLINK_FEC_H__
#define __SKYLINK_FEC_H__

/*
 * Skylink protocol Forward Error Correction (FEC) defines.
 */

#include "skylink.h"
#include "diag.h"
#include <string.h>


#define RS_MSGLEN       223		// Maximum length of message under FEC
#define RS_PARITYS      32		// Number of parity bytes used by FEC. These two in effect sum to 255


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


/*
 * Decode Forward error correcting code on the received frame.
 * Randomizer + Reed-solomon.
 *
 * params:
 *   frame: Frame received over the physical radio link.
 *   diag: Diagnostics telemetry struct.
 *
 * returns:
 *   Returns 0 of success, negative error code on failure.
 */
int sky_fec_decode(SkyRadioFrame *frame, SkyDiagnostics *diag);



/*
 * Encode Forward error correction code on the frame to be transmit.
 *
 * params:
 *   frame: Frame to be encoded.
 *
 * returns:
 *   O on success. Negative return code on error.
 */
int sky_fec_encode(SkyRadioFrame *frame);







//#define SKY_INCLUDE_DEPENDENCIES

#ifdef SKY_INCLUDE_DEPENDENCIES

#include "../ext/libfec/fec.h"
#include "../ext/blake3/blake3.h"
#include "../ext/gr-satellites/golay24.h"

#else

#include <stdint.h>
#include <stddef.h>

typedef uint8_t data_t;

/*
 */
int decode_golay24(uint32_t* data);
int encode_golay24(uint32_t* data);


int decode_rs_8(data_t *data, int *eras_pos, int no_eras, int pad);
void encode_rs_8(data_t *data, data_t *parity, int pad);


#define BLAKE3_KEY_LEN 32
#define BLAKE3_OUT_LEN 32

typedef void blake3_hasher;

void blake3_hasher_init_keyed(blake3_hasher *self, const uint8_t key[BLAKE3_KEY_LEN]);
void blake3_hasher_update(blake3_hasher *self, const void *input, size_t input_len);
void blake3_hasher_finalize(const blake3_hasher *self, uint8_t *out, size_t out_len);

#endif

#endif /* __SKYLINK_FEC_H__ */
