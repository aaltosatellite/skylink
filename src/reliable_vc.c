//
// Created by elmore on 12.11.2021.
//

#include "skylink/sequence_ring.h"
#include "skylink/reliable_vc.h"
#include "skylink/platform.h"
#include "skylink/skylink.h"
#include "skylink/utilities.h"
#include "skylink/frame.h"
#include "skylink/diag.h"



void sky_get_state(SkyHandle self, SkyState* state) {

	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++) {
		SkyVirtualChannel* vc = self->virtual_channels[i];
		SkyVCState* vc_state = &state->vc[i];

		vc_state->state = vc->arq_state_flag;
		vc_state->free_tx_slots = sendRing_count_free_send_slots(vc->sendRing);
		vc_state->tx_frames = sendRing_count_packets_to_send(vc->sendRing, 1);
		vc_state->rx_frames = rcvRing_count_readable_packets(vc->rcvRing);
	}

}


static int compute_required_elementount(int elementsize, int total_ring_slots, int maximum_pl_size){
	int32_t n_per_pl = element_buffer_element_requirement(elementsize, maximum_pl_size);
	int32_t required = (total_ring_slots * n_per_pl);
	return required;
}



//===== SKYLINK VIRTUAL CHANNEL  =======================================================================================
SkyVirtualChannel* new_virtual_channel(SkyVCConfig* config){
	if((config->rcv_ring_len < 6) || (config->rcv_ring_len > 250))
		config->rcv_ring_len = 32;
	if(config->horizon_width > (config->rcv_ring_len - 3))
		config->horizon_width = config->rcv_ring_len - 3;
	if((config->send_ring_len < 6) || (config->send_ring_len > 250))
		config->send_ring_len = 32;
	if((config->element_size < 12) || (config->element_size > 500))
		config->element_size = 36;
	SkyVirtualChannel* vchannel = SKY_MALLOC(sizeof(SkyVirtualChannel));
	vchannel->sendRing = new_send_ring(config->send_ring_len, 0);
	SKY_ASSERT(vchannel->sendRing != NULL)
	vchannel->rcvRing = new_rcv_ring(config->rcv_ring_len, config->horizon_width, 0);
	SKY_ASSERT(vchannel->rcvRing != NULL)
	int32_t ring_slots = config->rcv_ring_len + config->send_ring_len -2;
	int32_t optimal_element_count = compute_required_elementount(config->element_size, ring_slots, SKY_MAX_PAYLOAD_LEN);
	vchannel->elementBuffer = new_element_buffer(config->element_size, optimal_element_count);
	SKY_ASSERT(vchannel->elementBuffer != NULL)
	sky_vc_wipe_to_arq_off_state(vchannel);
	return vchannel;
}


void destroy_virtual_channel(SkyVirtualChannel* vchannel){
	destroy_send_ring(vchannel->sendRing);
	destroy_rcv_ring(vchannel->rcvRing);
	destroy_element_buffer(vchannel->elementBuffer);
	SKY_FREE(vchannel);
}


void sky_vc_wipe_to_arq_off_state(SkyVirtualChannel* vchannel){
	wipe_send_ring(vchannel->sendRing, vchannel->elementBuffer, 0);
	wipe_rcv_ring(vchannel->rcvRing, vchannel->elementBuffer, 0);
	wipe_element_buffer(vchannel->elementBuffer);
	vchannel->need_recall = 0;
	vchannel->arq_state_flag = ARQ_STATE_OFF;
	vchannel->arq_session_identifier = 0;
	vchannel->last_tx_tick = 0;
	vchannel->last_rx_tick = 0;
	vchannel->last_ctrl_send_tick = 0;
	vchannel->unconfirmed_payloads = 0;
	vchannel->handshake_send = 0;
}


int sky_vc_wipe_to_arq_init_state(SkyVirtualChannel* vchannel){
	wipe_rcv_ring(vchannel->rcvRing, vchannel->elementBuffer, 0);
	wipe_send_ring(vchannel->sendRing, vchannel->elementBuffer, 0);
	vchannel->need_recall = 0;
	vchannel->arq_state_flag = ARQ_STATE_IN_INIT;
	vchannel->arq_session_identifier = (uint32_t) sky_get_tick_time();
	vchannel->last_tx_tick = sky_get_tick_time();
	vchannel->last_rx_tick = sky_get_tick_time();
	vchannel->last_ctrl_send_tick = 0;
	vchannel->unconfirmed_payloads = 0;
	vchannel->handshake_send = 0;
	return 0;
}


