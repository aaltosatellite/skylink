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


static void sky_rx_process_extensions(SkyHandle self, RCVFrame* frame, uint8_t this_type);
static void sky_rx_process_ext_mac_reset(SkyHandle self, ExtMACSpec macSpec);
static void sky_rx_process_ext_arq_sequence_reset(SkyHandle self, ExtArqSeqReset arqSeqReset, int vc);
static void sky_rx_process_ext_arq_req(SkyHandle self, ExtArqReq arqReq, int vc);
static void sky_rx_process_ext_hmac_enforcement(SkyHandle self, ExtHMACTxReset hmacTxReset, int vc);
static int sky_rx_1(SkyHandle self, RCVFrame* frame);








int sky_rx(SkyHandle self, RCVFrame* frame, int contains_golay){
	int ret = 0;
	if(frame->radioFrame.length < SKY_PLAIN_FRAME_MIN_LENGTH){
		return SKY_RET_INVALID_LENGTH;
	}
	RadioFrame* radioFrame = &frame->radioFrame;
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

	return sky_rx_1(self, frame);
}



static int sky_rx_1(SkyHandle self, RCVFrame* frame){
	RadioFrame* radioFrame = &frame->radioFrame;

	// Some error checks
	if(radioFrame->length < SKY_PLAIN_FRAME_MIN_LENGTH){
		return SKY_RET_INVALID_LENGTH;
	}
	if(radioFrame->vc >= SKY_NUM_VIRTUAL_CHANNELS){
		return SKY_RET_INVALID_VC;
	}
	if(radioFrame->ext_length > (radioFrame->length - EXTENSION_START_IDX)){
		return SKY_RET_INVALID_EXT_LENGTH;
	}

	// This extension has to be checked here. Otherwise, if both peers use incorrect hmac-sequencing, we would be in lock state.
	sky_rx_process_extensions(self, frame, EXTENSION_HMAC_ENFORCEMENT);

	// The virtual channel necessitates auth.
	if(self->conf->vc[radioFrame->vc].require_authentication > 0){
		if (!(radioFrame->flags & SKY_FLAG_AUTHENTICATED)){
			self->hmac->vc_enfocement_need[radioFrame->vc] = 1;
			return SKY_RET_AUTH_MISSING;
		}
		int ret = sky_hmac_check_authentication(self, frame);
		if (ret < 0){
			if(ret == SKY_RET_EXCESSIVE_HMAC_JUMP){
				self->hmac->vc_enfocement_need[radioFrame->vc] = 1;
			}
			return ret;
		}
	}

	// The virtual channel does not require auth, but it is there. Just remove the hash.
	if(self->conf->vc[radioFrame->vc].require_authentication == 0){
		if (radioFrame->flags & SKY_FLAG_AUTHENTICATED){
			sky_hmac_remove_hash(frame);
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

	// Even payloadless packets contain ARQ sequence. Here we can deduce if we have missed some.
	if(self->conf->vc[radioFrame->vc].arq_on){
		if(radioFrame->arq_sequence != self->arrayBuffers[radioFrame->vc]->primaryRcvRing->head_sequence){
			self->arrayBuffers[radioFrame->vc]->resend_request_need = 1;
		}
	}

	// If there is no payload, we can return now.
	if(!(frame->radioFrame.flags & SKY_FLAG_HAS_PAYLOAD)){
		return 0;
	}


	if(!self->conf->vc[frame->radioFrame.vc].arq_on){
		if(!(frame->radioFrame.flags & SKY_FLAG_ARQ_ON)){
			/* ARQ is configured off, and frame does not have ARQ either sequence. All is fine. */
			int pl_length = frame->radioFrame.length - (EXTENSION_START_IDX + frame->radioFrame.ext_length);
			uint8_t* pl_start = frame->radioFrame.raw + EXTENSION_START_IDX + frame->radioFrame.ext_length;
			skyArray_push_rx_packet_monotonic(self->arrayBuffers[frame->radioFrame.vc], pl_start, pl_length);
		}
		if(frame->radioFrame.flags & SKY_FLAG_ARQ_ON){
			/* ARQ is configured off, but frame has ARQ sequence. What to do? */
			int pl_length = frame->radioFrame.length - (EXTENSION_START_IDX + frame->radioFrame.ext_length);
			uint8_t* pl_start = frame->radioFrame.raw + EXTENSION_START_IDX + frame->radioFrame.ext_length;
			skyArray_push_rx_packet_monotonic(self->arrayBuffers[frame->radioFrame.vc], pl_start, pl_length);
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
			int pl_length = frame->radioFrame.length - (EXTENSION_START_IDX + frame->radioFrame.ext_length);
			uint8_t* pl_start = frame->radioFrame.raw + EXTENSION_START_IDX + frame->radioFrame.ext_length;
			int r = skyArray_push_rx_packet(self->arrayBuffers[radioFrame->vc], pl_start, pl_length, radioFrame->arq_sequence);
			//printf("\treceived seq: %d,  it fits: %d,  head seq: %d\n", radioFrame->arq_sequence, r, self->arrayBuffers[radioFrame->vc]->primaryRcvRing->head_sequence);
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
	if((frame->radioFrame.ext_length + EXTENSION_START_IDX) > frame->radioFrame.length){
		return; //todo error: too short packet.
	}
	if(frame->radioFrame.ext_length <= 1){
		return; //no extensions.
	}
	int cursor = EXTENSION_START_IDX;
	while ((cursor - EXTENSION_START_IDX) < frame->radioFrame.ext_length){
		SkyPacketExtension ext;
		int r = interpret_extension(&frame->radioFrame.raw[cursor], frame->radioFrame.length - cursor, &ext);
		if(r < 0){
			return;
		}
		cursor += r;
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
	//printf("\trr head: %d. \n", arqReq.sequence);
	int r = skyArray_schedule_resend(self->arrayBuffers[vc], arqReq.sequence);
	//printf("\tschedule resend for: %d.  success:%d\n", arqReq.sequence, r);
	if(r < 0){
		return;
	}

	for (int i = 0; i < 16; ++i) {
		if(mask & (1<<i)){
			continue;
		}
		uint8_t sequence = (uint8_t)sequence_wrap(arqReq.sequence + i + 1);
		if(sequence == self->arrayBuffers[vc]->primarySendRing->tx_sequence){
			//printf("\treached tx-head at %d. Break.\n", self->arrayBuffers[vc]->primarySendRing->tx_sequence);
			return;
		}
		r = skyArray_schedule_resend(self->arrayBuffers[vc], sequence);
		//printf("\tschedule resend for: %d.  success:%d\n",sequence, r);
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




