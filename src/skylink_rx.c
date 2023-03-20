#include "skylink/skylink.h"
#include "skylink/fec.h"
#include "skylink/reliable_vc.h"
#include "skylink/frame.h"
#include "skylink/mac.h"
#include "skylink/hmac.h"
#include "skylink/utilities.h"



static void sky_rx_process_ext_mac_control(SkyHandle self, const SkyRadioFrame* frame, const SkyPacketExtension* ext);


int sky_rx_with_golay(SkyHandle self, SkyRadioFrame* frame) {

	// Read Golay coded length
	uint32_t coded_len = (frame->raw[0] << 16) | (frame->raw[1] << 8) | frame->raw[2];
	int ret = decode_golay24(&coded_len);
	if (ret < 0) {
		SKY_PRINTF(SKY_DIAG_FEC, "\x1B[31m" "Golay failed" "\x1B[0m\n");
		self->diag->rx_fec_fail++;
		return SKY_RET_GOLAY_FAILED;
	}

	frame->length = (int32_t)coded_len & SKY_GOLAY_PAYLOAD_LENGTH_MASK;
	if (frame->length > sizeof(frame->raw))
		return SKY_RET_INVALID_ENCODED_LENGTH;

	// Remove the length header from the rest of the data
	for (unsigned int i = 0; i < frame->length; i++)
		frame->raw[i] = frame->raw[i + 3];

	return sky_rx_with_fec(self, frame);
}


int sky_rx_with_fec(SkyHandle self, SkyRadioFrame* frame) {

	self->diag->rx_frames++;

	if (frame->length < EXTENSION_START_IDX + RS_PARITYS) {
		SKY_PRINTF(SKY_DIAG_FRAMES, "\x1B[31m" "Too short frame" "\x1B[0m\n");
		return SKY_RET_INVALID_ENCODED_LENGTH;
	}

	// Decode FEC
	int ret = sky_fec_decode(frame, self->diag);
	if (ret < 0)
		return ret;

	return sky_rx(self, frame);
}


int sky_rx(SkyHandle self, SkyRadioFrame* frame) {
	
	int ret;

	// Some error checks
	if(frame->length < SKY_PLAIN_FRAME_MIN_LENGTH){
		SKY_PRINTF(SKY_DIAG_FRAMES, "\x1B[31m" "Too short frame" "\x1B[0m\n");
		return SKY_RET_INVALID_PLAIN_LENGTH;
	}

	// Validate static header
	if (frame->start_byte != SKYLINK_START_BYTE)
		return SKY_RET_INVALID_START_BYTE;
	
	if (frame->vc >= SKY_NUM_VIRTUAL_CHANNELS)
		return SKY_RET_INVALID_VC;
	
	if (frame->ext_length + EXTENSION_START_IDX > (int)frame->length)
		return SKY_RET_INVALID_EXT_LENGTH;
	
	if (memcmp(frame->identity, self->conf->identity, SKY_IDENTITY_LEN) == 0)
		return SKY_RET_OWN_TRANSMISSION;


	// Parse and validate all extension headers
	SkyParsedExtensions exts;
	if ((ret = sky_frame_parse_extension_headers(frame, &exts)) < 0)
		return ret;

	// Check the authentication/HMAC if the virtual channel necessitates it.
	if ((ret = sky_hmac_check_authentication(self, frame, exts.hmac_reset)) < 0)
		return ret;

	// Update MAC/TDD state, and check for MAC/TDD handshake extension
	if (exts.mac_tdd != NULL)
		sky_rx_process_ext_mac_control(self, frame, exts.mac_tdd);

	// Extract the frame payload
	const uint8_t* payload = frame->raw + EXTENSION_START_IDX + frame->ext_length;
	int payload_len = (int) frame->length - (EXTENSION_START_IDX + frame->ext_length);
	if ((frame->flags & SKY_FLAG_HAS_PAYLOAD) == 0)
		payload_len = -1;
	
	ret = sky_vc_process_content(self->virtual_channels[frame->vc], payload, payload_len, &exts, sky_get_tick_time());

	return ret;
}


static void sky_rx_process_ext_mac_control(SkyHandle self, const SkyRadioFrame* frame, const SkyPacketExtension* ext) {
	if (ext->length != sizeof(ExtTDDControl) + 1){
		return;
	}
	if ((!self->conf->mac.unauthenticated_mac_updates) && (!(frame->flags & SKY_FLAG_AUTHENTICATED)) ) {
		return;
	}
	uint16_t w = sky_ntoh16(ext->TDDControl.window);
	uint16_t r = sky_ntoh16(ext->TDDControl.remaining);
	SKY_PRINTF(SKY_DIAG_MAC | SKY_DIAG_DEBUG, "MAC Updated: Window length %d, window remaining %d\n", w, r);
	mac_update_belief(self->mac, sky_get_tick_time(), frame->rx_time_ticks, w, r);
}