void sky_vc_wipe_to_arq_on_state(SkyVirtualChannel* vchannel, uint32_t identifier){
	wipe_rcv_ring(vchannel->rcvRing, vchannel->elementBuffer, 0);
	wipe_send_ring(vchannel->sendRing, vchannel->elementBuffer, 0);
	vchannel->need_recall = 0;
	vchannel->arq_state_flag = ARQ_STATE_ON;
	vchannel->arq_session_identifier = identifier;
	vchannel->last_tx_tick = sky_get_tick_time();
	vchannel->last_rx_tick = sky_get_tick_time();
	vchannel->last_ctrl_send_tick = 0;
	vchannel->unconfirmed_payloads = 0;
	vchannel->handshake_send = 1;
}


int sky_vc_handle_handshake(SkyVirtualChannel* vchannel, uint8_t peer_state, uint32_t identifier){
	int match = (identifier == vchannel->arq_session_identifier);

	if((vchannel->arq_state_flag == ARQ_STATE_ON) && match ){
		vchannel->handshake_send = 0;
		if(peer_state == ARQ_STATE_IN_INIT){
			vchannel->handshake_send = 1;
		}
		return 0;
	}

	if((vchannel->arq_state_flag == ARQ_STATE_ON) && !match ){
		sky_vc_wipe_to_arq_on_state(vchannel, identifier);
		vchannel->handshake_send = 1;
		return 1;
	}

	if((vchannel->arq_state_flag == ARQ_STATE_IN_INIT) && match ){
		vchannel->arq_state_flag = ARQ_STATE_ON;
		vchannel->handshake_send = 0;
		return 1;
	}

	if((vchannel->arq_state_flag == ARQ_STATE_IN_INIT) && !match ){
		if(identifier > vchannel->arq_session_identifier){
			sky_vc_wipe_to_arq_on_state(vchannel, identifier);
			vchannel->handshake_send = 1;
			return 1;
		}
		return 0;
	}

	if(vchannel->arq_state_flag == ARQ_STATE_OFF){
		sky_vc_wipe_to_arq_on_state(vchannel, identifier);
		vchannel->handshake_send = 1;
		return 1;
	}
	return 0;
}


void sky_vc_poll_arq_state_timeout(SkyVirtualChannel* vchannel, tick_t now, tick_t timeout){
	if(vchannel->arq_state_flag == ARQ_STATE_OFF){
		return;
	}
	if(wrap_time_ticks(now - vchannel->last_tx_tick) > timeout ){
		sky_vc_wipe_to_arq_off_state(vchannel);
	}
	if(wrap_time_ticks(now - vchannel->last_rx_tick) > timeout ){
		sky_vc_wipe_to_arq_off_state(vchannel);
	}
}
//===== SKYLINK VIRTUAL CHANNEL ========================================================================================







//=== SEND =============================================================================================================
//======================================================================================================================
int sky_vc_push_packet_to_send(SkyVirtualChannel* vchannel, void* payload, int length){
	if(length > SKY_MAX_PAYLOAD_LEN){
		return SKY_RET_TOO_LONG_PAYLOAD;
	}
	return sendRing_push_packet_to_send(vchannel->sendRing, vchannel->elementBuffer, payload, length);
}

int sky_vc_send_buffer_is_full(SkyVirtualChannel* vchannel){
	return sendRing_is_full(vchannel->sendRing);
}

int sky_vc_schedule_resend(SkyVirtualChannel* arqRing, int sequence){
	return sendRing_schedule_resend(arqRing->sendRing, sequence);
}

int sky_vc_count_packets_to_tx(SkyVirtualChannel* vchannel, int include_resend){
	return sendRing_count_packets_to_send(vchannel->sendRing, include_resend);
}

int sky_vc_read_packet_for_tx(SkyVirtualChannel* vchannel, void* tgt, int* sequence, int include_resend){
	return sendRing_read_to_tx(vchannel->sendRing, vchannel->elementBuffer, tgt, sequence, include_resend);
}

