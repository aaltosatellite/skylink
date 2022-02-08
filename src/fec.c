/*
 * Skylink Protocol: Sublayer for forward error correction
 */


#include "skylink/fec.h"
#include "skylink/diag.h"
#include "skylink/skylink.h"
#include "skylink/conf.h"
#include "skylink/frame.h"



#if SKY_FRAME_MAX_LEN < RS_MSGLEN + RS_PARITYS
#error "Too small buffer for radio frames"
#endif

#if RS_MSGLEN + RS_PARITYS > 255
#error "Too big Reed-Solomon codeword"
#endif


#if 1
/*
 * The pseudo random sequence generated using the polynomial  h(x) = x8 + x7 + x5 + x3 + 1.
 * Ref: CCSDS 131.0-B-3, 10.4.1
 */
#define WHITENING_LEN   256
static const uint8_t whitening[WHITENING_LEN] = {
	0xff, 0x48, 0x0e, 0xc0, 0x9a, 0x0d, 0x70, 0xbc,
	0x8e, 0x2c, 0x93, 0xad, 0xa7, 0xb7, 0x46, 0xce,
	0x5a, 0x97, 0x7d, 0xcc, 0x32, 0xa2, 0xbf, 0x3e,
	0x0a, 0x10, 0xf1, 0x88, 0x94, 0xcd, 0xea, 0xb1,
	0xfe, 0x90, 0x1d, 0x81, 0x34, 0x1a, 0xe1, 0x79,
	0x1c, 0x59, 0x27, 0x5b, 0x4f, 0x6e, 0x8d, 0x9c,
	0xb5, 0x2e, 0xfb, 0x98, 0x65, 0x45, 0x7e, 0x7c,
	0x14, 0x21, 0xe3, 0x11, 0x29, 0x9b, 0xd5, 0x63,
	0xfd, 0x20, 0x3b, 0x02, 0x68, 0x35, 0xc2, 0xf2,
	0x38, 0xb2, 0x4e, 0xb6, 0x9e, 0xdd, 0x1b, 0x39,
	0x6a, 0x5d, 0xf7, 0x30, 0xca, 0x8a, 0xfc, 0xf8,
	0x28, 0x43, 0xc6, 0x22, 0x53, 0x37, 0xaa, 0xc7,
	0xfa, 0x40, 0x76, 0x04, 0xd0, 0x6b, 0x85, 0xe4,
	0x71, 0x64, 0x9d, 0x6d, 0x3d, 0xba, 0x36, 0x72,
	0xd4, 0xbb, 0xee, 0x61, 0x95, 0x15, 0xf9, 0xf0,
	0x50, 0x87, 0x8c, 0x44, 0xa6, 0x6f, 0x55, 0x8f,
	0xf4, 0x80, 0xec, 0x09, 0xa0, 0xd7, 0x0b, 0xc8,
	0xe2, 0xc9, 0x3a, 0xda, 0x7b, 0x74, 0x6c, 0xe5,
	0xa9, 0x77, 0xdc, 0xc3, 0x2a, 0x2b, 0xf3, 0xe0,
	0xa1, 0x0f, 0x18, 0x89, 0x4c, 0xde, 0xab, 0x1f,
	0xe9, 0x01, 0xd8, 0x13, 0x41, 0xae, 0x17, 0x91,
	0xc5, 0x92, 0x75, 0xb4, 0xf6, 0xe8, 0xd9, 0xcb,
	0x52, 0xef, 0xb9, 0x86, 0x54, 0x57, 0xe7, 0xc1,
	0x42, 0x1e, 0x31, 0x12, 0x99, 0xbd, 0x56, 0x3f,
	0xd2, 0x03, 0xb0, 0x26, 0x83, 0x5c, 0x2f, 0x23,
	0x8b, 0x24, 0xeb, 0x69, 0xed, 0xd1, 0xb3, 0x96,
	0xa5, 0xdf, 0x73, 0x0c, 0xa8, 0xaf, 0xcf, 0x82,
	0x84, 0x3c, 0x62, 0x25, 0x33, 0x7a, 0xac, 0x7f,
	0xa4, 0x07, 0x60, 0x4d, 0x06, 0xb8, 0x5e, 0x47,
	0x16, 0x49, 0xd6, 0xd3, 0xdb, 0xa3, 0x67, 0x2d,
	0x4b, 0xbe, 0xe6, 0x19, 0x51, 0x5f, 0x9f, 0x05,
	0x08, 0x78, 0xc4, 0x4a, 0x66, 0xf5, 0x58, 0xff
};

#else

/*
 * Texas Instruments' PN9 scrambler
 */
