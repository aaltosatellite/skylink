//
// Created by elmore on 29.10.2021.
//

#include "skylink_rx.h"


void sky_rx_process_extensions(SkyHandle self, SkyRadioFrame* frame, uint8_t this_type);
void sky_rx_process_ext_mac_reset(SkyHandle self, ExtMACSpec macSpec);
void sky_rx_process_ext_arq_sequence_reset(SkyHandle self, ExtArqSeqReset arqSeqReset, int vc);
void sky_rx_process_ext_arq_req(SkyHandle self, ExtArqReq arqReq, int vc);
void sky_rx_process_ext_hmac_tx_seq_reset(SkyHandle self, ExtHMACTxReset hmacTxReset, int vc);











int sky_rx_0(SkyHandle self, SkyRadioFrame* frame){
	// Read Golay decoded len
	uint32_t coded_len = (frame->raw[0] << 16) | (frame->raw[1] << 8) | frame->raw[2];

	int ret = decode_golay24(&coded_len);
	if (ret < 0) {
		// TODO: log the number of corrected bits?
		self->diag->rx_fec_fail++;
		return SKY_RET_GOLAY_FAILED;
	}

	if ((coded_len & 0xF00) != (SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED)){
		return -1;
	}

	frame->length = coded_len & SKY_GOLAY_PAYLOAD_LENGTH_MASK;

	// Remove the length header from the rest of the data
	for (unsigned int i = 0; i < frame->length; i++){
		frame->raw[i] = frame->raw[i + 3];
	}

	// Decode FEC
	if ((ret = sky_fec_decode(frame, self->diag)) < 0){
		return ret;
	}

	return sky_rx_1(self, frame);
}



int sky_rx_1(SkyHandle self, SkyRadioFrame *frame){
	int ret;

	// Decode packet
	if((ret = decode_skylink_packet(frame)) < 0){
		return ret;
	}

	// This extension has to be checked here. Otherwise, if both peers use incorrect hmac-sequencing, we would be in lock state.
	sky_rx_process_extensions(self, frame, EXTENSION_HMAC_INVALID_SEQ);


	//todo: set metadata on auth and such negative on frame init.

	// If the virtual channel necessitates auth, but the frame isn't, return error.
	if(sky_hmac_vc_demands_auth(self, frame->vc) && (!frame->hmac_on)){
		return SKY_RET_AUTH_MISSING;
	}

	// Check authentication code if the frame claims it is authenticated.
	if (frame->hmac_on) {
		ret = sky_hmac_check_authentication(self, frame);
		if (ret < 0){
			if(ret == SKY_RET_EXCESSIVE_HMAC_JUMP){
				self->hmac->vc_enfocement_need[frame->vc] = 1;
			}
			return ret;
		}
	}


	// Update MAC status
	if((frame->hmac_on) || self->conf->mac.unauthenticated_mac_updates){
		mac_update_belief(self->mac, &self->conf->mac, frame->metadata.rx_time_ms, frame->mac_length, frame->mac_remaining);
	}

	// Check the rest of extension types.
	sky_rx_process_extensions(self, frame, EXTENSION_MAC_PARAMETERS);
	sky_rx_process_extensions(self, frame, EXTENSION_ARQ_RESEND_REQ);
	sky_rx_process_extensions(self, frame, EXTENSION_ARQ_SEQ_RESET);


	if(!self->conf->vc[frame->vc].arq_on){
		if(!(frame->arq_on)){
			/* ARQ is configured off, and frame does not have ARQ either sequence. All is fine. */
			skyArray_push_rx_packet_monotonic(self->arrayBuffers[frame->vc], frame->metadata.payload_read_start, frame->metadata.payload_read_length);
		}
		if(frame->arq_on){
			/* ARQ is configured off, but frame has ARQ sequence. What to do? */

		}

	}


	if(self->conf->vc[frame->vc].arq_on){
		if(!frame->arq_on){
			/* ARQ is configured on, but frame does not have ARQ sequence. Toggle the need for ARQ state enforcement. */
			self->arrayBuffers[frame->vc]->state_enforcement_need = 1;
			return SKY_RET_NO_MAC_SEQUENCE;
		}
		if(frame->arq_on) {
			/* ARQ is configured on, and frame has ARQ too. All is fine. */
			int r = skyArray_push_rx_packet(self->arrayBuffers[frame->vc], frame->metadata.payload_read_start, frame->metadata.payload_read_length, frame->arq_sequence);
			if (r == RING_RET_INVALID_SEQUENCE){
				/* Observe: If the received arq-sequence is past our horizon, we shall jump aboard the sequencing here,
				 * instead of waiting for a sequence enforcement by the sender. (See comments in sky_rx_process_ext_arq_req) †
				 */
				skyArray_set_receive_sequence(self->arrayBuffers[frame->vc], frame->arq_sequence, 0);
				skyArray_push_rx_packet(self->arrayBuffers[frame->vc], frame->metadata.payload_read_start, frame->metadata.payload_read_length, frame->arq_sequence);
				self->diag->rx_arq_resets++;
			}
		}
	}


	//todo: log behavior based on r.
	return 0;
}




