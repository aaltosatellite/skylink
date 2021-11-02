//
// Created by elmore on 29.10.2021.
//

#include "skylink_rx.h"


static void sky_rx_process_extensions(SkyHandle self, RCVFrame* frame, uint8_t this_type);
static void sky_rx_process_ext_mac_reset(SkyHandle self, ExtMACSpec macSpec);
static void sky_rx_process_ext_arq_sequence_reset(SkyHandle self, ExtArqSeqReset arqSeqReset, int vc);
static void sky_rx_process_ext_arq_req(SkyHandle self, ExtArqReq arqReq, int vc);
static void sky_rx_process_ext_hmac_enforcement(SkyHandle self, ExtHMACTxReset hmacTxReset, int vc);
static int sky_rx_1(SkyHandle self, RCVFrame* frame);








int sky_rx_0(SkyHandle self, RCVFrame* frame, int contains_golay){
	int ret = 0;
	RadioFrame2* radioFrame = &frame->radioFrame;
	if(contains_golay) {
		// Read Golay decoded len
		uint32_t coded_len = (radioFrame->raw[0] << 16) | (radioFrame->raw[1] << 8) | radioFrame->raw[2];

		ret = decode_golay24(&coded_len);
		if (ret < 0) {
			// TODO: log the number of corrected bits?
			self->diag->rx_fec_fail++;
			return SKY_RET_GOLAY_FAILED;
		}

		if ((coded_len & 0xF00) != (SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED)) {
			return -1;
		}

		radioFrame->length = (int32_t)coded_len & SKY_GOLAY_PAYLOAD_LENGTH_MASK;

		// Remove the length header from the rest of the data
		for (int i = 0; i < radioFrame->length; i++) {
			radioFrame->raw[i] = radioFrame->raw[i + 3];
		}
	}

	// Decode FEC
	if ((ret = sky_fec_decode(radioFrame, self->diag)) < 0){
		return ret;
	}

	//initialize_for_decoding(frame);

	return sky_rx_1(self, frame);
}



static int sky_rx_1(SkyHandle self, RCVFrame* frame){
	int ret;
	RadioFrame2* radioFrame = &frame->radioFrame;

	// Decode packet
	//if((ret = decode_skylink_packet(frame)) < 0){
	//	return ret;
	//}
	//todo: error checks?

	// This extension has to be checked here. Otherwise, if both peers use incorrect hmac-sequencing, we would be in lock state.
	sky_rx_process_extensions(self, frame, EXTENSION_HMAC_ENFORCEMENT);



	// If the virtual channel necessitates auth, but the frame isn't, return error.
	if( (self->conf->vc[radioFrame->vc].require_authentication > 0)  && (!(radioFrame->flags & SKY_FLAG_AUTHENTICATED))){
		self->hmac->vc_enfocement_need[radioFrame->vc] = 1;
		return SKY_RET_AUTH_MISSING;
	}

	// Check authentication code if the frame claims it is authenticated.
	if (radioFrame->flags & SKY_FLAG_AUTHENTICATED) {
		ret = sky_hmac_check_authentication(self, frame);
		if (ret < 0){
			if(ret == SKY_RET_EXCESSIVE_HMAC_JUMP){
				self->hmac->vc_enfocement_need[radioFrame->vc] = 1;
			}
			return ret;
		}
	}


	// Update MAC status
	if((radioFrame->flags & SKY_FLAG_AUTHENTICATED) || self->conf->mac.unauthenticated_mac_updates){
		mac_update_belief(self->mac, &self->conf->mac, frame->rx_time_ms, frame->radioFrame.mac_window, frame->radioFrame.mac_remaining);
	}

	// Check the rest of extension types.
	sky_rx_process_extensions(self, frame, EXTENSION_MAC_PARAMETERS);
	sky_rx_process_extensions(self, frame, EXTENSION_ARQ_RESEND_REQ);
	sky_rx_process_extensions(self, frame, EXTENSION_ARQ_SEQ_RESET);


	if(!(frame->radioFrame.flags & SKY_FLAG_HAS_PAYLOAD)){
		return 0;
	}

	if(!self->conf->vc[frame->radioFrame.vc].arq_on){
		if(!(frame->radioFrame.flags & SKY_FLAG_ARQ_ON)){
			/* ARQ is configured off, and frame does not have ARQ either sequence. All is fine. */
			int pl_length = frame->radioFrame.length - (I_PK_EXTENSIONS + frame->radioFrame.ext_length);
			uint8_t* pl_start = frame->radioFrame.raw + frame->radioFrame.ext_length;
			skyArray_push_rx_packet_monotonic(self->arrayBuffers[frame->radioFrame.vc], pl_start, pl_length);
		}
		if(frame->radioFrame.flags & SKY_FLAG_ARQ_ON){
			/* ARQ is configured off, but frame has ARQ sequence. What to do? */

		}

	}


	if(self->conf->vc[radioFrame->vc].arq_on){
		if(!(radioFrame->flags & SKY_FLAG_ARQ_ON)){
			/* ARQ is configured on, but frame does not have ARQ sequence. Toggle the need for ARQ state enforcement. */
			self->arrayBuffers[radioFrame->vc]->state_enforcement_need = 1;
			return SKY_RET_NO_MAC_SEQUENCE;
		}
		if(radioFrame->flags & SKY_FLAG_ARQ_ON) {
			/* ARQ is configured on, and frame has ARQ too. All is fine. */
			int pl_length = frame->radioFrame.length - (I_PK_EXTENSIONS + frame->radioFrame.ext_length);
			uint8_t* pl_start = frame->radioFrame.raw + frame->radioFrame.ext_length;
			int r = skyArray_push_rx_packet(self->arrayBuffers[radioFrame->vc], pl_start, pl_length, radioFrame->arq_sequence);
			if (r == RING_RET_INVALID_SEQUENCE){
				/* Observe: If the received arq-sequence is past our horizon, we shall jump aboard the sequencing here,
				 * instead of waiting for a sequence enforcement by the sender. (See comments in sky_rx_process_ext_arq_req) †
				 */
				skyArray_set_receive_sequence(self->arrayBuffers[radioFrame->vc], radioFrame->arq_sequence, 0);
				skyArray_push_rx_packet(self->arrayBuffers[radioFrame->vc], pl_start, pl_length, radioFrame->arq_sequence);
				self->diag->rx_arq_resets++;
			}
		}
	}


	//todo: log behavior based on r.
	return 0;
}




