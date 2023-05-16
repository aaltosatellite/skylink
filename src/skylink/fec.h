#ifndef __SKYLINK_FEC_H__
#define __SKYLINK_FEC_H__

/*
 * Skylink protocol Forward Error Correction (FEC) defines.
 */

#include "skylink/skylink.h"


#define RS_MSGLEN       223		// Maximum length of message under FEC
#define RS_PARITYS      32		// Number of parity bytes used by FEC. These two in effect sum to 255


/**
 * PHY header high bit definitions
 *
 * Remarks:
 *   These follow the Mode-5 definitions.
 */
#define SKY_GOLAY_PAYLOAD_LENGTH_MASK  0x0FF
//#define SKY_GOLAY_VITERBI_ENABLED      0x800
//#define SKY_GOLAY_RANDOMIZER_ENABLED   0x400
//#define SKY_GOLAY_RS_ENABLED           0x200


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


#endif /* __SKYLINK_FEC_H__ */
