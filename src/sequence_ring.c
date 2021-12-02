//
// Created by elmore on 20.11.2021.
//

#include "skylink/sequence_ring.h"
#include "skylink/platform.h"
#include "skylink/skylink.h"
#include "skylink/utilities.h"
#include "skylink/frame.h"
#include "skylink/diag.h"



static int ring_wrap(int idx, int len){
	return ((idx % len) + len) % len;
}



int wrap_sequence(int sequence){
	return ring_wrap(sequence, ARQ_SEQUENCE_MODULO);
}




//===== RCV RING =======================================================================================================
void wipe_rcv_ring(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer, int initial_sequence){
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
	rcvRing->head_sequence = wrap_sequence(initial_sequence);
	rcvRing->tail_sequence = wrap_sequence(initial_sequence);
}


SkyRcvRing* new_rcv_ring(int length, int horizon_width, int initial_sequence){
	if((length < 3) || (horizon_width < 0)){
		return NULL;
	}
	if(length < (horizon_width + 3)){
		return NULL;
	}
	SkyRcvRing* rcvRing = SKY_MALLOC(sizeof(SkyRcvRing));
	RingItem* ring = SKY_MALLOC(sizeof(RingItem)*length);
	memset(ring, 0, sizeof(RingItem)*length);
	rcvRing->length = length;
	rcvRing->buff = ring;
	rcvRing->horizon_width =  (horizon_width <= ARQ_MAXIMUM_HORIZON) ? horizon_width : ARQ_MAXIMUM_HORIZON; //todo: Maybe we should allow larger horizons, despire recall-mask recalling 16 at time.
	wipe_rcv_ring(rcvRing, NULL, wrap_sequence(initial_sequence) );
	return rcvRing;
}


void destroy_rcv_ring(SkyRcvRing* rcvRing){
	SKY_FREE(rcvRing->buff);
	SKY_FREE(rcvRing);
}


int rcvRing_count_readable_packets(SkyRcvRing* rcvRing){
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
		rcvRing->head_sequence = wrap_sequence(rcvRing->head_sequence + 1);
		item = &rcvRing->buff[rcvRing->head];
		advanced++;
	}
	return advanced;
}


static int rcvRing_rx_sequence_fits(SkyRcvRing* rcvRing, int sequence){
	if(wrap_sequence(sequence - rcvRing->head_sequence) > rcvRing->horizon_width){
		return 0;
	}
	return 1;
}


int rcvRing_read_next_received(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer, void* tgt, int* sequence){
	if(rcvRing_count_readable_packets(rcvRing) == 0){
		return SKY_RET_RING_EMPTY;
	}
	RingItem* tail_item = &rcvRing->buff[rcvRing->tail];
	int read = element_buffer_read(elementBuffer, tgt, tail_item->idx, SKY_MAX_PAYLOAD_LEN + 100);
	if(read < 0){
		SKY_ASSERT(read > 0)
		return SKY_RET_RING_ELEMENTBUFFER_FAULT; //todo: Should never occur. Grounds for full wipe in order to recover.
	}
	*sequence = tail_item->sequence;
	element_buffer_delete(elementBuffer, tail_item->idx);
	tail_item->idx = EB_NULL_IDX;
	tail_item->sequence = 0;
	rcvRing->storage_count--;
	rcvRing->tail = ring_wrap(rcvRing->tail + 1, rcvRing->length);
	rcvRing->tail_sequence = wrap_sequence(rcvRing->tail_sequence + 1);
	rcvRing_advance_head(rcvRing); //This nees to be performed bc: If the buffer has been so full that head advance has stalled, it needs to be advanced "manually"
	return read;
}


int rcvRing_push_rx_packet(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer, void* src, int length, int sequence){
	if(!rcvRing_rx_sequence_fits(rcvRing, sequence)){
		return SKY_RET_RING_INVALID_SEQUENCE;
	}
	int ring_idx = ring_wrap(rcvRing->head + wrap_sequence(sequence - rcvRing->head_sequence), rcvRing->length);
	RingItem* item = &rcvRing->buff[ring_idx];
	if(item->idx != EB_NULL_IDX){
		return SKY_RET_RING_PACKET_ALREADY_IN;
	}
	int idx = element_buffer_store(elementBuffer, src, length);
	if(idx < 0){
		SKY_ASSERT(idx > 0)
		return SKY_RET_RING_BUFFER_FULL;
	}
	item->idx = idx;
	item->sequence = sequence;
	rcvRing->storage_count++;
	int advanced = rcvRing_advance_head(rcvRing);
	return advanced;
}