int sky_vc_read_packet_for_tx_monotonic(SkyVirtualChannel* vchannel, void* tgt, int* sequence){
	int read = sendRing_read_to_tx(vchannel->sendRing, vchannel->elementBuffer, tgt, sequence, 0);
	if(read >= 0){
		sendRing_clean_tail_up_to(vchannel->sendRing, vchannel->elementBuffer, vchannel->sendRing->tx_sequence);
	}
	return read;
}

int sky_vc_can_recall(SkyVirtualChannel* vchannel, int sequence){
	return sendRing_can_recall(vchannel->sendRing, sequence);
}

void sky_vc_update_tx_sync(SkyVirtualChannel* vchannel, int peer_rx_head_sequence_by_ctrl, tick_t now){
	int n_cleared = sendRing_clean_tail_up_to(vchannel->sendRing, vchannel->elementBuffer, peer_rx_head_sequence_by_ctrl);
	if(n_cleared > 0){
		vchannel->last_tx_tick = now;
	}
	if(vchannel->sendRing->tx_sequence == peer_rx_head_sequence_by_ctrl){
		vchannel->last_tx_tick = now;
	}
}

int sky_vc_peek_next_tx_size_and_sequence(SkyVirtualChannel* vchannel, int include_resend, int* length, int* sequence){
	return sendRing_peek_next_tx_size_and_sequence(vchannel->sendRing, vchannel->elementBuffer, include_resend, length, sequence);
}
//======================================================================================================================
//======================================================================================================================







//=== RECEIVE ==========================================================================================================
//======================================================================================================================
int sky_vc_count_readable_rcv_packets(SkyVirtualChannel* vchannel){
	return rcvRing_count_readable_packets(vchannel->rcvRing);
}

int sky_vc_read_next_received(SkyVirtualChannel* vchannel, void* tgt, int max_length){
	return rcvRing_read_next_received(vchannel->rcvRing, vchannel->elementBuffer, tgt, max_length);
}

int sky_vc_push_rx_packet_monotonic(SkyVirtualChannel* vchannel, void* src, int length){
	int sequence = vchannel->rcvRing->head_sequence;
	return rcvRing_push_rx_packet(vchannel->rcvRing, vchannel->elementBuffer, src, length, sequence);
}

int sky_vc_push_rx_packet(SkyVirtualChannel* vchannel, void* src, int length, int sequence, tick_t now){
	int r = rcvRing_push_rx_packet(vchannel->rcvRing, vchannel->elementBuffer, src, length, sequence);
	if(r > 0){ //head advanced at least by 1
		vchannel->last_rx_tick = now;
	}
	vchannel->unconfirmed_payloads++;
	return r;
}


void sky_vc_update_rx_sync(SkyVirtualChannel* vchannel, int peer_tx_head_sequence_by_ctrl, tick_t now){
	int sync = rcvRing_get_sequence_sync_status(vchannel->rcvRing, peer_tx_head_sequence_by_ctrl);
	if(sync == SKY_RET_RING_SEQUENCES_IN_SYNC){
		vchannel->last_rx_tick = now;
		vchannel->need_recall = 0;
	}
	if(sync == SKY_RET_RING_SEQUENCES_OUT_OF_SYNC){
		vchannel->need_recall = 1;
	}
	if(sync == SKY_RET_RING_SEQUENCES_DETACHED){
		//vc->arq_state_flag = ARQ_STATE_BROKEN;
	}
}
//======================================================================================================================
//======================================================================================================================