#define WHITENING_LEN   256
static const uint8_t whitening[WHITENING_LEN] = {
	0xff, 0xe1, 0x1d, 0x9a, 0xed, 0x85, 0x33, 0x24,
	0xea, 0x7a, 0xd2, 0x39, 0x70, 0x97, 0x57, 0x0a,
	0x54, 0x7d, 0x2d, 0xd8, 0x6d, 0x0d, 0xba, 0x8f,
	0x67, 0x59, 0xc7, 0xa2, 0xbf, 0x34, 0xca, 0x18,
	0x30, 0x53, 0x93, 0xdf, 0x92, 0xec, 0xa7, 0x15,
	0x8a, 0xdc, 0xf4, 0x86, 0x55, 0x4e, 0x18, 0x21,
	0x40, 0xc4, 0xc4, 0xd5, 0xc6, 0x91, 0x8a, 0xcd,
	0xe7, 0xd1, 0x4e, 0x09, 0x32, 0x17, 0xdf, 0x83,
	0xff, 0xf0, 0x0e, 0xcd, 0xf6, 0xc2, 0x19, 0x12,
	0x75, 0x3d, 0xe9, 0x1c, 0xb8, 0xcb, 0x2b, 0x05,
	0xaa, 0xbe, 0x16, 0xec, 0xb6, 0x06, 0xdd, 0xc7,
	0xb3, 0xac, 0x63, 0xd1, 0x5f, 0x1a, 0x65, 0x0c,
	0x98, 0xa9, 0xc9, 0x6f, 0x49, 0xf6, 0xd3, 0x0a,
	0x45, 0x6e, 0x7a, 0xc3, 0x2a, 0x27, 0x8c, 0x10,
	0x20, 0x62, 0xe2, 0x6a, 0xe3, 0x48, 0xc5, 0xe6,
	0xf3, 0x68, 0xa7, 0x04, 0x99, 0x8b, 0xef, 0xc1,
	0x7f, 0x78, 0x87, 0x66, 0x7b, 0xe1, 0x0c, 0x89,
	0xba, 0x9e, 0x74, 0x0e, 0xdc, 0xe5, 0x95, 0x02,
	0x55, 0x5f, 0x0b, 0x76, 0x5b, 0x83, 0xee, 0xe3,
	0x59, 0xd6, 0xb1, 0xe8, 0x2f, 0x8d, 0x32, 0x06,
	0xcc, 0xd4, 0xe4, 0xb7, 0x24, 0xfb, 0x69, 0x85,
	0x22, 0x37, 0xbd, 0x61, 0x95, 0x13, 0x46, 0x08,
	0x10, 0x31, 0x71, 0xb5, 0x71, 0xa4, 0x62, 0xf3,
	0x79, 0xb4, 0x53, 0x82, 0xcc, 0xc5, 0xf7, 0xe0,
	0x3f, 0xbc, 0x43, 0xb3, 0xbd, 0x70, 0x86, 0x44,
	0x5d, 0x4f, 0x3a, 0x07, 0xee, 0xf2, 0x4a, 0x81,
	0xaa, 0xaf, 0x05, 0xbb, 0xad, 0x41, 0xf7, 0xf1,
	0x2c, 0xeb, 0x58, 0xf4, 0x97, 0x46, 0x19, 0x03,
	0x66, 0x6a, 0xf2, 0x5b, 0x92, 0xfd, 0xb4, 0x42,
	0x91, 0x9b, 0xde, 0xb0, 0xca, 0x09, 0x23, 0x04,
	0x88, 0x98, 0xb8, 0xda, 0x38, 0x52, 0xb1, 0xf9,
	0x3c, 0xda, 0x29, 0x41, 0xe6, 0xe2, 0x7b, 0xf0,
	// 0x1f, 0xde // Two "extra" ones for CRC-16
};
#endif


/*
 * Default configuration for the PHY layer
 */
SkyPHYConfig phy_defaults = {
	.enable_scrambler = 1,
	.enable_rs = 1,
};

SkyPHYConfig* conf = &phy_defaults;


/** Decode a received frame */
int sky_fec_decode(SkyRadioFrame *frame, SkyDiagnostics *diag)
{
	int ret = SKY_RET_OK;

	if (conf->enable_scrambler) {
		/*
		 * Remove scrambler/whitening
		 */
		for (unsigned int i = 0; i < frame->length; i++){
			frame->raw[i] ^= whitening[i & WHITENING_LEN]; //Note: this '&' is an effective modulo, and a lot faster than actual modulo on Cortex-M0
		}
	}

	if (conf->enable_rs) {
		if (frame->length < RS_PARITYS || frame->length > (RS_MSGLEN + RS_PARITYS)){
			return SKY_RET_RS_INVALID_LENGTH;
		}

		/*
		 * Decode Reed-Solomon FEC
		 *
		 * Simply decode in place in the original buffer
	 	 * and pass it to the next layer together with
	 	 * the original metadata. */
		if ((ret = decode_rs_8(frame->raw, NULL, 0, RS_MSGLEN + RS_PARITYS - frame->length)) < 0) {
			diag->rx_fec_fail++;
			SKY_PRINTF(SKY_DIAG_FEC, "\x1B[31m" "FEC failed" "\x1B[0m\n")
			return SKY_RET_RS_FAILED; /* Reed-Solomon decode failed */
		}

		// Frame is now "shorter"
		frame->length -= RS_PARITYS;

		if (ret > 0){
			SKY_PRINTF(SKY_DIAG_FEC, "\x1B[33m" "FEC corrected %d bytes" "\x1B[0m\n", ret)
		}

		// Update FEC Telemetry
		diag->rx_fec_ok++;
		diag->rx_fec_errs += ret;
		diag->rx_fec_octs += frame->length + RS_PARITYS;

		ret = SKY_RET_OK;
	}

	return ret;
}


/** Encode a frame to transmit */
int sky_fec_encode(SkyRadioFrame *frame)
{
	if (conf->enable_rs) {
		/*
		 * Calculate Reed-Solomon parity bytes
		 */
		encode_rs_8(frame->raw, &frame->raw[frame->length], RS_MSGLEN - frame->length);
		frame->length += RS_PARITYS;
	}

	if (conf->enable_scrambler) {
		/*
		 * Apply data whitening
		 */
		for (unsigned int i = 0; i < frame->length; i++){
			frame->raw[i] ^= whitening[i & WHITENING_LEN]; //Note: this & is an effective modulo, and a lot faster than actual modulo on Cortex-M0
		}
	}

	return SKY_RET_OK;
}
