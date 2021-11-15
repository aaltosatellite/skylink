//
// Created by elmore on 12.11.2021.
//

#include "skylink/arq_ring2.h"
#include "skylink/platform.h"
#include "skylink/skylink.h"
#include "skylink/utilities.h"



static int ring_wrap(int idx, int len){
	return ((idx % len) + len) % len;
}

static int sendRingFull(SkySendRing* sendRing){
	return ring_wrap(sendRing->head+1, sendRing->length) == ring_wrap(sendRing->tail, sendRing->length);
}

int sequence_wrap(int sequence){
	return ring_wrap(sequence, ARQ_SEQUENCE_MODULO);
}





//===== RCV RING =======================================================================================================
static void wipe_rcv_ring(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer, int initial_sequence){
	for (int i = 0; i < rcvRing->length; ++i) {
		RingItem* item = &rcvRing->buff[i];
		if((item->idx != EB_NULL_IDX) && elementBuffer ){
			element_buffer_delete(elementBuffer, item->idx);
		}
		item->idx = EB_NULL_IDX;
		item->sequence = 0;
	}
	rcvRing->head = 0;
	rcvRing->tail = 0;
	rcvRing->storage_count = 0;
	rcvRing->head_sequence = sequence_wrap(initial_sequence);
	rcvRing->tail_sequence = sequence_wrap(initial_sequence);
}


static SkyRcvRing* new_rcv_ring(int length, int horizon_width, int initial_sequence){
	if((length < 3) || (horizon_width < 0)){
		return NULL;
	}
	if(length < (horizon_width + 3)){
		return NULL;
	}
	SkyRcvRing* rcvRing = SKY_MALLOC(sizeof(SkyRcvRing));
	RingItem* ring = SKY_MALLOC(sizeof(RingItem)*length);
	rcvRing->length = length;
	rcvRing->buff = ring;
	rcvRing->horizon_width =  (horizon_width <= ARQ_MAXIMUM_HORIZON) ? horizon_width : ARQ_MAXIMUM_HORIZON; //todo: Maybe we should allow larger horizons, despire recall-mask recalling 16 at time.
	wipe_rcv_ring(rcvRing, NULL, sequence_wrap(initial_sequence) );
	return rcvRing;
}


static void destroy_rcv_ring(SkyRcvRing* rcvRing){
	free(rcvRing->buff);
	free(rcvRing);
}


static int rcvRing_count_readable_packets(SkyRcvRing* rcvRing){
	return ring_wrap(rcvRing->head - rcvRing->tail, rcvRing->length);
}


static int rcvRing_advance_head(SkyRcvRing* rcvRing){
	RingItem* item = &rcvRing->buff[rcvRing->head];
	int advanced = 0;
	while (item->idx != EB_NULL_IDX){
		int tail_collision_imminent = ring_wrap(rcvRing->head + rcvRing->horizon_width + 1, rcvRing->length) == rcvRing->tail;
		if (tail_collision_imminent){
			break;
		}
		rcvRing->head = ring_wrap(rcvRing->head + 1, rcvRing->length);
		rcvRing->head_sequence = sequence_wrap(rcvRing->head_sequence + 1);
		item = &rcvRing->buff[rcvRing->head];
		advanced++;
	}
	return advanced;
}


static int rcvRing_rx_sequence_fits(SkyRcvRing* rcvRing, int sequence){
	if(sequence_wrap(sequence - rcvRing->head_sequence) > rcvRing->horizon_width){
		return 0;
	}
	return 1;
}