int rcvRing_get_horizon_bitmap(SkyRcvRing* rcvRing){
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


int rcvRing_get_sequence_sync_status(SkyRcvRing* rcvRing, int peer_tx_head_sequence_by_ctrl){
	if(peer_tx_head_sequence_by_ctrl == rcvRing->head_sequence){
		return SKY_RET_RING_SEQUENCES_IN_SYNC;
	}
	int offset = wrap_sequence(peer_tx_head_sequence_by_ctrl - rcvRing->head_sequence);
	if(offset <= ARQ_MAXIMUM_HORIZON){
		return SKY_RET_RING_SEQUENCES_OUT_OF_SYNC;
	}
	return SKY_RET_RING_SEQUENCES_DETACHED;
}
//===== RCV RING =======================================================================================================




//===== SEND RING ======================================================================================================
void wipe_send_ring(SkySendRing* sendRing, ElementBuffer* elementBuffer, int initial_sequence){
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
	sendRing->head_sequence = wrap_sequence(initial_sequence);
	sendRing->tx_sequence = wrap_sequence(initial_sequence);
	sendRing->tail_sequence = wrap_sequence(initial_sequence);
	sendRing->resend_count = 0;
}


SkySendRing* new_send_ring(int length, int initial_sequence){
	if(length < 4){
		return NULL;
	}
	SkySendRing* sendRing = SKY_MALLOC(sizeof(SkySendRing));
	RingItem* ring = SKY_MALLOC(sizeof(RingItem)*length);
	memset(ring, 0, sizeof(RingItem)*length);
	sendRing->buff = ring;
	sendRing->length = length;
	wipe_send_ring(sendRing, NULL, wrap_sequence(initial_sequence));
	return sendRing;
}


void destroy_send_ring(SkySendRing* sendRing){
	SKY_FREE(sendRing->buff);
	SKY_FREE(sendRing);
}


int sendRing_is_full(SkySendRing* sendRing){
	return ring_wrap(sendRing->head+1, sendRing->length) == ring_wrap(sendRing->tail, sendRing->length);
}


//This function employs two ring inexings with different modulos. If you are not the original author (Markus), get some coffee.
int sendRing_can_recall(SkySendRing* sendRing, int sequence){
	int tx_head_ahead_of_tail = ring_wrap(sendRing->tx_head - sendRing->tail, sendRing->length);
	int sequence_ahead_of_tail = wrap_sequence(sequence - sendRing->tail_sequence);
	if(sequence_ahead_of_tail >= tx_head_ahead_of_tail){
		return 0;
	}
	return 1;
}


//This function employs two ring inexings with different modulos. If you are not the original author (Markus), get some coffee.
int sendRing_get_recall_ring_index(SkySendRing* sendRing, int recall_sequence){
	if(!sendRing_can_recall(sendRing, recall_sequence)){
		return SKY_RET_RING_CANNOT_RECALL;
	}
	int sequence_ahead_of_tail = wrap_sequence(recall_sequence - sendRing->tail_sequence);
	int index = ring_wrap(sendRing->tail + sequence_ahead_of_tail, sendRing->length);
	return index;
}


int sendRing_push_packet_to_send(SkySendRing* sendRing, ElementBuffer* elementBuffer, void* payload, int length){
	if(sendRing_is_full(sendRing)){
		return SKY_RET_RING_RING_FULL;
	}
	int idx = element_buffer_store(elementBuffer, payload, length);
	if(idx < 0){
		SKY_ASSERT(idx > 0)
		return SKY_RET_RING_BUFFER_FULL;
	}
	RingItem* item = &sendRing->buff[sendRing->head];
	item->idx = idx;
	item->sequence = sendRing->head_sequence;

	sendRing->storage_count++;
	sendRing->head = ring_wrap(sendRing->head+1, sendRing->length);
	sendRing->head_sequence = wrap_sequence(sendRing->head_sequence + 1);
	return item->sequence;
}


int sendRing_schedule_resend(SkySendRing* sendRing, int sequence){
	if(sendRing->resend_count >= ARQ_RESEND_SCHEDULE_DEPTH){
		return SKY_RET_RING_RESEND_FULL;
	}
	if(!sendRing_can_recall(sendRing, sequence)){
		return SKY_RET_RING_CANNOT_RECALL;
	}
	if(x_in_u16_array(sequence, sendRing->resend_list, sendRing->resend_count) >= 0){
		return 0;
	}
	sendRing->resend_list[sendRing->resend_count] = sequence;
	sendRing->resend_count++;
	return 0;
}


int sendRing_schedule_resends_by_mask(SkySendRing* sendRing, int sequence, uint16_t mask){
	int r = sendRing_schedule_resend(sendRing, sequence);
	for (int i = 0; i < 16; ++i) {
		if ( !(mask & (1<<i)) ){
			int s = wrap_sequence(sequence + i + 1);
			r |= sendRing_schedule_resend(sendRing, s);
		}
	}
	return r;
}


int sendRing_count_packets_to_send(SkySendRing* sendRing, int include_resend){
	int n = ring_wrap(sendRing->head - sendRing->tx_head, sendRing->length);

	/* Do not overstep over maximum horizon of what the peer has acked  */
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
		return SKY_RET_RING_EMPTY;
	}
	int r = sendRing->resend_list[0];
	sendRing->resend_count--;
	if(sendRing->resend_count > 0){
		for (int i = 0; i < sendRing->resend_count; ++i) {
			sendRing->resend_list[i] = sendRing->resend_list[i+1];
		}
	}
	return r;
}


