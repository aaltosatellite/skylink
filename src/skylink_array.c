//
// Created by elmore on 14.10.2021.
//

#include "skylink/skylink_array.h"



int init_rcv_ring(SkyRcvRing* rcvRing, RingItem* ring, int length, int horizon_width){
	if((length < 3) || (horizon_width < 0)){
		return -1;
	}
	if(length < (horizon_width + 3)){
		return -1;
	}
	rcvRing->length = length;
	rcvRing->buff = ring;
	rcvRing->horizon_width = horizon_width;
	wipe_rcv_ring(rcvRing, NULL);
	return 0;
}

void wipe_rcv_ring(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer){
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
	rcvRing->head_sequence = 0;
	rcvRing->tail_sequence = 0;
}

SkyRcvRing* new_rcv_ring(int length, int horizon_width){
	SkyRcvRing* rcvRing = SKY_MALLOC(sizeof(SkyRcvRing));
	RingItem* ring = SKY_MALLOC(sizeof(RingItem)*length);
	int r = init_rcv_ring(rcvRing, ring, length, horizon_width);
	if(r < 0){
		free(rcvRing);
		free(ring);
		return NULL;
	}
	return rcvRing;
}

void destroy_rcv_ring(SkyRcvRing* rcvRing){
	free(rcvRing->buff);
	free(rcvRing);
}



int init_send_ring(SkySendRing* sendRing, RingItem* ring, int length, int n_recall){
	if((length < 3) || (n_recall < 0)){
		return -1;
	}
	if(length < (n_recall + 1)){
		return -1;
	}
	sendRing->buff = ring;
	sendRing->length = length;
	sendRing->n_recall = n_recall;
	wipe_send_ring(sendRing, NULL);
	return 0;
}

void wipe_send_ring(SkySendRing* sendRing, ElementBuffer* elementBuffer){
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
	sendRing->head_sequence = 0;
	sendRing->tx_sequence = 0;
	sendRing->tail_sequence = 0;
}

SkySendRing* new_send_ring(int length, int n_recall){
	SkySendRing* sendRing = SKY_MALLOC(sizeof(SkySendRing));
	RingItem* ring = SKY_MALLOC(sizeof(RingItem)*length);
	int r = init_send_ring(sendRing, ring, length, n_recall);
	if(r < 0){
		free(ring);
		free(sendRing);
		return NULL;
	}
	return sendRing;
}

void destroy_send_ring(SkySendRing* sendRing){
	free(sendRing->buff);
	free(sendRing);
}







int init_skylink_array(SkyArrayAllocation* alloc, SkyArrayConfig* conf){
	alloc->skylinkArray->sendRing = alloc->sendRing;
	alloc->skylinkArray->rcvRing = alloc->rcvRing;
	alloc->skylinkArray->elementBuffer = alloc->elementBuffer;
	alloc->skylinkArray->arq_on = conf->arq_on;
	int r = init_rcv_ring(alloc->rcvRing, alloc->rcvbuff, conf->rcv_ring_length, conf->horizon_width);
	r |= init_send_ring(alloc->sendRing, alloc->sendbuff, conf->send_ring_length, conf->n_recall);
	r |= init_element_buffer(alloc->elementBuffer, alloc->element_pool, conf->element_size, conf->element_count);
	return r;
}

void wipe_skylink_array(SkylinkArray* array){
	wipe_send_ring(array->sendRing, array->elementBuffer);
	wipe_rcv_ring(array->rcvRing, array->elementBuffer);
	wipe_element_buffer(array->elementBuffer);
}