static void sky_rx_process_extensions(SkyHandle self, RCVFrame* frame, uint8_t this_type){
	if((frame->radioFrame.ext_length + I_PK_EXTENSIONS) > frame->radioFrame.length){
		return; //todo error: too short packet.
	}
	if(frame->radioFrame.ext_length <= 1){
		return; //no extensions.
	}
	int cursor = I_PK_EXTENSIONS;
	while ((cursor - I_PK_EXTENSIONS) < frame->radioFrame.ext_length){
		SkyPacketExtension ext;
		int r = interpret_extension(&frame->radioFrame.raw[cursor], frame->radioFrame.length - cursor, &ext);
		if(r < 0){
			return;
		}
		if((ext.type == EXTENSION_MAC_PARAMETERS) && (ext.type == this_type)){
			sky_rx_process_ext_mac_reset(self, ext.ext_union.MACSpec);
		}
		if((ext.type == EXTENSION_ARQ_SEQ_RESET) && (ext.type == this_type)){
			sky_rx_process_ext_arq_sequence_reset(self, ext.ext_union.ArqSeqReset, frame->radioFrame.vc);
		}
		if((ext.type == EXTENSION_ARQ_RESEND_REQ) && (ext.type == this_type)){
			sky_rx_process_ext_arq_req(self, ext.ext_union.ArqReq, frame->radioFrame.vc);
		}
		if((ext.type == EXTENSION_HMAC_ENFORCEMENT) && (ext.type == this_type)){
			sky_rx_process_ext_hmac_enforcement(self, ext.ext_union.HMACTxReset, frame->radioFrame.vc);
		}
	}
}




static void sky_rx_process_ext_mac_reset(SkyHandle self, ExtMACSpec macSpec){
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


static void sky_rx_process_ext_arq_sequence_reset(SkyHandle self, ExtArqSeqReset arqSeqReset, int vc){
	self->conf->vc[vc].arq_on = (arqSeqReset.toggle > 0);
	/* This extension only toggles ARQ on or off. The ONLY way misaligned sequences realign,
	 * is through out-of-horizon reception */
	//if(arqSeqReset.toggle){
	//	skyArray_set_receive_sequence(self->arrayBuffers[vc], arqSeqReset.enforced_sequence, 0); //sequence gets wrapped anyway..
	//}
}


static void sky_rx_process_ext_arq_req(SkyHandle self, ExtArqReq arqReq, int vc){
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


static void sky_rx_process_ext_hmac_enforcement(SkyHandle self, ExtHMACTxReset hmacTxReset, int vc){
	//todo: should include toggle!
	//self->conf->vc->require_authentication = 1;
	uint16_t new_sequence = hmacTxReset.correct_tx_sequence;
	if(new_sequence == HMAC_NO_SEQUENCE){
		self->conf->vc[vc].require_authentication = 0;
	} else {
		self->hmac->sequence_tx[vc] = wrap_hmac_sequence(new_sequence);
	}
}