static int rcvRing_read_next_received(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer, void* tgt, int* sequence){
	if(!rcvRing_count_readable_packets(rcvRing)){
		return RING_RET_EMPTY;
	}
	RingItem* tail_item = &rcvRing->buff[rcvRing->tail];
	int read = element_buffer_read(elementBuffer, tgt, tail_item->idx, SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE);
	if(read < 0){
		return RING_RET_ELEMENTBUFFER_FAULT; //todo: Should never occur. Grounds for full wipe in order to recover.
	}
	*sequence = tail_item->sequence;
	element_buffer_delete(elementBuffer, tail_item->idx);
	tail_item->idx = EB_NULL_IDX;
	tail_item->sequence = 0;
	rcvRing->storage_count--;
	rcvRing->tail = ring_wrap(rcvRing->tail + 1, rcvRing->length);
	rcvRing->tail_sequence = sequence_wrap(rcvRing->tail_sequence + 1);
	rcvRing_advance_head(rcvRing); //This nees to be performed bc: If the buffer has been so full that head advance has stalled, it needs to be advanced "manually"
	return read;
}


static int rcvRing_push_rx_packet(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer, void* src, int length, int sequence){
	if(!rcvRing_rx_sequence_fits(rcvRing, sequence)){
		return RING_RET_INVALID_SEQUENCE;
	}
	int ring_idx = ring_wrap(rcvRing->head + sequence_wrap(sequence - rcvRing->head_sequence), rcvRing->length);
	RingItem* item = &rcvRing->buff[ring_idx];
	if(item->idx != EB_NULL_IDX){
		return RING_RET_PACKET_ALREADY_IN; //todo: or return 0?
	}
	int idx = element_buffer_store(elementBuffer, src, length);
	if(idx < 0){
		return RING_RET_BUFFER_FULL;
	}
	item->idx = idx;
	item->sequence = sequence;
	rcvRing->storage_count++;
	int advanced = rcvRing_advance_head(rcvRing);
	return advanced;
}


static int rcvRing_get_horizon_bitmap(SkyRcvRing* rcvRing){
	uint16_t map = 0;
	int scan_count = (rcvRing->horizon_width < 16) ? rcvRing->horizon_width : 16;
	for (int i = 0; i < scan_count; ++i) { //HEAD is not contained in the mask.
		int ring_idx = ring_wrap(rcvRing->head + 1 + i, rcvRing->length);
		RingItem* item = &rcvRing->buff[ring_idx];
		if(item->idx != EB_NULL_IDX){
			map |= (1<<i);
		}
	}
	return map;
}


static int rcvRing_sync_status(SkyRcvRing* rcvRing, int peer_tx_head_sequence_by_ctrl){
	if(peer_tx_head_sequence_by_ctrl == rcvRing->head_sequence){
		return RING_RET_SEQUENCES_IN_SYNC;
	}
	int offset = sequence_wrap(peer_tx_head_sequence_by_ctrl - rcvRing->head_sequence);
	if(offset <= ARQ_MAXIMUM_HORIZON){
		return RING_RET_SEQUENCES_OUT_OF_SYNC;
	}
	return RING_RET_SEQUENCES_DETACHED;

}
//===== RCV RING =======================================================================================================




//===== SEND RING ======================================================================================================
static void wipe_send_ring(SkySendRing* sendRing, ElementBuffer* elementBuffer, int initial_sequence){
	for (int i = 0; i < sendRing->length; ++i) {
		RingItem* item = &sendRing->buff[i];
		if((item->idx != EB_NULL_IDX) && elementBuffer){
			element_buffer_delete(elementBuffer, item->idx);
		}
		item->idx = EB_NULL_IDX;
		item->sequence = 0;
	}
	sendRing->head = 0;
	sendRing->tx_head = 0;
	sendRing->tail = 0;
	sendRing->storage_count = 0;
	sendRing->head_sequence = sequence_wrap(initial_sequence);
	sendRing->tx_sequence = sequence_wrap(initial_sequence);
	sendRing->tail_sequence = sequence_wrap(initial_sequence);
	sendRing->resend_count = 0;
}