static int sendRing_read_new_packet_to_tx_(SkySendRing* sendRing, ElementBuffer* elementBuffer, void* tgt, int* sequence){ //NEXT PACKET
	*sequence = -1;
	if(sendRing_count_packets_to_send(sendRing, 0) == 0){
		return SKY_RET_RING_EMPTY;
	}
	RingItem* item = &sendRing->buff[sendRing->tx_head];
	int read = element_buffer_read(elementBuffer, tgt, item->idx, SKY_MAX_PAYLOAD_LEN + 100);
	if(read < 0){
		SKY_ASSERT(read > 0)
		return SKY_RET_RING_ELEMENTBUFFER_FAULT; //todo: Should never occur. Grounds for full wipe in order to recover.
	}
	*sequence = sendRing->tx_sequence;
	sendRing->tx_head = ring_wrap(sendRing->tx_head+1, sendRing->length);
	sendRing->tx_sequence = wrap_sequence(sendRing->tx_sequence + 1);
	return read;
}


static int sendRing_read_recall_packet_to_tx_(SkySendRing* sendRing, ElementBuffer* elementBuffer, void* tgt, int* sequence){
	*sequence = -1;
	int recall_seq = sendRing_pop_resend_sequence(sendRing);
	if(recall_seq < 0){
		return SKY_RET_RING_EMPTY;
	}
	int recall_ring_index = sendRing_get_recall_ring_index(sendRing, recall_seq);
	if(recall_ring_index < 0){
		return SKY_RET_RING_CANNOT_RECALL;
	}
	RingItem* item = &sendRing->buff[recall_ring_index];
	int read = element_buffer_read(elementBuffer, tgt, item->idx, SKY_MAX_PAYLOAD_LEN + 100);
	if(read < 0){
		SKY_ASSERT(read > 0)
		return SKY_RET_RING_ELEMENTBUFFER_FAULT; //todo: Should never occur. Grounds for full wipe in order to recover.
	}
	*sequence = recall_seq;
	return read;
}


int sendRing_read_to_tx(SkySendRing* sendRing, ElementBuffer* elementBuffer, void* tgt, int* sequence, int include_resend){
	int read = SKY_RET_RING_EMPTY;
	if(include_resend && (sendRing->resend_count > 0)){
		read = sendRing_read_recall_packet_to_tx_(sendRing, elementBuffer, tgt, sequence);
		if(read >= 0){
			return read;
		}
	}
	read = sendRing_read_new_packet_to_tx_(sendRing, elementBuffer, tgt, sequence);
	return read;
}


int sendRing_peek_next_tx_size_and_sequence(SkySendRing* sendRing, ElementBuffer* elementBuffer, int include_resend, int* size, int* sequence){
	if(sendRing_count_packets_to_send(sendRing, include_resend) == 0){
		return SKY_RET_RING_EMPTY;
	}
	if(include_resend && (sendRing->resend_count > 0)){
		int idx = sendRing_get_recall_ring_index(sendRing, sendRing->resend_list[0]);
		if(idx >= 0){
			RingItem* item = &sendRing->buff[idx];
			int length = element_buffer_get_data_length(elementBuffer, item->idx);
			*size = length;
			*sequence = item->sequence;
			return 0;
		}
	}
	if(sendRing_count_packets_to_send(sendRing, 0) == 0){
		return SKY_RET_RING_EMPTY;
	}
	RingItem* item = &sendRing->buff[sendRing->tx_head];
	int length = element_buffer_get_data_length(elementBuffer, item->idx);
	*size = length;
	*sequence = item->sequence;
	return 0;
}


int sendRing_clean_tail_up_to(SkySendRing* sendRing, ElementBuffer* elementBuffer, int new_tail_sequence){
	int tx_ahead_of_tail = wrap_sequence(sendRing->tx_sequence - sendRing->tail_sequence);
	int peer_head_ahead_of_tail = wrap_sequence(new_tail_sequence - sendRing->tail_sequence);
	if(peer_head_ahead_of_tail > tx_ahead_of_tail){ //attempt to ack sequences that have not been sent.
		return SKY_RET_RING_INVALID_ACKNOWLEDGE;
	}

	int n_cleared = 0;
	while (sendRing->tail_sequence != new_tail_sequence){
		RingItem* tail_item = &sendRing->buff[sendRing->tail];
		element_buffer_delete(elementBuffer, tail_item->idx);
		tail_item->idx = EB_NULL_IDX;
		tail_item->sequence = 0;
		sendRing->storage_count--;
		sendRing->tail = ring_wrap(sendRing->tail+1, sendRing->length);
		sendRing->tail_sequence = wrap_sequence(sendRing->tail_sequence + 1);
		n_cleared++;
	}
	return n_cleared; //the number of payloads cleared.
}
//===== SEND RING ======================================================================================================
