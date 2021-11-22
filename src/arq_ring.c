//
// Created by elmore on 12.11.2021.
//

#include "skylink/packet_ring.h"
#include "skylink/arq_ring.h"
#include "skylink/platform.h"
#include "skylink/skylink.h"
#include "skylink/utilities.h"
#include "skylink/frame.h"



//===== SKYLINK ARRAY ==================================================================================================
SkyArqRing* new_arq_ring(SkyArrayConfig* config){
	if((config->rcv_ring_len >= ARQ_SEQUENCE_MODULO) || (config->send_ring_len >= ARQ_SEQUENCE_MODULO)){
		return NULL;
	}
	SkyArqRing* arqRing = SKY_MALLOC(sizeof(SkyArqRing));
	arqRing->sendRing = new_send_ring(config->send_ring_len, 0);
	arqRing->rcvRing = new_rcv_ring(config->rcv_ring_len, config->horizon_width, 0);
	arqRing->elementBuffer = new_element_buffer(config->element_size, config->element_count);
	skyArray_wipe_to_arq_off_state(arqRing);
	return arqRing;
}


void destroy_arq_ring(SkyArqRing* array){
	destroy_send_ring(array->sendRing);
	destroy_rcv_ring(array->rcvRing);
	destroy_element_buffer(array->elementBuffer);
	free(array);
}


void skyArray_wipe_to_arq_off_state(SkyArqRing* array){
	wipe_send_ring(array->sendRing, array->elementBuffer, 0);
	wipe_rcv_ring(array->rcvRing, array->elementBuffer, 0);
	wipe_element_buffer(array->elementBuffer);
	array->need_recall = 0;
	array->arq_state_flag = ARQ_STATE_OFF;
	array->arq_session_identifier = 0;
	array->last_rx_ms = 0;
	array->last_tx_ms = 0;
	array->last_ctrl_send = 0;
	array->handshake_send = 0;
}


int skyArray_wipe_to_arq_init_state(SkyArqRing* array, int32_t now_ms){
	wipe_rcv_ring(array->rcvRing, array->elementBuffer, 0);
	wipe_send_ring(array->sendRing, array->elementBuffer, 0);
	array->need_recall = 0;
	array->arq_state_flag = ARQ_STATE_IN_INIT;
	array->arq_session_identifier = now_ms;
	array->last_tx_ms = now_ms;
	array->last_rx_ms = now_ms;
	array->last_ctrl_send = 0;
	array->handshake_send = 0;
	return 0;
}


void skyArray_wipe_to_arq_on_state(SkyArqRing* array, int32_t identifier, int32_t now_ms){
	wipe_rcv_ring(array->rcvRing, array->elementBuffer, 0);
	wipe_send_ring(array->sendRing, array->elementBuffer, 0);
	array->need_recall = 0;
	array->arq_state_flag = ARQ_STATE_ON;
	array->arq_session_identifier = identifier;
	array->last_tx_ms = now_ms;
	array->last_rx_ms = now_ms;
	array->last_ctrl_send = 0;
	array->handshake_send = 1;
}


void skyArray_handle_handshake(SkyArqRing* array, ExtARQHandshake handshake_extension, int32_t now_ms){
	int match = (handshake_extension.identifier == array->arq_session_identifier);

	if( (array->arq_state_flag == ARQ_STATE_ON) && match ){
		array->handshake_send = 0;
		if(handshake_extension.peer_state == ARQ_STATE_IN_INIT){
			array->handshake_send = 1;
		}
		return;
	}

	if( (array->arq_state_flag == ARQ_STATE_ON) && !match ){
		skyArray_wipe_to_arq_on_state(array, handshake_extension.identifier, now_ms);
		array->handshake_send = 1;
		return;
	}

	if( (array->arq_state_flag == ARQ_STATE_IN_INIT) && match ){
		array->arq_state_flag = ARQ_STATE_ON;
		array->handshake_send = 0;
		return;
	}

	if( (array->arq_state_flag == ARQ_STATE_IN_INIT) && !match ){
		if(handshake_extension.identifier > array->arq_session_identifier){
			skyArray_wipe_to_arq_on_state(array, handshake_extension.identifier, now_ms);
			array->handshake_send = 1;
		}
		return;
	}

	if(array->arq_state_flag == ARQ_STATE_OFF){
		skyArray_wipe_to_arq_on_state(array, handshake_extension.identifier, now_ms);
		array->handshake_send = 1;
		return;
	}
}