static SkySendRing* new_send_ring(int length, int n_recall, int initial_sequence){
	if((length < 3) || (n_recall < 0)){
		return NULL;
	}
	if(length < (n_recall + 1)){
		return NULL;
	}
	SkySendRing* sendRing = SKY_MALLOC(sizeof(SkySendRing));
	RingItem* ring = SKY_MALLOC(sizeof(RingItem)*length);
	sendRing->buff = ring;
	sendRing->length = length;
	sendRing->n_recall = (n_recall <= ARQ_MAXIMUM_HORIZON) ? n_recall : ARQ_MAXIMUM_HORIZON; //todo: a pointless variable.
	wipe_send_ring(sendRing, NULL, sequence_wrap(initial_sequence));
	return sendRing;
}


static void destroy_send_ring(SkySendRing* sendRing){
	free(sendRing->buff);
	free(sendRing);
}


//This function employs two ring inexings with different modulos. If you are not the original author (Markus), get some coffee.
static int sendRing_can_recall(SkySendRing* sendRing, int sequence){
	int n_tx_head_ahead_of_tail = ring_wrap(sendRing->tx_head - sendRing->tail, sendRing->length);
	int sequence_ahead_of_tail = sequence_wrap(sequence - sendRing->tail_sequence);
	if(sequence_ahead_of_tail >= n_tx_head_ahead_of_tail){
		return 0;
	}
	return 1;
}


//This function employs two ring inexings with different modulos. If you are not the original author (Markus), get some coffee.
static int sendRing_get_recall_ring_index(SkySendRing* sendRing, int recall_sequence){
	if(!sendRing_can_recall(sendRing, recall_sequence)){
		return RING_RET_CANNOT_RECALL;
	}
	int sequence_ahead_of_tail = sequence_wrap(recall_sequence - sendRing->tail_sequence);
	int index = ring_wrap(sendRing->tail + sequence_ahead_of_tail, sendRing->length);
	return index;
}


static int sendRing_push_packet_to_send(SkySendRing* sendRing, ElementBuffer* elementBuffer, void* payload, int length){
	if(sendRingFull(sendRing)){
		return RING_RET_RING_FULL;
	}
	int idx = element_buffer_store(elementBuffer, payload, length);
	if(idx < 0){
		return RING_RET_BUFFER_FULL;
	}
	RingItem* item = &sendRing->buff[sendRing->head];
	item->idx = idx;
	item->sequence = sendRing->head_sequence;

	sendRing->storage_count++;
	sendRing->head = ring_wrap(sendRing->head+1, sendRing->length);
	sendRing->head_sequence = sequence_wrap(sendRing->head_sequence + 1);
	return item->sequence;
}


static int sendRing_schedule_resend(SkySendRing* sendRing, int sequence){
	if(sendRing->resend_count >= ARQ_RESEND_SCHEDULE_DEPTH){
		return RING_RET_RESEND_FULL;
	}
	if(!sendRing_can_recall(sendRing, sequence)){
		return RING_RET_CANNOT_RECALL;
	}
	if(x_in_u16_array(sequence, sendRing->resend_list, sendRing->resend_count) >= 0){
		return 0;
	}
	sendRing->resend_list[sendRing->resend_count] = sequence;
	sendRing->resend_count++;
	return 0;
}


static int sendRing_count_packets_to_send(SkySendRing* sendRing, int include_resend){
	int n = ring_wrap(sendRing->head - sendRing->tx_head, sendRing->length);

	/* Do not overstep what the peer has acked  */
	if(ring_wrap(sendRing->tx_head - sendRing->tail, sendRing->length) > ARQ_MAXIMUM_HORIZON){
		n = 0;
	}

	if(include_resend){
		n = n + sendRing->resend_count;
	}
	return n;
}


static int sendRing_pop_resend_sequence(SkySendRing* sendRing){
	if(sendRing->resend_count == 0){
		return RING_RET_EMPTY;
	}
	int r = sendRing->resend_list[0];
	sendRing->resend_count--;
	if(sendRing->resend_count > 0){
		memmove(sendRing->resend_list, sendRing->resend_list+1, sendRing->resend_count);
	}
	return r;
}