int sky_vc_content_to_send(SkyVirtualChannel* vchannel, SkyConfig* config, tick_t now, uint16_t frames_sent_in_this_vc_window){
	uint8_t state0 = vchannel->arq_state_flag;

	// ARQ OFF ---------------------------------------------------------------------------------------------------------
	if(state0 == ARQ_STATE_OFF){
		if(sendRing_count_packets_to_send(vchannel->sendRing, 0) > 0){
			return 1;
		}
	}
	// -----------------------------------------------------------------------------------------------------------------

	// ARQ IN INIT -----------------------------------------------------------------------------------------------------
	if(state0 == ARQ_STATE_IN_INIT){
		if (frames_sent_in_this_vc_window < config->arq_idle_frames_per_window) {
			return 1;
		}
	}
	// -----------------------------------------------------------------------------------------------------------------

	// ARQ ON ----------------------------------------------------------------------------------------------------------
	if(state0 == ARQ_STATE_ON){
		if(sendRing_count_packets_to_send(vchannel->sendRing, 1) > 0){
			return 1;
		}

		if((frames_sent_in_this_vc_window < config->arq_idle_frames_per_window) && (rcvRing_get_horizon_bitmap(vchannel->rcvRing) || vchannel->need_recall)){
			return 1;
		}

		if(vchannel->handshake_send){
			return 1;
		}

		int b0 = frames_sent_in_this_vc_window < config->arq_idle_frames_per_window;
		int b1 = wrap_time_ticks(now - vchannel->last_ctrl_send_tick) > config->arq_idle_frame_threshold;
		int b2 = wrap_time_ticks(now - vchannel->last_tx_tick) > config->arq_idle_frame_threshold;
		int b3 = wrap_time_ticks(now - vchannel->last_rx_tick) > config->arq_idle_frame_threshold;
		int b4 = vchannel->unconfirmed_payloads > 0;
		if(b0 && (b1 || b2 || b3 || b4)){
			return 1;
		}
	}
	// -----------------------------------------------------------------------------------------------------------------
	return 0;
}





int sky_vc_fill_frame(SkyVirtualChannel* vchannel, SkyConfig* config, SkyRadioFrame* frame, tick_t now, uint16_t frames_sent_in_this_vc_window){

	uint8_t state0 = vchannel->arq_state_flag;

	// ARQ OFF ---------------------------------------------------------------------------------------------------------
	if(state0 == ARQ_STATE_OFF){
		if(sendRing_count_packets_to_send(vchannel->sendRing, 0) == 0){
			return 0;
		}
		int length = -1;
		int sequence = -1;
		int r = sendRing_peek_next_tx_size_and_sequence(vchannel->sendRing, vchannel->elementBuffer, 0, &length, &sequence);
		SKY_ASSERT(r >= 0)
		SKY_ASSERT(length <= available_payload_space(frame))
		int read = sky_vc_read_packet_for_tx_monotonic(vchannel, frame->raw + frame->length, &sequence);
		SKY_ASSERT(read >= 0)
		frame->length += read;
		frame->flags |= SKY_FLAG_HAS_PAYLOAD;
		return 1;
	}
	//------------------------------------------------------------------------------------------------------------------


	// ARQ IN INIT -----------------------------------------------------------------------------------------------------
	if(state0 == ARQ_STATE_IN_INIT){
		if(frames_sent_in_this_vc_window < config->arq_idle_frames_per_window){
			sky_packet_add_extension_arq_handshake(frame, ARQ_STATE_IN_INIT, vchannel->arq_session_identifier);
			return 1;
		}
	}
	//------------------------------------------------------------------------------------------------------------------


	// ARQ ON ----------------------------------------------------------------------------------------------------------
	if(state0 == ARQ_STATE_ON){
		int ret = 0;
		if(vchannel->handshake_send){
			sky_packet_add_extension_arq_handshake(frame, ARQ_STATE_ON, vchannel->arq_session_identifier);
			vchannel->handshake_send = 0;
			ret = 1;
		}
		uint16_t mask = rcvRing_get_horizon_bitmap(vchannel->rcvRing);
		if((frames_sent_in_this_vc_window < config->arq_idle_frames_per_window) && (mask || vchannel->need_recall)){
			sky_packet_add_extension_arq_request(frame, vchannel->rcvRing->head_sequence, mask);
			vchannel->need_recall = 0;
			ret = 1;
		}

		int payload_to_send = sendRing_count_packets_to_send(vchannel->sendRing, 1) > 0;
		int b0 = frames_sent_in_this_vc_window < config->arq_idle_frames_per_window;
		int b1 = wrap_time_ticks(now - vchannel->last_ctrl_send_tick) > config->arq_idle_frame_threshold;
		int b2 = wrap_time_ticks(now - vchannel->last_tx_tick) > config->arq_idle_frame_threshold;
		int b3 = wrap_time_ticks(now - vchannel->last_rx_tick) > config->arq_idle_frame_threshold;
		int b4 = vchannel->unconfirmed_payloads > 0;
		if((b0 && (b1 || b2 || b3 || b4)) || payload_to_send){
			sky_packet_add_extension_arq_ctrl(frame, vchannel->sendRing->tx_sequence, vchannel->rcvRing->head_sequence);
			vchannel->last_ctrl_send_tick = now;
			vchannel->unconfirmed_payloads = 0;
			ret = 1;
		}

		if(sendRing_count_packets_to_send(vchannel->sendRing, 1) > 0){
			int sequence = -1;
			int length = 0;
			int r = sendRing_peek_next_tx_size_and_sequence(vchannel->sendRing, vchannel->elementBuffer, 1, &length, &sequence);
			int length_requirement = length + (int)sizeof(ExtARQSeq) + 1;
			if ((r == 0) && (length_requirement <= available_payload_space(frame)) && (sequence > -1)){
				sky_packet_add_extension_arq_sequence(frame, sequence);
				int read = sendRing_read_to_tx(vchannel->sendRing, vchannel->elementBuffer, frame->raw + frame->length, &sequence, 1);
				SKY_ASSERT(read >= 0)
				frame->length += read;
				frame->flags |= SKY_FLAG_HAS_PAYLOAD;
				ret = 1;
			} else {
				/* If the payload for some reason is too large, remove is nonetheless. */
				uint8_t tmp_tgt[300];
				sendRing_read_to_tx(vchannel->sendRing, vchannel->elementBuffer, tmp_tgt, &sequence, 1);
			}

		}
		return ret;
	}
	//------------------------------------------------------------------------------------------------------------------
	return 0;
}