void skyArray_poll_arq_state_timeout(SkyArqRing* array, int32_t now_ms){
	if(array->arq_state_flag == ARQ_STATE_OFF){
		return;
	}
	if( wrap_time_ms(now_ms - array->last_tx_ms) > ARQ_TIMEOUT_MS ){
		skyArray_wipe_to_arq_off_state(array); //todo: notify up in stack?
	}
	if( wrap_time_ms(now_ms - array->last_rx_ms) > ARQ_TIMEOUT_MS ){
		skyArray_wipe_to_arq_off_state(array); //todo: notify up in stack?
	}
}
//===== SKYLINK ARRAY ==================================================================================================







//=== SEND =============================================================================================================
//======================================================================================================================
int skyArray_push_packet_to_send(SkyArqRing* array, void* payload, int length){
	return sendRing_push_packet_to_send(array->sendRing, array->elementBuffer, payload, length);
}

int skyArray_schedule_resend(SkyArqRing* arqRing, int sequence){
	return sendRing_schedule_resend(arqRing->sendRing, sequence);
}

int skyArray_count_packets_to_tx(SkyArqRing* array, int include_resend){
	return sendRing_count_packets_to_send(array->sendRing, include_resend);
}

int skyArray_read_packet_for_tx(SkyArqRing* array, void* tgt, int* sequence, int include_resend){
	return sendRing_read_to_tx(array->sendRing, array->elementBuffer, tgt, sequence, include_resend);
}

int skyArray_read_packet_for_tx_monotonic(SkyArqRing* array, void* tgt, int* sequence){
	int read = sendRing_read_to_tx(array->sendRing, array->elementBuffer, tgt, sequence, 0);
	if(read >= 0){
		sendRing_clean_tail_up_to(array->sendRing, array->elementBuffer, array->sendRing->tx_sequence);
	}
	return read;
}

int skyArray_can_recall(SkyArqRing* array, int sequence){
	return sendRing_can_recall(array->sendRing, sequence);
}

void skyArray_update_tx_sync(SkyArqRing* array, int peer_rx_head_sequence_by_ctrl, int32_t now_ms){
	int n_cleared = sendRing_clean_tail_up_to(array->sendRing, array->elementBuffer, peer_rx_head_sequence_by_ctrl);
	if(n_cleared > 0){
		array->last_tx_ms = now_ms;
	}
	if(array->sendRing->head_sequence == peer_rx_head_sequence_by_ctrl){
		array->last_tx_ms = now_ms;
	}
}

int skyArray_peek_next_tx_size_and_sequence(SkyArqRing* array, int include_resend, int* length, int* sequence){
	return sendRing_peek_next_tx_size_and_sequence(array->sendRing, array->elementBuffer, include_resend, length, sequence);
}
//======================================================================================================================
//======================================================================================================================







//=== RECEIVE ==========================================================================================================
//======================================================================================================================
int skyArray_count_readable_rcv_packets(SkyArqRing* array){
	return rcvRing_count_readable_packets(array->rcvRing);
}

int skyArray_read_next_received(SkyArqRing* array, void* tgt, int* sequence){
	return rcvRing_read_next_received(array->rcvRing, array->elementBuffer, tgt, sequence);
}

int skyArray_push_rx_packet_monotonic(SkyArqRing* array, void* src, int length){
	int sequence = array->rcvRing->head_sequence;
	return rcvRing_push_rx_packet(array->rcvRing, array->elementBuffer, src, length, sequence);
}

int skyArray_push_rx_packet(SkyArqRing* array, void* src, int length, int sequence, int32_t now_ms){
	int r = rcvRing_push_rx_packet(array->rcvRing, array->elementBuffer, src, length, sequence);
	if(r > 0){ //head advanced at least by 1
		array->last_rx_ms = now_ms;
		return r;
	}
	return r;
}


