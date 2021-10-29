//
// Created by elmore on 14.10.2021.
//

#include "skylink/arq_ring.h"


static int ring_wrap(int idx, int len){
	return ((idx % len) + len) % len;
}

static int sendRingFull(SkySendRing* sendRing){
	return ring_wrap(sendRing->head+1, sendRing->length) == ring_wrap(sendRing->tail, sendRing->length);
}

static int sequence_wrap(int sequence){
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
	rcvRing->horizon_width = horizon_width;
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


static void rcvRing_advance_head(SkyRcvRing* rcvRing){
	RingItem* item = &rcvRing->buff[rcvRing->head];
	while (item->idx != EB_NULL_IDX){
		int tail_collision_imminent = ring_wrap(rcvRing->head + rcvRing->horizon_width + 1, rcvRing->length) == rcvRing->tail;
		if (tail_collision_imminent){
			break;
		}
		rcvRing->head = ring_wrap(rcvRing->head + 1, rcvRing->length);
		rcvRing->head_sequence = sequence_wrap(rcvRing->head_sequence + 1);
		item = &rcvRing->buff[rcvRing->head];
	}
}


int rcvRing_rx_sequence_fits(SkyRcvRing* rcvRing, int sequence){
	if(sequence_wrap(sequence - rcvRing->head_sequence) > rcvRing->horizon_width){
		return 0;
	}
	return 1;
}


int rcvRing_read_next_received(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer, void* tgt, int* sequence){
	if(!rcvRing_count_readable_packets(rcvRing)){
		return -1;
	}
	RingItem* tail_item = &rcvRing->buff[rcvRing->tail];
	int read = element_buffer_read(elementBuffer, tgt, tail_item->idx, SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE);
	if(read < 0){
		return -1; //todo: Should never occur. Grounds for full wipe in order to recover.
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


int rcvRing_push_rx_packet(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer, void* src, int length, int sequence){
	if(!rcvRing_rx_sequence_fits(rcvRing, sequence)){
		return -1;
	}
	int ring_idx = ring_wrap(rcvRing->head + sequence_wrap(sequence - rcvRing->head_sequence), rcvRing->length);
	RingItem* item = &rcvRing->buff[ring_idx];
	if(item->idx != EB_NULL_IDX){
		return -1;
	}
	int idx = element_buffer_store(elementBuffer, src, length);
	if(idx < 0){
		return -1;
	}
	item->idx = idx;
	item->sequence = sequence;
	rcvRing->storage_count++;
	rcvRing_advance_head(rcvRing);
	return 0;
}


int rcvRing_get_horizon_bitmap(SkyRcvRing* rcvRing){
	uint16_t map = 0;
	for (int i = 0; i < rcvRing->horizon_width; ++i) {
		int ring_idx = ring_wrap(rcvRing->head + i, rcvRing->length);
		RingItem* item = &rcvRing->buff[ring_idx];
		if(item->idx != EB_NULL_IDX){
			map |= (1<<i);
		}
	}
	return map;
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
	sendRing->n_recall = n_recall;
	wipe_send_ring(sendRing, NULL, sequence_wrap(initial_sequence));
	return sendRing;
}


static void destroy_send_ring(SkySendRing* sendRing){
	free(sendRing->buff);
	free(sendRing);
}


static int sendRing_count_packets_to_send(SkySendRing* sendRing){
	return ring_wrap(sendRing->head - sendRing->tx_head, sendRing->length);
}


static int sendRing_push_packet_to_send(SkySendRing* sendRing, ElementBuffer* elementBuffer, void* payload, int length){
	if(sendRingFull(sendRing)){
		return -1;
	}
	int idx = element_buffer_store(elementBuffer, payload, length);
	if(idx < 0){
		return -1;
	}
	RingItem* item = &sendRing->buff[sendRing->head];
	item->idx = idx;
	item->sequence = sendRing->head_sequence;

	sendRing->storage_count++;
	sendRing->head = ring_wrap(sendRing->head+1, sendRing->length);
	sendRing->head_sequence = sequence_wrap(sendRing->head_sequence + 1); //matters only with arq on
	return 0;
}


static int sendRing_read_packet_to_tx(SkySendRing* sendRing, ElementBuffer* elementBuffer, void* tgt, int* sequence){
	if(sendRing_count_packets_to_send(sendRing) == 0){
		return -1;
	}
	RingItem* item = &sendRing->buff[sendRing->tx_head];
	int read = element_buffer_read(elementBuffer, tgt, item->idx, SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE);
	if(read < 0){
		return -1; //todo: Should never occur. Grounds for full wipe in order to recover.
	}
	*sequence = sendRing->tx_sequence;
	sendRing->tx_head = ring_wrap(sendRing->tx_head+1, sendRing->length);
	sendRing->tx_sequence = sequence_wrap(sendRing->tx_sequence + 1); //matters only with arq on

	if (ring_wrap(sendRing->tx_head - sendRing->tail, sendRing->length) > sendRing->n_recall) {
		RingItem* tail_item = &sendRing->buff[sendRing->tail];
		element_buffer_delete(elementBuffer, tail_item->idx);
		tail_item->idx = EB_NULL_IDX;
		tail_item->sequence = 0;
		sendRing->storage_count--;
		sendRing->tail = ring_wrap(sendRing->tail+1, sendRing->length);
		sendRing->tail_sequence = sequence_wrap(sendRing->tail_sequence + 1); //matters only with arq on
	}
	return read;
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


static int sendRing_recall(SkySendRing* sendRing, ElementBuffer* elementBuffer, int sequence, void* tgt){
	if(!sendRing_can_recall(sendRing, sequence)){
		return -1;
	}
	int n = sequence_wrap(sequence - sendRing->tail_sequence);
	int recall_ring_index = ring_wrap(sendRing->tail + n, sendRing->length);
	RingItem* item = &sendRing->buff[recall_ring_index];
	int read = element_buffer_read(elementBuffer, tgt, item->idx, SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE);
	if(read < 0){
		return -1; //todo: Should never occur. Grounds for full wipe in order to recover.
	}
	return read;
}

static int sendRing_schedule_resend(SkySendRing* sendRing, int sequence){
	if(sendRing->resend_count >= 16){
		return -1;
	}
	if(!sendRing_can_recall(sendRing, sequence)){
		return -1;
	}
	sendRing->resend_list[sendRing->resend_count] = sequence;
	sendRing->resend_count++;
	return 0;
}

static int sendRing_pop_resend_sequence(SkySendRing* sendRing){
	if(sendRing->resend_count == 0){
		return -1;
	}
	int r = sendRing->resend_list[0];
	sendRing->resend_count--;
	memmove(sendRing->resend_list, sendRing->resend_list+1, sendRing->resend_count);
	return r;
}

static int sendRing_peek_next_tx_size(SkySendRing* sendRing, ElementBuffer* elementBuffer){
	if(sendRing_count_packets_to_send(sendRing) == 0){
		return -1;
	}
	RingItem* item = &sendRing->buff[sendRing->tx_head];
	int length = element_buffer_get_data_length(elementBuffer, item->idx);
	return length;
}
//===== SEND RING ======================================================================================================





//===== SKYLINK ARRAY ==================================================================================================
SkyArqRing* new_arq_ring(SkyArrayConfig* config){
	if((config->rcv_ring_len >= ARQ_SEQUENCE_MODULO) || (config->send_ring_len >= ARQ_SEQUENCE_MODULO)){
		return NULL;
	}
	SkyArqRing* arqRing = SKY_MALLOC(sizeof(SkyArqRing));
	arqRing->primarySendRing = new_send_ring(config->send_ring_len, config->n_recall, config->initial_send_sequence);
	arqRing->secondarySendRing = new_send_ring(config->send_ring_len, config->n_recall, config->initial_send_sequence);
	arqRing->primaryRcvRing = new_rcv_ring(config->rcv_ring_len, config->horizon_width, config->initial_rcv_sequence);
	arqRing->secondaryRcvRing = new_rcv_ring(config->rcv_ring_len, config->horizon_width, config->initial_rcv_sequence);
	arqRing->elementBuffer = new_element_buffer(config->element_size, config->element_count);
	wipe_arq_ring(arqRing, config->initial_send_sequence, config->initial_rcv_sequence);
	return arqRing;
}


void destroy_arq_ring(SkyArqRing* array){
	destroy_send_ring(array->primarySendRing);
	destroy_send_ring(array->secondarySendRing);
	destroy_rcv_ring(array->primaryRcvRing);
	destroy_rcv_ring(array->secondaryRcvRing);
	destroy_element_buffer(array->elementBuffer);
	free(array);
}


void wipe_arq_ring(SkyArqRing* array, int initial_send_sequence, int initial_rcv_sequence){
	wipe_send_ring(array->primarySendRing, array->elementBuffer, initial_send_sequence);
	wipe_send_ring(array->secondarySendRing, array->elementBuffer, initial_send_sequence);
	wipe_rcv_ring(array->primaryRcvRing, array->elementBuffer, initial_rcv_sequence);
	wipe_rcv_ring(array->secondaryRcvRing, array->elementBuffer, initial_rcv_sequence);
	wipe_element_buffer(array->elementBuffer);
}


int skyArray_set_send_sequence(SkyArqRing* array, uint16_t sequence, int wipe_all){
	wipe_send_ring(array->secondarySendRing, array->elementBuffer, sequence);
	if(wipe_all){
		wipe_send_ring(array->primarySendRing, array->elementBuffer, sequence);
	}
	SkySendRing* x = array->secondarySendRing;
	array->secondarySendRing = array->primarySendRing;
	array->primarySendRing = x;
	return 0;
}


int skyArray_set_receive_sequence(SkyArqRing* array, uint16_t sequence, int wipe_all){
	wipe_rcv_ring(array->secondaryRcvRing, array->elementBuffer, sequence);
	if(wipe_all){
		wipe_rcv_ring(array->primaryRcvRing, array->elementBuffer, sequence);
	}
	SkyRcvRing* x = array->secondaryRcvRing;
	array->secondaryRcvRing = array->primaryRcvRing;
	array->primaryRcvRing = x;
	return 0;
}


void skyArray_clean_unreachable(SkyArqRing* array){
	if (rcvRing_count_readable_packets(array->secondaryRcvRing) == 0){
		if(array->secondaryRcvRing->storage_count > 0){
			wipe_rcv_ring(array->secondaryRcvRing, array->elementBuffer, 0);
		}
	}
	if (sendRing_count_packets_to_send(array->secondarySendRing) == 0){
		if(array->secondarySendRing->storage_count > 0){
			wipe_send_ring(array->secondarySendRing, array->elementBuffer, 0);
		}
	}
}
//===== SKYLINK ARRAY ==================================================================================================







//=== SEND =============================================================================================================
//======================================================================================================================
int skyArray_push_packet_to_send(SkyArqRing* array, void* payload, int length){
	return sendRing_push_packet_to_send(array->primarySendRing, array->elementBuffer, payload, length);
}


int skyArray_count_packets_to_tx(SkyArqRing* array){
	int s1 = sendRing_count_packets_to_send(array->primarySendRing);
	int s2 = sendRing_count_packets_to_send(array->secondarySendRing);
	return s1 + s2;
}


int skyArray_read_packet_for_tx(SkyArqRing* array, void* tgt, int* sequence){
	if(sendRing_count_packets_to_send(array->secondarySendRing)){ //secondary comes first, as it needs to be emptied first, after switching.
		return sendRing_read_packet_to_tx(array->secondarySendRing, array->elementBuffer, tgt, sequence);
	}
	if(sendRing_count_packets_to_send(array->primarySendRing)){
		return sendRing_read_packet_to_tx( array->primarySendRing, array->elementBuffer, tgt, sequence);
	}
	return -1;
}


int skyArray_can_recall(SkyArqRing* array, int sequence){
	if (sendRing_can_recall(array->primarySendRing, sequence)){
		return 1;
	}
	if (sendRing_can_recall(array->secondarySendRing, sequence)){
		return 2;
	}
	return 0;
}


int skyArray_recall(SkyArqRing* array, int sequence, void* tgt){
	int able = skyArray_can_recall(array, sequence);
	if(able == 1){
		return sendRing_recall(array->primarySendRing, array->elementBuffer, sequence, tgt);
	}
	if(able == 2){
		return sendRing_recall(array->secondarySendRing, array->elementBuffer, sequence, tgt);
	}
	return -1;
}


int skyArray_schedule_resend(SkyArqRing* arqRing, int sequence){
	return sendRing_schedule_resend(arqRing->primarySendRing, sequence);
}


int skyArray_pop_resend_sequence(SkyArqRing* arqRing){
	return sendRing_pop_resend_sequence(arqRing->primarySendRing);
}

int skyArray_peek_next_tx_size(SkyArqRing* arqRing){
	return sendRing_peek_next_tx_size(arqRing->primarySendRing, arqRing->elementBuffer);
}
//======================================================================================================================
//======================================================================================================================







//=== RECEIVE ==========================================================================================================
//======================================================================================================================
int skyArray_count_readable_rcv_packets(SkyArqRing* array){
	int s = rcvRing_count_readable_packets(array->primaryRcvRing) + rcvRing_count_readable_packets(array->secondaryRcvRing);
	return s;
}


int skyArray_read_next_received(SkyArqRing* array, void* tgt, int* sequence){
	if(rcvRing_count_readable_packets(array->secondaryRcvRing) > 0){
		return rcvRing_read_next_received(array->secondaryRcvRing, array->elementBuffer, tgt, sequence);
	}
	if(rcvRing_count_readable_packets(array->primaryRcvRing) > 0){
		return rcvRing_read_next_received(array->primaryRcvRing, array->elementBuffer, tgt, sequence);
	}
	return -1;
}


int skyArray_push_rx_packet_monotonic(SkyArqRing* array, void* src, int length){
	int sequence = array->primaryRcvRing->head_sequence;
	return rcvRing_push_rx_packet(array->primaryRcvRing, array->elementBuffer, src, length, sequence);
}


int skyArray_push_rx_packet(SkyArqRing* array, void* src, int length, int sequence){
	return rcvRing_push_rx_packet(array->primaryRcvRing, array->elementBuffer, src, length, sequence);
}


uint16_t skyArray_get_horizon_bitmap(SkyArqRing* array){
	return rcvRing_get_horizon_bitmap(array->primaryRcvRing);
}
//======================================================================================================================
//======================================================================================================================

