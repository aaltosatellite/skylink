//
// Created by elmore on 29.10.2021.
//

#include "skylink/skylink.h"
#include "skylink/conf.h"
#include "skylink/fec.h"
#include "skylink/reliable_vc.h"
#include "skylink/frame.h"
#include "skylink/mac.h"
#include "skylink/hmac.h"
#include "skylink/utilities.h"

static void sky_rx_process_extensions(SkyHandle self, const SkyRadioFrame* frame, uint8_t this_type);
static void sky_rx_process_ext_mac_control(SkyHandle self, const SkyRadioFrame* frame, const SkyPacketExtension* ext);
static void sky_rx_process_ext_hmac_sequence_reset(SkyHandle self, const SkyPacketExtension* ext, int vc);
static int sky_rx_1(SkyHandle self, SkyRadioFrame* frame);





int sky_rx(SkyHandle self, SkyRadioFrame* frame, int contains_golay) {
	int ret;
	if(frame->length < SKY_ENCODED_FRAME_MIN_LENGTH) {
		return SKY_RET_INVALID_ENCODED_LENGTH;
	}

	if (contains_golay) {
		// Read Golay decoded len
		uint32_t coded_len = (frame->raw[0] << 16) | (frame->raw[1] << 8) | frame->raw[2];

		ret = decode_golay24(&coded_len);
		if (ret < 0) {
			// TODO: log the number of corrected bits?
			self->diag->rx_fec_fail++;
			return SKY_RET_GOLAY_FAILED;
		}

		if ((coded_len & 0xF00) != (SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED)) {
			return SKY_RET_GOLAY_MISCONFIGURED;
		}

		frame->length = (int32_t)coded_len & SKY_GOLAY_PAYLOAD_LENGTH_MASK;

		// Remove the length header from the rest of the data
		for (unsigned int i = 0; i < frame->length; i++)
			frame->raw[i] = frame->raw[i + 3];
	}

	self->diag->rx_frames++;

	// Decode FEC
	if ((ret = sky_fec_decode(frame, self->diag)) < 0){
		return ret;
	}

	return sky_rx_1(self, frame);
}



static int sky_rx_1(SkyHandle self, SkyRadioFrame* frame){
	// Some error checks
	if(frame->length < SKY_PLAIN_FRAME_MIN_LENGTH){
		return SKY_RET_INVALID_PLAIN_LENGTH;
	}
	if(frame->vc >= SKY_NUM_VIRTUAL_CHANNELS){
		return SKY_RET_INVALID_VC;
	}
	if((frame->ext_length + EXTENSION_START_IDX) > (int)frame->length){
		return SKY_RET_INVALID_EXT_LENGTH;
	}
	if(memcmp(frame->identity, self->conf->identity, SKY_IDENTITY_LEN) == 0){
		return SKY_RET_OWN_TRANSMISSION;
	}


	// This extension has to be checked here. Otherwise, if both peers use incorrect hmac-sequencing, we would be in lock state.
	sky_rx_process_extensions(self, frame, EXTENSION_HMAC_SEQUENCE_RESET);


	// Check the authentication/HMAC if the virtual channel necessitates it.
	int ret = sky_hmac_check_authentication(self, frame);
	if (ret != SKY_RET_OK)
		return ret;


	// Update MAC/TDD state, and check for MAC/TDD handshake extension
	sky_rx_process_extensions(self, frame, EXTENSION_MAC_TDD_CONTROL);


	SkyPacketExtension* ext_seq = sky_rx_get_extension(frame, EXTENSION_ARQ_SEQUENCE);
	SkyPacketExtension* ext_rrequest = sky_rx_get_extension(frame, EXTENSION_ARQ_REQUEST);
	SkyPacketExtension* ext_ctrl = sky_rx_get_extension(frame, EXTENSION_ARQ_CTRL);
	SkyPacketExtension* ext_handshake = sky_rx_get_extension(frame, EXTENSION_ARQ_HANDSHAKE);
	void* pl = frame->raw + EXTENSION_START_IDX + frame->ext_length;
	int len_pl = (int) frame->length - (EXTENSION_START_IDX + frame->ext_length);
	if(!(frame->flags & SKY_FLAG_HAS_PAYLOAD)){
		len_pl = -1;
	}
	sky_vc_process_content(self->virtual_channels[frame->vc], pl, len_pl, ext_seq, ext_ctrl, ext_handshake, ext_rrequest,
						   sky_get_tick_time());


	//todo: log behavior based on r.
	return SKY_RET_OK;
}








static void sky_rx_process_extensions(SkyHandle self, const SkyRadioFrame* frame, uint8_t this_type){
	SkyPacketExtension* ext = sky_rx_get_extension(frame, this_type);
	if(ext != NULL){
		switch(ext->type) {
			case EXTENSION_HMAC_SEQUENCE_RESET:
				sky_rx_process_ext_hmac_sequence_reset(self, ext, frame->vc);
				break;

			case EXTENSION_MAC_TDD_CONTROL:
				sky_rx_process_ext_mac_control(self, frame, ext);
				break;

			default:
				//SKY_PRINTF("Unknwon extension header type %d", ext->type);
				break;
		}
	}
}



static void sky_rx_process_ext_hmac_sequence_reset(SkyHandle self, const SkyPacketExtension* ext, int vc) {
	if (ext->length !=(sizeof(ExtHMACSequenceReset) +1)){
		return;
	}
	uint16_t new_sequence = sky_ntoh16(ext->HMACSequenceReset.sequence);
	if(new_sequence == HMAC_NO_SEQUENCE){
		self->conf->vc[vc].require_authentication = 0;
	} else {
		self->conf->vc[vc].require_authentication = 1;
		self->hmac->sequence_tx[vc] = wrap_hmac_sequence(new_sequence);
	}
}



static void sky_rx_process_ext_mac_control(SkyHandle self, const SkyRadioFrame* frame, const SkyPacketExtension* ext) {
	if (ext->length != (sizeof(ExtTDDControl)+1)){
		return;
	}
	if ((!self->conf->mac.unauthenticated_mac_updates) && (!(frame->flags & SKY_FLAG_AUTHENTICATED)) ) {
		return;
	}
	uint16_t w = sky_ntoh16(ext->TDDControl.window);
	uint16_t r = sky_ntoh16(ext->TDDControl.remaining);
	mac_update_belief(self->mac, &self->conf->mac, sky_get_tick_time() , w, r);
}
