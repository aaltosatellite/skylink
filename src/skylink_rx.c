//
// Created by elmore on 29.10.2021.
//

#include "skylink/skylink.h"
#include "skylink/conf.h"
#include "skylink/fec.h"
#include "skylink/arq_ring.h"
#include "skylink/frame.h"
#include "skylink/mac.h"
#include "skylink/hmac.h"
#include "skylink/utilities.h"

static void sky_rx_process_extensions(SkyHandle self, const SkyRadioFrame* frame, uint8_t this_type);
static void sky_rx_process_ext_mac_reset(SkyHandle self, const SkyPacketExtension* ext);
static void sky_rx_process_ext_mac_control(SkyHandle self, const SkyRadioFrame* frame, const SkyPacketExtension* ext);
static void sky_rx_process_ext_arq_request(SkyHandle self, const SkyPacketExtension* ext, int vc);
static void sky_rx_process_ext_arq_reset(SkyHandle self, const SkyPacketExtension* ext, int vc);
static void sky_rx_process_ext_hmac_sequence_reset(SkyHandle self, const SkyPacketExtension* ext, int vc);
static int sky_rx_1(SkyHandle self, SkyRadioFrame* frame);







int sky_rx(SkyHandle self, SkyRadioFrame* frame, int contains_golay) {
	int ret = 0;
	if(frame->length < SKY_ENCODED_FRAME_MIN_LENGTH)
		return SKY_RET_INVALID_ENCODED_LENGTH;

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
	if(frame->ext_length > (frame->length - EXTENSION_START_IDX)){
		return SKY_RET_INVALID_EXT_LENGTH;
	}


	const SkyVCConfig* vc_conf = &self->conf->vc[frame->vc];

	// This extension has to be checked here. Otherwise, if both peers use incorrect hmac-sequencing, we would be in lock state.
	sky_rx_process_extensions(self, frame, EXTENSION_HMAC_SEQUENCE_RESET);

	// The virtual channel necessitates auth.
	if (vc_conf->require_authentication){
		if ((frame->flags & SKY_FLAG_AUTHENTICATED) == 0){
			self->hmac->vc_enfocement_need[frame->vc] = 1;
			return SKY_RET_AUTH_MISSING;
		}
		int ret = sky_hmac_check_authentication(self, frame);
		if (ret < 0){
			if(ret == SKY_RET_EXCESSIVE_HMAC_JUMP){
				self->hmac->vc_enfocement_need[frame->vc] = 1;
			}
			return ret;
		}
	}

	// The virtual channel does not require auth, but it is there. Just remove the hash.
	if (vc_conf->require_authentication == 0) {
		if ((frame->flags & SKY_FLAG_AUTHENTICATED) != 0)
			sky_hmac_remove_hash(frame);
	}

	// Check the rest of extension types.
	sky_rx_process_extensions(self, frame, EXTENSION_MAC_TDD_CONTROL);
	sky_rx_process_extensions(self, frame, EXTENSION_MAC_PARAMETERS);
	sky_rx_process_extensions(self, frame, EXTENSION_ARQ_REQUEST);
	sky_rx_process_extensions(self, frame, EXTENSION_ARQ_RESET);

	// Even payloadless packets contain ARQ sequence. Here we can deduce if we have missed some.
	if (vc_conf->arq_on) {
		if (frame->arq_sequence != self->arrayBuffers[frame->vc]->primaryRcvRing->head_sequence){
			self->arrayBuffers[frame->vc]->resend_request_need = 1;
		}
	}

	// If there is no payload, we can return now.
	if((frame->flags & SKY_FLAG_HAS_PAYLOAD) == 0){
		return 0;
	}

	int pl_length = frame->length - (EXTENSION_START_IDX + frame->ext_length);
	uint8_t* pl_start = frame->raw + EXTENSION_START_IDX + frame->ext_length;

	if (vc_conf->arq_on == 0){
		if((frame->flags & SKY_FLAG_ARQ_ON) == 0) {
			/* ARQ is configured off, and frame does not have ARQ either sequence. All is fine. */
			skyArray_push_rx_packet_monotonic(self->arrayBuffers[frame->vc], pl_start, pl_length);
		}
		else {
			/* ARQ is configured off, but frame has ARQ sequence. What to do? */
			skyArray_push_rx_packet_monotonic(self->arrayBuffers[frame->vc], pl_start, pl_length);
		}

	}


	if (vc_conf->arq_on) {
		if ((frame->flags & SKY_FLAG_ARQ_ON) == 0) {
			/* ARQ is configured on, but frame does not have ARQ sequence. Toggle the need for ARQ state enforcement. */
			self->arrayBuffers[frame->vc]->state_enforcement_need = 1;
			return SKY_RET_NO_MAC_SEQUENCE;
		}
		else {
			/* ARQ is configured on, and frame has ARQ too. All is fine. */
			int r = skyArray_push_rx_packet(self->arrayBuffers[frame->vc], pl_start, pl_length, frame->arq_sequence);
			if (r == RING_RET_INVALID_SEQUENCE){
				/* Observe: If the received arq-sequence is past our horizon, we shall jump aboard the sequencing here,
				 * instead of waiting for a sequence enforcement by the sender. (See comments in sky_rx_process_ext_arq_req) †
				 */
				skyArray_set_receive_sequence(self->arrayBuffers[frame->vc], frame->arq_sequence, 0);
				skyArray_push_rx_packet(self->arrayBuffers[frame->vc], pl_start, pl_length, frame->arq_sequence);
				self->diag->rx_arq_resets++;
			}
		}
	}

	//todo: log behavior based on r.
	return 0;
}