static void sky_vc_process_content_arq_off(SkyVirtualChannel *vchannel, const uint8_t *pl, int len_pl);
static void sky_vc_process_content_arq_on(SkyVirtualChannel *vchannel, const uint8_t *pl, int len_pl, SkyParsedExtensions *exts, tick_t now);

int sky_vc_process_content(SkyVirtualChannel *vchannel,
						   const uint8_t *payload,
						   int payload_len,
						   SkyParsedExtensions *exts,
						   tick_t now)
{
	if (exts->arq_handshake != NULL) {
		const ExtARQHandshake *handshake = &exts->arq_handshake->ARQHandshake;
		sky_vc_handle_handshake(vchannel, handshake->peer_state, handshake->identifier);
		// return ????
	}

	uint8_t state0 = vchannel->arq_state_flag;
	if(state0 == ARQ_STATE_OFF){
		sky_vc_process_content_arq_off(vchannel, payload, payload_len);
	}
	if(state0 == ARQ_STATE_ON){
		sky_vc_process_content_arq_on(vchannel, payload, payload_len, exts, now);
	}
	return 0;
}

static void sky_vc_process_content_arq_off(SkyVirtualChannel *vchannel, const uint8_t *payload, int payload_len)
{
	if (payload_len < 0)
		return;
	sky_vc_push_rx_packet_monotonic(vchannel, payload, payload_len);
}

static void sky_vc_process_content_arq_on(SkyVirtualChannel *vchannel, const uint8_t *payload, int payload_len, SkyParsedExtensions *exts, tick_t now)
{
	int seq = -1;
	if (exts->arq_sequence) {
		seq = sky_ntoh16(exts->arq_sequence->ARQSeq.sequence);
	}

	if (exts->arq_ctrl) {
		sky_vc_update_tx_sync(vchannel, sky_ntoh16(exts->arq_ctrl->ARQCtrl.rx_sequence), now);
		sky_vc_update_rx_sync(vchannel, sky_ntoh16(exts->arq_ctrl->ARQCtrl.tx_sequence), now);
	}

	if ( (seq > -1) && (payload_len >= 0) ){
		sky_vc_push_rx_packet(vchannel, payload, payload_len, seq, now);
	}
	if ( (seq == -1) && (payload_len >= 0) ){
		//break arq?
	}

	if (exts->arq_request){
		uint16_t mask = sky_ntoh16(exts->arq_request->ARQReq.mask);
		sendRing_schedule_resends_by_mask(vchannel->sendRing, sky_ntoh16(exts->arq_request->ARQReq.sequence), mask);
	}
}