static int sendRing_read_new_packet_to_tx_(SkySendRing* sendRing, ElementBuffer* elementBuffer, void* tgt, int* sequence){ //NEXT PACKET
	*sequence = 1;
	if(sendRing_count_packets_to_send(sendRing, 0) == 0){
		return RING_RET_EMPTY;
	}
	RingItem* item = &sendRing->buff[sendRing->tx_head];
	int read = element_buffer_read(elementBuffer, tgt, item->idx, SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE);
	if(read < 0){
		return RING_RET_ELEMENTBUFFER_FAULT; //todo: Should never occur. Grounds for full wipe in order to recover.
	}
	*sequence = sendRing->tx_sequence;
	sendRing->tx_head = ring_wrap(sendRing->tx_head+1, sendRing->length);
	sendRing->tx_sequence = sequence_wrap(sendRing->tx_sequence + 1);
	return read;
}


static int sendRing_read_recall_packet_to_tx_(SkySendRing* sendRing, ElementBuffer* elementBuffer, void* tgt, int* sequence){
	*sequence = -1;
	int recall_seq = sendRing_pop_resend_sequence(sendRing);
	if(recall_seq < 0){
		return RING_RET_EMPTY;
	}
	int recall_ring_index = sendRing_get_recall_ring_index(sendRing, recall_seq);
	if(recall_ring_index < 0){
		return RING_RET_CANNOT_RECALL;
	}
	RingItem* item = &sendRing->buff[recall_ring_index];
	int read = element_buffer_read(elementBuffer, tgt, item->idx, SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE);
	if(read < 0){
		return RING_RET_ELEMENTBUFFER_FAULT; //todo: Should never occur. Grounds for full wipe in order to recover.
	}
	*sequence = recall_seq;
	return read;
}


static int sendRing_read_to_tx(SkySendRing* sendRing, ElementBuffer* elementBuffer, void* tgt, int* sequence, int include_resend){
	int read = RING_RET_EMPTY;
	if(include_resend && (sendRing->resend_count > 0)){
		read = sendRing_read_recall_packet_to_tx_(sendRing, elementBuffer, tgt, sequence);
		if(read >= 0){
			return read;
		}
	}
	read = sendRing_read_new_packet_to_tx_(sendRing, elementBuffer, tgt, sequence);
	return read;
}


static int sendRing_peek_next_tx_size(SkySendRing* sendRing, ElementBuffer* elementBuffer, int include_resend){
	if(sendRing_count_packets_to_send(sendRing, include_resend) == 0){
		return RING_RET_EMPTY;
	}
	if(include_resend && (sendRing->resend_count > 0)){
		int idx = sendRing_get_recall_ring_index(sendRing, sendRing->resend_list[0]);
		if(idx >= 0){
			RingItem* item = &sendRing->buff[idx];
			int length = element_buffer_get_data_length(elementBuffer, item->idx);
			return length;
		}
	}
	RingItem* item = &sendRing->buff[sendRing->tx_head];
	int length = element_buffer_get_data_length(elementBuffer, item->idx);
	return length;
}


static int sendRing_confirm_transmit_up_to(SkySendRing* sendRing, ElementBuffer* elementBuffer, int peer_rx_head_equence){
	int tx_ahead_of_tail = sequence_wrap(sendRing->tx_sequence - sendRing->tail_sequence);
	int peer_head_ahead_of_tail = sequence_wrap(peer_rx_head_equence - sendRing->tail_sequence);
	if(peer_head_ahead_of_tail > tx_ahead_of_tail){ //attempt to ack sequences that have not been sent.
		return RING_RET_INVALID_ACKNOWLEDGE;
	}

	int n_cleared = 0;
	while (sendRing->tail_sequence != peer_rx_head_equence){
		RingItem* tail_item = &sendRing->buff[sendRing->tail];
		element_buffer_delete(elementBuffer, tail_item->idx);
		tail_item->idx = EB_NULL_IDX;
		tail_item->sequence = 0;
		sendRing->storage_count--;
		sendRing->tail = ring_wrap(sendRing->tail+1, sendRing->length);
		sendRing->tail_sequence = sequence_wrap(sendRing->tail_sequence + 1);
		n_cleared++;
	}
	return n_cleared; //the number of payloads cleared.
}


