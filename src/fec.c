/*
 * Skylink Protocol: Sublayer for forward error correction
 */

#include "skylink/skylink.h"
#include "skylink/fec.h"
#include "skylink/diag.h"
#include <string.h>


#if RADIOFRAME_MAXLEN < RS_MSGLEN + RS_PARITYS
#error "Too small buffer for radio frames"
#endif

#if RS_MSGLEN + RS_PARITYS > 255
#error "Too big Reed-Solomon codeword"
#endif

/* Random numbers from Python as whitening pattern */
#define WHITENING_LEN 0x40
static const uint8_t whitening[WHITENING_LEN] = {
		78, 211, 126, 14, 91, 53, 21, 55, 50, 216, 253, 236, 238, 151, 150, 124, 77, 245, 66, 108, 56, 48, 119, 139, 222, 137, 73, 67, 84, 151, 7, 19, 169, 166, 142, 74, 168, 72, 139, 96, 34, 175, 122, 252, 97, 23, 247, 58, 209, 7, 86, 182, 85, 208, 111, 92, 26, 23, 178, 60, 76, 234, 73, 87
};

/* Decode a received frame */
int sky_fec_decode(struct radioframe *frame, struct ap_diag *diag)
{
	if(frame->length != RS_MSGLEN + RS_PARITYS) return -1;

	/* Simply decode in place in the original buffer
	 * and pass it to the next layer together with
	 * the original metadata. */
	unsigned i;
	for (i=0; i<RS_MSGLEN + RS_PARITYS; i++)
		frame->data[i] ^= whitening[i & (WHITENING_LEN-1)];

	int ret = decode_rs_8(frame->data, NULL, 0, 223-RS_MSGLEN);
	frame->length = RS_MSGLEN;
	if(ret < 0) {
		++diag->rx_fec_fail;
		return -1; /* Reed-Solomon decode failed */
	} else {
		++diag->rx_fec_ok;
		diag->rx_fec_errs += ret;
		diag->rx_fec_octs += RS_MSGLEN + RS_PARITYS;
		return ret; /* Decoding success */
	}

}


/* Encode a frame to transmit */
int sky_fec_encode(struct radioframe *frame)
{
	SKY_ASSERT(frame && frame->length < RS_MSGLEN)

	if (frame->length < RS_MSGLEN) {
		/* Zero pad the rest to achieve a constant frame size.
		 * This works because MUX allows it, but this should be better
		 * done within MUX (TODO: move it there) */
		memset(frame->data + frame->length, 0, RS_MSGLEN - frame->length);
	}

	encode_rs_8(frame->data, frame->data+RS_MSGLEN, 223-RS_MSGLEN);
	frame->length = RS_MSGLEN + RS_PARITYS;

	int i;
	for (i=0; i<RS_MSGLEN + RS_PARITYS; i++)
		frame->data[i] ^= whitening[i & (WHITENING_LEN-1)];

	//sky_printf(SKY_DIAG_DEBUG, "FEC: %10u: Frame ready to transmit\n", current_time);
	return 0;
}