SkylinkArray* new_skylink_array(int element_count, int element_size, int rcv_ring_leng, int send_ring_len, uint8_t arq_on, int n_recall, int horizon_width){
	SkylinkArray* skylinkArray = SKY_MALLOC(sizeof(SkylinkArray));
	SkySendRing* sendRing = SKY_MALLOC(sizeof(SkySendRing));
	SkyRcvRing* rcvRing = SKY_MALLOC(sizeof(SkyRcvRing));
	ElementBuffer* elementBuffer = SKY_MALLOC(sizeof(ElementBuffer));
	RingItem* sendbuff = SKY_MALLOC(sizeof(RingItem)*send_ring_len);
	RingItem* rcvbuff = SKY_MALLOC(sizeof(RingItem)*rcv_ring_leng);
	uint8_t* element_pool = SKY_MALLOC(element_size*element_count);
	SkyArrayConfig config;
	config.element_count = element_count;
	config.element_size = element_size;
	config.n_recall = n_recall;
	config.send_ring_length = send_ring_len;
	config.rcv_ring_length = rcv_ring_leng;
	config.arq_on = arq_on;
	config.horizon_width = horizon_width;
	SkyArrayAllocation alloc;
	alloc.skylinkArray = skylinkArray;
	alloc.elementBuffer = elementBuffer;
	alloc.element_pool = element_pool;
	alloc.sendRing = sendRing;
	alloc.rcvRing = rcvRing;
	alloc.sendbuff = sendbuff;
	alloc.rcvbuff = rcvbuff;
	init_skylink_array(&alloc, &config);
	return skylinkArray;
}
void destroy_skylink_array(SkylinkArray* array){
	destroy_send_ring(array->sendRing);
	destroy_rcv_ring(array->rcvRing);
	destroy_element_buffer(array->elementBuffer);
	free(array);
}






int ring_wrap(int idx, int len){
	return (len + idx) % len;
}

int sendRingFull(SkySendRing* sendRing){
	return ring_wrap(sendRing->head+1, sendRing->length) == ring_wrap(sendRing->tail, sendRing->length);
}

//=== SEND =============================================================================================================
//======================================================================================================================
uint16_t skyArray_get_horizon_bitmap(SkylinkArray* array){
	int n = array->rcvRing->horizon_width;
	uint16_t map = 0;
	for (int i = 0; i < n; ++i) {
		int ring_idx = ring_wrap(array->rcvRing->head + i + 1, array->rcvRing->length);
		RingItem* item = &array->rcvRing->buff[ring_idx];
		if(item->idx != EB_NULL_IDX){
			map |= (1<<i);
		}
	}
	return map;
}


int skyArray_push_packet_to_send(SkylinkArray* array, void* payload, int length){
	if(sendRingFull(array->sendRing)){
		return -1;
	}
	int idx = element_buffer_store(array->elementBuffer, payload, length);
	if(idx < 0){
		return -1;
	}
	RingItem* item = &array->sendRing->buff[array->sendRing->head];
	item->idx = idx;
	item->sequence = array->sendRing->head_sequence;

	array->sendRing->head = ring_wrap(array->sendRing->head+1, array->sendRing->length);
	array->sendRing->head_sequence++; //matters only with arq on
	return 0;
}


int skyArray_packets_to_tx(SkylinkArray* array){
	return array->sendRing->tx_head != array->sendRing->head;
}


int skyArray_read_packet_for_tx(SkylinkArray* array, void* tgt){
	if(!skyArray_packets_to_tx(array)){
		return -1;
	}
	RingItem* item = &array->sendRing->buff[array->sendRing->tx_head];
	int read = element_buffer_read(array->elementBuffer, tgt, item->idx, 255); //todo: replace 255. A random constant max value.
	if(read < 0){
		return -1; //todo: Should never occur. Grounds for full wipe in order to recover.
	}
	array->sendRing->tx_head = ring_wrap(array->sendRing->tx_head+1, array->sendRing->length);
	array->sendRing->tx_sequence++; //matters only with arq on

	if (ring_wrap(array->sendRing->tx_head - array->sendRing->tail, array->sendRing->length) > array->sendRing->n_recall) {
		RingItem* tail_item = &array->sendRing->buff[array->sendRing->tail];
		element_buffer_delete(array->elementBuffer, tail_item->idx);
		tail_item->idx = EB_NULL_IDX;
		array->sendRing->tail = ring_wrap(array->sendRing->tail+1, array->sendRing->length);
		array->sendRing->tail_sequence++; //matters only with arq on
	}
	return read;
}