void sky_rx_process_extensions(SkyHandle self, SkyRadioFrame* frame, uint8_t this_type){
	for (int i = 0; i < frame->n_extensions; ++i) {
		SkyPacketExtension* ext = &frame->extensions[i];
		if((ext->type == EXTENSION_MAC_PARAMETERS) && (ext->type == this_type)){
			sky_rx_process_ext_mac_reset(self, ext->ext_union.MACSpec);
		}
		if((ext->type == EXTENSION_ARQ_SEQ_RESET) && (ext->type == this_type)){
			sky_rx_process_ext_arq_sequence_reset(self, ext->ext_union.ArqSeqReset, frame->vc);
		}
		if((ext->type == EXTENSION_ARQ_RESEND_REQ) && (ext->type == this_type)){
			sky_rx_process_ext_arq_req(self, ext->ext_union.ArqReq, frame->vc);
		}
		if((ext->type == EXTENSION_HMAC_INVALID_SEQ) && (ext->type == this_type)){
			sky_rx_process_ext_hmac_tx_seq_reset(self, ext->ext_union.HMACTxReset, frame->vc);
		}
	}
}


void sky_rx_process_ext_mac_reset(SkyHandle self, ExtMACSpec macSpec){
	if(!mac_valid_window_length(&self->conf->mac, macSpec.window_size)){
		return;
	}
	if(!mac_valid_gap_length(&self->conf->mac, macSpec.gap_size)){
		return;
	}
	mac_set_gap_constant(self->mac, macSpec.gap_size);
	mac_set_my_window_length(self->mac, macSpec.window_size);
	mac_set_peer_window_length(self->mac, macSpec.window_size);
}


void sky_rx_process_ext_arq_sequence_reset(SkyHandle self, ExtArqSeqReset arqSeqReset, int vc){
	self->conf->vc[vc].arq_on = (arqSeqReset.toggle > 0);
	/* This extension only toggles ARQ on or off. The ONLY way misaligned sequences realign,
	 * is through out-of-horizon reception */
	//if(arqSeqReset.toggle){
	//	skyArray_set_receive_sequence(self->arrayBuffers[vc], arqSeqReset.enforced_sequence, 0); //sequence gets wrapped anyway..
	//}
}


void sky_rx_process_ext_arq_req(SkyHandle self, ExtArqReq arqReq, int vc){
	uint16_t mask = arqReq.mask1 + (arqReq.mask2 << 8);
	for (int i = 0; i < 16; ++i) {
		if(mask & (1<<i)){
			continue;
		}
		uint8_t sequence = (uint8_t) positive_modulo(arqReq.sequence + i, ARQ_SEQUENCE_MODULO);
		int r = skyArray_schedule_resend(self->arrayBuffers[vc], sequence);
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


void sky_rx_process_ext_hmac_tx_seq_reset(SkyHandle self, ExtHMACTxReset hmacTxReset, int vc){
	uint16_t new_sequence = hmacTxReset.correct_tx_sequence;
	self->hmac->sequence_tx[vc] = new_sequence;
}