static void sky_rx_process_extensions(SkyHandle self, const SkyRadioFrame* frame, uint8_t this_type){
	if((frame->ext_length + EXTENSION_START_IDX) > frame->length) {
		return; //todo error: too short packet.
	}
	if(frame->ext_length <= 1){
		return; //no extensions.
	}

	unsigned int cursor = 0;
	while (cursor < frame->ext_length) {

		SkyPacketExtension* ext = (SkyPacketExtension*)&frame->raw[EXTENSION_START_IDX + cursor]; //Magic happens here.
		if (cursor + ext->length >= frame->length)
			return;
		if(ext->length == 0){
			return;
		}
		cursor += ext->length;


		if (this_type != ext->type)
			continue;

		switch(ext->type) {
		case EXTENSION_MAC_PARAMETERS:
			sky_rx_process_ext_mac_reset(self, ext);
			break;

		case EXTENSION_MAC_TDD_CONTROL:
			sky_rx_process_ext_mac_control(self, frame, ext);
			break;

 		case EXTENSION_ARQ_RESET:
			sky_rx_process_ext_arq_reset(self, ext, frame->vc);
			break;

		case EXTENSION_ARQ_REQUEST:
			sky_rx_process_ext_arq_request(self, ext, frame->vc);
			break;

		case EXTENSION_HMAC_SEQUENCE_RESET:
			sky_rx_process_ext_hmac_sequence_reset(self, ext, frame->vc);
			break;

		default:
			//SKY_PRINTF("Unknwon extension header type %d", ext->type);
			break;
		}

	}
}



static void sky_rx_process_ext_mac_reset(SkyHandle self, const SkyPacketExtension* ext) {
	if (ext->length != (sizeof(ExtTDDParams)+1)){
		return;
	}

	if(!mac_valid_window_length(&self->conf->mac, ext->TDDParams.window_size)) {
		return;
	}
	if(!mac_valid_gap_length(&self->conf->mac, ext->TDDParams.gap_size)){
		return;
	}
	mac_set_gap_constant(self->mac, ext->TDDParams.gap_size);
	mac_set_my_window_length(self->mac, ext->TDDParams.window_size);
	mac_set_peer_window_length(self->mac, ext->TDDParams.window_size);
}


static void sky_rx_process_ext_mac_control(SkyHandle self, const SkyRadioFrame* frame, const SkyPacketExtension* ext) {
	if (ext->length != (sizeof(ExtTDDControl)+1)){
		return;
	}

	// Update MAC status
	if ((frame->flags & SKY_FLAG_AUTHENTICATED) == 0)
		return;

	if (self->conf->mac.unauthenticated_mac_updates) {
		mac_update_belief(self->mac, &self->conf->mac, frame->rx_time_ms, ext->TDDControl.window, ext->TDDControl.remaining);
	}

}


static void sky_rx_process_ext_arq_request(SkyHandle self, const SkyPacketExtension* ext, int vc) {
	if (ext->length != (sizeof(ExtARQReq)+1)){
		return;
	}

	uint16_t mask = sky_ntoh16(ext->ARQReq.mask);
	int r = skyArray_schedule_resend(self->arrayBuffers[vc], ext->ARQReq.sequence);
	if(r < 0){
		return;
	}

	for (int i = 0; i < 16; ++i) {
		if(mask & (1<<i)){
			continue;
		}
		uint8_t sequence = (uint8_t)sequence_wrap(ext->ARQReq.sequence + i + 1);
		if(sequence == self->arrayBuffers[vc]->primarySendRing->tx_sequence){
			return;
		}
		r = skyArray_schedule_resend(self->arrayBuffers[vc], sequence);
		if(r < 0){
			/* †
			 * No. When unable to resend sequence requested, we send nothing. Was sich überhaupt sagen lässt, lässt
			 * sich klar sagen; und wovon man nicht reden kann, darüber muss man schweigen.
			 */
			//self->arrayBuffers[vc]->state_enforcement_need = 1;
			//return;
		}
	}
}



static void sky_rx_process_ext_arq_reset(SkyHandle self, const SkyPacketExtension* ext, int vc) {
	if (ext->length != (sizeof(ExtARQReset)+1)){
		return;
	}

	self->conf->vc[vc].arq_on = (ext->ARQReset.toggle > 0);
	/* This extension only toggles ARQ on or off. The ONLY way misaligned sequences realign,
	 * is through out-of-horizon reception */
	//if(ext->ArqSeqReset.toggle){
	//	skyArray_set_receive_sequence(self->arrayBuffers[vc], ext->ArqSeqReset.enforced_sequence, 0); //sequence gets wrapped anyway..
	//}
}



static void sky_rx_process_ext_hmac_sequence_reset(SkyHandle self, const SkyPacketExtension* ext, int vc) {
	if (ext->length !=(sizeof(ExtHMACSequenceReset) +1)){
		return;
	}

	//todo: should include toggle!
	//self->conf->vc->require_authentication = 1;
	uint16_t new_sequence = sky_ntoh16(ext->HMACSequenceReset.sequence);
	if(new_sequence == HMAC_NO_SEQUENCE){
		self->conf->vc[vc].require_authentication = 0;
	} else {
		self->hmac->sequence_tx[vc] = wrap_hmac_sequence(new_sequence);
	}
}