int skyArray_can_recall(SkylinkArray* array, int sequence){
	if(sequence < array->sendRing->tail_sequence){
		return 0;
	}
	if(sequence >= array->sendRing->tx_sequence){ //recall of untransmitted packet would be an odd thing to ask...
		return 0;
	}
	return 1;
}


int skyArray_recall(SkylinkArray* array, int sequence, void* tgt){
	if(!skyArray_can_recall(array, sequence)){
		return -1;
	}
	int n = sequence - array->sendRing->tail_sequence;
	int recall_ring_index = ring_wrap(array->sendRing->tail + n, array->sendRing->length);
	RingItem* item = &array->sendRing->buff[recall_ring_index];
	int read = element_buffer_read(array->elementBuffer, tgt, item->idx, 255); //todo: replace 255. A random constant max value.
	if(read < 0){
		return -1; //todo: Should never occur. Grounds for full wipe in order to recover.
	}
	return read;
}
//======================================================================================================================
//======================================================================================================================







//=== RECEIVE ==========================================================================================================
//======================================================================================================================
int skyArray_get_readable_rcv_packet_count(SkylinkArray* array){
	return ring_wrap(array->rcvRing->head - array->rcvRing->tail, array->rcvRing->length);
}

void skyArray_advance_rcv_head(SkylinkArray* array){
	RingItem* item = &array->rcvRing->buff[array->rcvRing->head];
	while (item->idx != EB_NULL_IDX){
		int tail_collision_imminent = ring_wrap(array->rcvRing->head + array->rcvRing->horizon_width + 1, array->rcvRing->length) == array->rcvRing->tail;
		if (tail_collision_imminent){
			break;
		}
		array->rcvRing->head = ring_wrap(array->rcvRing->head + 1, array->rcvRing->length);
		array->rcvRing->head_sequence++;
		item = &array->rcvRing->buff[array->rcvRing->head];
	}
}

int skyArray_read_next_received(SkylinkArray* array, void* tgt, int* sequence){
	if(!skyArray_get_readable_rcv_packet_count(array)){
		return -1;
	}
	RingItem* tail_item = &array->rcvRing->buff[array->rcvRing->tail];
	int read = element_buffer_read(array->elementBuffer, tgt, tail_item->idx, 255); //todo: replace this 255 with a better value
	if(read < 0){
		return -1; //todo: Should never occur. Grounds for full wipe in order to recover.
	}
	*sequence = tail_item->sequence;
	element_buffer_delete(array->elementBuffer, tail_item->idx);
	tail_item->idx = EB_NULL_IDX;
	array->rcvRing->tail = ring_wrap(array->rcvRing->tail + 1, array->rcvRing->length);
	array->rcvRing->tail_sequence++;
	skyArray_advance_rcv_head(array); //This nees to be performed bc: If the buffer has been so full that head advance has stalled, it needs to be advanced "manually"
	return read;
}

int skyArray_rx_sequence_fits(SkylinkArray* array, int sequence){
	if (sequence < array->rcvRing->head_sequence){
		return 0;
	}
	if (sequence > (array->rcvRing->head_sequence + array->rcvRing->horizon_width)){ // (head_sequence+horizon_width) = last to be accepted
		return 0;
	}
	return 1;
}

int skyArray_push_rx_packet(SkylinkArray* array, void* src, int length, int sequence){
	if(!skyArray_rx_sequence_fits(array, sequence)){
		return -1;
	}
	int ring_idx = ring_wrap(array->rcvRing->head + (sequence - array->rcvRing->head_sequence), array->rcvRing->length);
	RingItem* item = &array->rcvRing->buff[ring_idx];
	if(item->idx != EB_NULL_IDX){
		element_buffer_delete(array->elementBuffer, item->idx);
		item->idx = EB_NULL_IDX;
	}
	int idx = element_buffer_store(array->elementBuffer, src, length);
	if(idx < 0){
		return -1;
	}
	item->idx = idx;
	item->sequence = sequence;
	skyArray_advance_rcv_head(array);
	return 0;
}
//======================================================================================================================
//======================================================================================================================