void skyArray_update_rx_sync(SkyArqRing* array, int peer_tx_head_sequence_by_ctrl, int32_t now_ms){
	int sync = rcvRing_get_sequence_sync_status(array->rcvRing, peer_tx_head_sequence_by_ctrl);
	if(sync == RING_RET_SEQUENCES_IN_SYNC){
		array->last_rx_ms = now_ms;
		array->need_recall = 0;
	}
	if(sync == RING_RET_SEQUENCES_OUT_OF_SYNC){
		array->need_recall = 1;
	}
	if(sync == RING_RET_SEQUENCES_DETACHED){
		//array->arq_state_flag = ARQ_STATE_BROKEN;
	}
}
//======================================================================================================================
//======================================================================================================================



int skyArray_content_to_send(SkyArqRing* array, int32_t now_ms, uint16_t frames_sent_in_this_vc_window){
	uint8_t state0 = array->arq_state_flag;

	// ARQ OFF ---------------------------------------------------------------------------------------------------------
	if(state0 == ARQ_STATE_OFF){
		if(sendRing_count_packets_to_send(array->sendRing, 0) > 0){
			return 1;
		}
	}
	// -----------------------------------------------------------------------------------------------------------------

	// ARQ IN INIT -----------------------------------------------------------------------------------------------------
	if(state0 == ARQ_STATE_IN_INIT){
		if (frames_sent_in_this_vc_window < 2) { //todo: parametrize
			return 1;
		}
	}
	// -----------------------------------------------------------------------------------------------------------------

	// ARQ ON ----------------------------------------------------------------------------------------------------------
	if(state0 == ARQ_STATE_ON){
		if(sendRing_count_packets_to_send(array->sendRing, 1) > 0){
			return 1;
		}

		if((frames_sent_in_this_vc_window < 2) && (rcvRing_get_horizon_bitmap(array->rcvRing) || array->need_recall)){
			return 1;
		}

		if(array->handshake_send){
			return 1;
		}

		int b0 = frames_sent_in_this_vc_window < 2; //todo: paramterize me!
		int b1 = wrap_time_ms(now_ms - array->last_ctrl_send) > (ARQ_TIMEOUT_MS/4);
		int b2 = wrap_time_ms(now_ms - array->last_tx_ms) > (ARQ_TIMEOUT_MS/4);
		int b3 = wrap_time_ms(now_ms - array->last_rx_ms) > (ARQ_TIMEOUT_MS/4);
		if(b0 && (b1 || b2 || b3)){
			return 1;
		}
	}
	// -----------------------------------------------------------------------------------------------------------------
	return 0;
}