static int sendRing_get_next_tx_sequence(SkySendRing* sendRing){
	return sendRing->tx_sequence;
}
//===== SEND RING ======================================================================================================





//===== SKYLINK ARRAY ==================================================================================================
SkyArqRing2* new_arq_ring(SkyArrayConfig* config){
	if((config->rcv_ring_len >= ARQ_SEQUENCE_MODULO) || (config->send_ring_len >= ARQ_SEQUENCE_MODULO)){
		return NULL;
	}
	SkyArqRing2* arqRing = SKY_MALLOC(sizeof(SkyArqRing2));
	arqRing->sendRing = new_send_ring(config->send_ring_len, config->n_recall, config->initial_send_sequence);
	arqRing->rcvRing = new_rcv_ring(config->rcv_ring_len, config->horizon_width, config->initial_rcv_sequence);
	arqRing->elementBuffer = new_element_buffer(config->element_size, config->element_count);
	wipe_arq_ring(arqRing, config->initial_send_sequence, config->initial_rcv_sequence);
	return arqRing;
}


void destroy_arq_ring(SkyArqRing2* array){
	destroy_send_ring(array->sendRing);
	destroy_rcv_ring(array->rcvRing);
	destroy_element_buffer(array->elementBuffer);
	free(array);
}

//todo: there really is very little use for giving anything but sequence=0 here. This cannot initialize ARQ_ON state.
void wipe_arq_ring(SkyArqRing2* array, int new_send_sequence, int new_rcv_sequence){
	wipe_send_ring(array->sendRing, array->elementBuffer, new_send_sequence);
	wipe_rcv_ring(array->rcvRing, array->elementBuffer, new_rcv_sequence);
	wipe_element_buffer(array->elementBuffer);
	array->arq_state = ARQ_STATE_OFF;
	array->need_recall = 0;
	array->last_rx_ms = 0;
	array->last_tx_ms = 0;
}


int skyArray_begin_arq_handshake(SkyArqRing2* array, int32_t now_ms){
	wipe_rcv_ring(array->rcvRing, array->elementBuffer, 0);
	wipe_send_ring(array->sendRing, array->elementBuffer, 0);
	array->arq_state = ARQ_STATE_IN_INIT;
	array->last_tx_ms = now_ms;
	array->last_rx_ms = now_ms;
	return 0;
}


int skyArray_arq_handshake_received(SkyArqRing2* array, int32_t now_ms){
	wipe_rcv_ring(array->rcvRing, array->elementBuffer, 0);
	wipe_send_ring(array->sendRing, array->elementBuffer, 0);
	array->arq_state = ARQ_STATE_ON;
	array->last_tx_ms = now_ms;
	array->last_rx_ms = now_ms;
	//todo: send ack response
	return 0;
}


int skyArray_arq_handshake_complete(SkyArqRing2* array, int32_t now_ms){
	array->arq_state = ARQ_STATE_ON;
	array->last_tx_ms = now_ms;
	array->last_rx_ms = now_ms;
	return 0;
}


void skyArray_poll_arq_state_timeout(SkyArqRing2* array, int32_t now_ms){
	if(array->arq_state == ARQ_STATE_OFF){
		return;
	}
	if( wrap_time_ms(now_ms - array->last_tx_ms) > ARQ_TIMEOUT_MS ){
		array->arq_state = ARQ_STATE_OFF;
	}
	if( wrap_time_ms(now_ms - array->last_rx_ms) > ARQ_TIMEOUT_MS ){
		array->arq_state = ARQ_STATE_OFF;
	}
}
//===== SKYLINK ARRAY ==================================================================================================







//=== SEND =============================================================================================================
//======================================================================================================================
int skyArray_push_packet_to_send(SkyArqRing2* array, void* payload, int length){
	return sendRing_push_packet_to_send(array->sendRing, array->elementBuffer, payload, length);
}

int skyArray_schedule_resend(SkyArqRing2* arqRing, int sequence){
	return sendRing_schedule_resend(arqRing->sendRing, sequence);
}

int skyArray_count_packets_to_tx(SkyArqRing2* array, int include_resend){
	return sendRing_count_packets_to_send(array->sendRing, include_resend);
}

int skyArray_read_packet_for_tx(SkyArqRing2* array, void* tgt, int* sequence, int include_resend){
	if(sendRing_count_packets_to_send(array->sendRing, include_resend) > 0){
		return sendRing_read_to_tx(array->sendRing, array->elementBuffer, tgt, sequence, include_resend);
	}
	return RING_RET_EMPTY;
}

int skyArray_peek_next_tx_size(SkyArqRing2* arqRing, int include_resend){
	int x = sendRing_peek_next_tx_size(arqRing->sendRing, arqRing->elementBuffer, include_resend);
	return x;
}

int skyArray_get_next_transmitted_sequence(SkyArqRing2* arqRing){
	return sendRing_get_next_tx_sequence(arqRing->sendRing);
}

int skyArray_can_recall(SkyArqRing2* array, int sequence){
	if (sendRing_can_recall(array->sendRing, sequence)){
		return 1;
	}
	return 0;
}

void skyArray_update_tx_sync(SkyArqRing2* array, int peer_rx_head_sequence_by_ctrl, int32_t now_ms){
	int r = sendRing_confirm_transmit_up_to(array->sendRing, array->elementBuffer, peer_rx_head_sequence_by_ctrl);
	if(r > 0){
		array->last_tx_ms = now_ms;
	}
	if(array->sendRing->head_sequence == peer_rx_head_sequence_by_ctrl){
		array->last_tx_ms = now_ms;
	}
}
//======================================================================================================================
//======================================================================================================================







//=== RECEIVE ==========================================================================================================
//======================================================================================================================
int skyArray_count_readable_rcv_packets(SkyArqRing2* array){
	int s = rcvRing_count_readable_packets(array->rcvRing);
	return s;
}

int skyArray_read_next_received(SkyArqRing2* array, void* tgt, int* sequence){
	if(rcvRing_count_readable_packets(array->rcvRing) > 0){
		return rcvRing_read_next_received(array->rcvRing, array->elementBuffer, tgt, sequence);
	}
	return RING_RET_EMPTY;
}

int skyArray_push_rx_packet_monotonic(SkyArqRing2* array, void* src, int length){
	int sequence = array->rcvRing->head_sequence;
	int r = rcvRing_push_rx_packet(array->rcvRing, array->elementBuffer, src, length, sequence);
	if(r >= 0){
		return 0;
	}
	return r;
}

int skyArray_push_rx_packet(SkyArqRing2* array, void* src, int length, int sequence, int32_t now_ms){
	int r = rcvRing_push_rx_packet(array->rcvRing, array->elementBuffer, src, length, sequence);
	if(r > 0){ //head advanced at least by 1
		array->last_rx_ms = now_ms;
		return 0;
	}
	return r;
}

uint16_t skyArray_get_horizon_bitmap(SkyArqRing2* array){
	return rcvRing_get_horizon_bitmap(array->rcvRing);
}

void skyArray_update_rx_sync(SkyArqRing2* array, int peer_tx_head_sequence_by_ctrl, int32_t now_ms){
	int sync = rcvRing_sync_status(array->rcvRing, peer_tx_head_sequence_by_ctrl);
	if(sync == RING_RET_SEQUENCES_IN_SYNC){
		array->last_rx_ms = now_ms;
		array->need_recall = 0;
	}
	if(sync == RING_RET_SEQUENCES_OUT_OF_SYNC){
		array->need_recall = 1;
	}
	if(sync == RING_RET_SEQUENCES_DETACHED){
		//array->arq_state = ARQ_STATE_BROKEN;
	}
}
//======================================================================================================================
//======================================================================================================================