int skyArray_fill_frame(SkyArqRing* array, SkyRadioFrame* frame, int32_t now_ms, uint16_t frames_sent_in_this_vc_window){

	uint8_t state0 = array->arq_state_flag;

	// ARQ OFF ---------------------------------------------------------------------------------------------------------
	if(state0 == ARQ_STATE_OFF){
		if(sendRing_count_packets_to_send(array->sendRing, 0) == 0){
			return 0;
		}
		int length = -1;
		int sequence = -1;
		int r = sendRing_peek_next_tx_size_and_sequence(array->sendRing, array->elementBuffer, 0, &length, &sequence);
		assert(r >= 0);
		assert(length <= available_payload_space(frame));
		//assert((length + (int)sizeof(ExtARQSeq) + 1) <= available_payload_space(frame));
		//sky_packet_add_extension_arq_sequence(frame, sequence);
		int read = skyArray_read_packet_for_tx_monotonic(array, frame->raw+frame->length, &sequence);
		assert(read >= 0);
		frame->length += read;
		frame->flags |= SKY_FLAG_HAS_PAYLOAD;
		return 1;
	}
	//------------------------------------------------------------------------------------------------------------------


	// ARQ IN INIT -----------------------------------------------------------------------------------------------------
	if(state0 == ARQ_STATE_IN_INIT){
		if(frames_sent_in_this_vc_window < 2){ //todo: paramterize me!
			sky_packet_add_extension_arq_handshake(frame, ARQ_STATE_IN_INIT, array->arq_session_identifier);
			return 1;
		}
	}
	//------------------------------------------------------------------------------------------------------------------


	// ARQ ON ----------------------------------------------------------------------------------------------------------
	if(state0 == ARQ_STATE_ON){
		int ret = 0;
		if(array->handshake_send){
			sky_packet_add_extension_arq_handshake(frame, ARQ_STATE_ON, array->arq_session_identifier);
			array->handshake_send = 0;
			ret = 1;
		}
		uint16_t mask = rcvRing_get_horizon_bitmap(array->rcvRing);
		if((frames_sent_in_this_vc_window < 2) && (mask || array->need_recall)){
			sky_packet_add_extension_arq_request(frame, array->rcvRing->head_sequence, mask);
			array->need_recall = 0;
			ret = 1;
		}

		int b0 = frames_sent_in_this_vc_window < 2; //todo: paramterize me!
		int b1 = wrap_time_ms(now_ms - array->last_ctrl_send) > (ARQ_TIMEOUT_MS/4);
		int b2 = wrap_time_ms(now_ms - array->last_tx_ms) > (ARQ_TIMEOUT_MS/4);
		int b3 = wrap_time_ms(now_ms - array->last_rx_ms) > (ARQ_TIMEOUT_MS/4);
		if(b0 && (b1 || b2 || b3)){
			sky_packet_add_extension_arq_ctrl(frame, array->sendRing->tx_sequence, array->rcvRing->head_sequence);
			array->last_ctrl_send = now_ms;
			ret = 1;
		}

		if(sendRing_count_packets_to_send(array->sendRing, 1) > 0){
			int sequence = -1;
			int length = -1;
			int r = sendRing_peek_next_tx_size_and_sequence(array->sendRing, array->elementBuffer, 1, &length, &sequence);
			int length_requirement = length + sizeof(ExtARQSeq) + 1;
			if ((r == 0) && (length_requirement <= available_payload_space(frame))){
				sky_packet_add_extension_arq_sequence(frame, sequence);
				int read = sendRing_read_to_tx(array->sendRing, array->elementBuffer, frame->raw + frame->length, &sequence, 1);
				assert(read >= 0);
				frame->length += read;
				frame->flags |= SKY_FLAG_HAS_PAYLOAD;
				ret = 1;
			}
		}
		return ret;
	}
	//------------------------------------------------------------------------------------------------------------------
	return 0;
}





void skyArray_process_content(SkyArqRing* array,
							  void* pl,
							  int len_pl,
							  SkyPacketExtension* ext_seq,
							  SkyPacketExtension* ext_ctrl,
							  SkyPacketExtension* ext_handshake,
							  SkyPacketExtension* ext_rrequest,
							  int32_t now_ms){
	uint8_t state0 = array->arq_state_flag;
	if(state0 == ARQ_STATE_OFF){
		if (ext_handshake){
			skyArray_handle_handshake(array, ext_handshake->ARQHandshake, now_ms);
			return;
		}
		if (len_pl > 0){
			skyArray_push_rx_packet_monotonic(array, pl, len_pl);
			return;
		}
	}
	if(state0 == ARQ_STATE_IN_INIT){
		if (ext_handshake){
			skyArray_handle_handshake(array, ext_handshake->ARQHandshake, now_ms);
			return;
		}
	}
	if(state0 == ARQ_STATE_ON){
		int seq = -1;
		if (ext_seq){
			seq = ext_seq->ARQSeq.sequence;
		}

		if (ext_handshake){
			skyArray_handle_handshake(array, ext_handshake->ARQHandshake, now_ms);
			if(array->arq_state_flag != ARQ_STATE_ON){
				return;
			}
		}

		if (ext_ctrl){
			skyArray_update_tx_sync(array, ext_ctrl->ARQCtrl.rx_sequence, now_ms);
			skyArray_update_rx_sync(array, ext_ctrl->ARQCtrl.tx_sequence, now_ms);
		}

		if ( (seq > -1) && (len_pl > 0) ){
			int r = skyArray_push_rx_packet(array, pl, len_pl, seq, now_ms);
		}
		if ( (seq == -1) && (len_pl > 0) ){
			//break arq?
		}

		if (ext_rrequest){
			uint16_t mask = sky_ntoh16(ext_rrequest->ARQReq.mask);
			sendRing_schedule_resends_by_mask(array->sendRing, ext_rrequest->ARQReq.sequence, mask);
			/* †
			 * No. When unable to resend sequence requested, we send nothing. Was sich überhaupt sagen lässt, lässt
			 * sich klar sagen; und wovon man nicht reden kann, darüber muss man schweigen.
			 */
		}
	}
}





