
#include "skylink/sequence_ring.h"
#include "skylink/element_buffer.h"
#include "skylink/utilities.h"
#include "skylink/frame.h"
#include "skylink/diag.h"

#include "sky_platform.h"

#include <string.h> // memset, memcpy

/*
Modulo operation between an index and a length. This is done to allow the index to wrap around the length.
The modulo is always positive, because it allows situations where we need to get an index for a value that is negative due to subtraction in the code.

Example:

	int tx_head_ahead_of_tail = ring_wrap(sendRing->tx_head - sendRing->tail, sendRing->length);
	If the tx_head has wrapped around the ring, the result of the subtraction will be negative.
	This is not a problem, because the modulo will make it positive.

In practice the function returns the correct index on the ring even if given a negative index.
*/
static inline int ring_wrap(int idx, int len) {
	return positive_modulo(idx, len);
}




//===== RCV RING =======================================================================================================

//Wipe a recieve ring and reset the sequence counters. Also delete the payloads from the element buffer.
void sky_rcv_ring_wipe(SkyRcvRing *rcvRing, SkyElementBuffer *elementBuffer, sky_arq_sequence_t initial_sequence)
{
	//Loop through the ring and delete all elements.
	for (int i = 0; i < rcvRing->length; ++i) {

		//Get the item from the ring with the current index.
		RingItem* item = &rcvRing->buff[i];

		//Delete the payload from the element buffer if the item has an index and the element buffer is not NULL.
		if ((item->idx != EB_NULL_IDX) && elementBuffer )
			sky_element_buffer_delete(elementBuffer, item->idx);

		//Reset the item.
		item->idx = EB_NULL_IDX;
		item->sequence = 0;
	}

	//Reset the ring counters.
	rcvRing->head = 0;
	rcvRing->tail = 0;
	rcvRing->storage_count = 0;
	rcvRing->head_sequence = initial_sequence;
	rcvRing->tail_sequence = initial_sequence;
}

//Create a new receive ring.
SkyRcvRing *sky_rcv_ring_create(int length, int horizon_width, sky_arq_sequence_t initial_sequence)
{
	if (length < 3 || horizon_width < 0)
		return NULL;
	if (length < horizon_width + 3)
		return NULL;

	//Allocate memory for the ring and the buffer.
	SkyRcvRing* rcvRing = SKY_MALLOC(sizeof(SkyRcvRing));
	RingItem* ring = SKY_MALLOC(sizeof(RingItem)*length);

	//Set the memory to zero.
	memset(ring, 0, sizeof(RingItem)*length);

	//Set the ring parameters.
	rcvRing->length = length;
	rcvRing->buff = ring;
	//Set the horizon width. If the horizon width is larger than the maximum horizon width, set it to the maximum horizon width.
	rcvRing->horizon_width = (horizon_width <= ARQ_MAXIMUM_HORIZON) ? horizon_width : ARQ_MAXIMUM_HORIZON;

	//Wipe the ring to make sure it is empty.
	sky_rcv_ring_wipe(rcvRing, NULL, initial_sequence);
	return rcvRing;
}

//Destroy a recieve ring. Frees the buffer and the ring.
void sky_rcv_ring_destroy(SkyRcvRing* rcvRing)
{
	SKY_FREE(rcvRing->buff);
	SKY_FREE(rcvRing);
}

//Returns the amount of packets that can be read from the ring.
int rcvRing_count_readable_packets(SkyRcvRing* rcvRing)
{
	return ring_wrap(rcvRing->head - rcvRing->tail, rcvRing->length);
}

//Moves the head forward until it reaches the tail or the horizon width is reached. Returns the amount of steps the head was advanced.
static int rcvRing_advance_head(SkyRcvRing* rcvRing)
{
	RingItem* item = &rcvRing->buff[rcvRing->head];
	int advanced = 0;
	while (item->idx != EB_NULL_IDX) {
		//Check if the sum of the head and the horizon width is equal to the tail. If so, the head has reached the horizon window.
		int tail_collision_imminent = ring_wrap(rcvRing->head + rcvRing->horizon_width + 1, rcvRing->length) == rcvRing->tail;
		//Head has been advanced as far as possible
		if (tail_collision_imminent)
			break;
		//Advance the head.
		rcvRing->head = ring_wrap(rcvRing->head + 1, rcvRing->length);
		rcvRing->head_sequence++; // natural overflow
		//Get the item from the ring with the current index.
		item = &rcvRing->buff[rcvRing->head];
		//Increment the return value.
		advanced++;
	}
	return advanced;
}

/*
Reads a payload from ring to address pointed by tgt, if it's length is less than max_length.
Returns zero on success, or negative error code.
*/
int rcvRing_read_next_received(SkyRcvRing* rcvRing, SkyElementBuffer* elementBuffer, uint8_t* tgt, unsigned int max_length)
{
	//Check if there are packets to read.
	if (rcvRing_count_readable_packets(rcvRing) == 0)
		return SKY_RET_RING_EMPTY;

	// Get the item at the tail of the ring.
	RingItem* tail_item = &rcvRing->buff[rcvRing->tail];

	// Read the payload from the element buffer with the index given by the item.
	int ret = sky_element_buffer_read(elementBuffer, tgt, tail_item->idx, max_length);
	if (ret < 0)
		return ret;

	// Wipe the item from the element buffer and the ring.
	sky_element_buffer_delete(elementBuffer, tail_item->idx);
	tail_item->idx = EB_NULL_IDX;
	tail_item->sequence = 0;

	// Decrement the storage count and advance the tails.
	rcvRing->storage_count--;
	rcvRing->tail = ring_wrap(rcvRing->tail + 1, rcvRing->length);
	rcvRing->tail_sequence++; // natural overflow
	rcvRing_advance_head(rcvRing); //This needs to be performed bc: If the buffer has been so full that head advance has stalled, it needs to be advanced "manually"

	return SKY_RET_OK;
}

//Pushes a payload received with "sequence". Returns how many steps the head advances (>=0) or negative error code.
int rcvRing_push_rx_packet(SkyRcvRing *rcvRing, SkyElementBuffer *elementBuffer, const uint8_t *src, unsigned int length, sky_arq_sequence_t sequence)
{
	// Check if the sequence fits in the horizon window.
	// Sequence fits if the size of the horizon window is larger than the distance between the head and the sequence.
	sky_arq_sequence_t diff = sequence - rcvRing->head_sequence;
	if (diff > rcvRing->horizon_width)
		return SKY_RET_RING_INVALID_SEQUENCE;

	// Get the index on the ring for the packet and get the item from the ring with that index..
	int ring_idx = ring_wrap(rcvRing->head + diff, rcvRing->length);
	RingItem* item = &rcvRing->buff[ring_idx];

	// Check if the item already has an index. If so, the packet is already in the ring.
	if (item->idx != EB_NULL_IDX)
		return SKY_RET_RING_PACKET_ALREADY_IN;

	// Store the payload in the element buffer.
	int idx = sky_element_buffer_store(elementBuffer, src, length);
	if (idx < 0)
		return idx;

	// Set the item parameters.
	item->idx = idx;
	item->sequence = sequence;

	// Increment the storage count and advance the head.
	rcvRing->storage_count++;
	int advanced = rcvRing_advance_head(rcvRing);
	return advanced;
}


int rcvRing_get_horizon_bitmap(SkyRcvRing* rcvRing)
{
	//Get the amount of to be checked from the horizon for the ring (Max 16).
	int scan_count = (rcvRing->horizon_width < 16) ? rcvRing->horizon_width : 16;

	//Loop through the ring and set the i:th bit in the bitmap if the i:th packet can be read.
	sky_arq_mask_t map = 0;
	for (int i = 0; i < scan_count; ++i) { //HEAD is not contained in the mask.
		//Get the item from the ring with the current index.
		int ring_idx = ring_wrap(rcvRing->head + 1 + i, rcvRing->length);
		RingItem* item = &rcvRing->buff[ring_idx];
		//If the item has an index, set the i:th bit in the bitmap.
		if (item->idx != EB_NULL_IDX)
			map |= (1U << i);
	}

	return map;
}

/*
Get the sync status of the recieve ring.
Returns 0 if everything is in sync, or a negative error code if the ring is out of sync by some packets or irreversibly.
*/
int rcvRing_get_sequence_sync_status(SkyRcvRing *rcvRing, sky_arq_sequence_t peer_tx_head_sequence_by_ctrl)
{
	//Recieve ring is in sync.
	if (peer_tx_head_sequence_by_ctrl == rcvRing->head_sequence)
		return SKY_RET_OK; // SKY_RET_RING_SEQUENCES_IN_SYNC;

	//Recieve ring is out of sync, but can be resynced.
	sky_arq_sequence_t offset = peer_tx_head_sequence_by_ctrl - rcvRing->head_sequence;
	if (offset <= ARQ_MAXIMUM_HORIZON)
		return SKY_RET_RING_SEQUENCES_OUT_OF_SYNC;

	//Recieve ring is irreversibly out of sync.
	return SKY_RET_RING_SEQUENCES_DETACHED;
}
//===== RCV RING =======================================================================================================




//===== SEND RING ======================================================================================================

//Wipe a send ring and reset the values of the ring to their initial values. Also delete the payloads from the element buffer.
void sky_send_ring_wipe(SkySendRing *sendRing, SkyElementBuffer *elementBuffer, sky_arq_sequence_t initial_sequence)
{
	//Loop through the ring and delete all elements.
	for (int i = 0; i < sendRing->length; ++i) {
		RingItem* item = &sendRing->buff[i];
		//If the item has an index and the element buffer is not NULL, delete the payload from the element buffer.
		if ((item->idx != EB_NULL_IDX) && elementBuffer) {
			sky_element_buffer_delete(elementBuffer, item->idx);
		}
		//Reset the item.
		item->idx = EB_NULL_IDX;
		item->sequence = 0;
	}
	// Wipe the resend list..
	memset(sendRing->resend_list, 0, sizeof(sendRing->resend_list));
	//Reset the ring counters.
	sendRing->head = 0;
	sendRing->tx_head = 0;
	sendRing->tail = 0;
	sendRing->storage_count = 0;
	sendRing->head_sequence = initial_sequence;
	sendRing->tail_sequence = initial_sequence;
	sendRing->tx_sequence = initial_sequence;
	sendRing->resend_count = 0;
}

//Create a new send ring.
SkySendRing *sky_send_ring_create(int length, sky_arq_sequence_t initial_sequence)
{
	if(length < 4)
		return NULL;
	//Allocate memory for the ring and the buffer.
	SkySendRing* sendRing = SKY_MALLOC(sizeof(SkySendRing));
	RingItem* ring = SKY_MALLOC(sizeof(RingItem)*length);
	//Set the memory to zero.
	memset(ring, 0, sizeof(RingItem)*length);
	//Set the ring parameters.
	sendRing->buff = ring;
	sendRing->length = length;
	//Wipe the ring to make sure it is empty.
	sky_send_ring_wipe(sendRing, NULL, initial_sequence);
	return sendRing;
}

//Destroy a send ring. Frees the buffer and the ring.
void sky_send_ring_destroy(SkySendRing* sendRing)
{
	SKY_FREE(sendRing->buff);
	SKY_FREE(sendRing);
}

//Returns boolean 1/0 whether the ring is full. (Full: tail == head+1. "Push_packet_to_send" will fail)
int sendRing_is_full(SkySendRing* sendRing)
{
	return ring_wrap(sendRing->head+1, sendRing->length) == ring_wrap(sendRing->tail, sendRing->length);
}


//This function employs two ring indexings with different modulos. If you are not the original author (Markus), get some coffee.
int sendRing_can_recall(SkySendRing *sendRing, sky_arq_sequence_t sequence)
{
	//How many steps ahead of the tail is the first untransmitted package?
	int tx_head_ahead_of_tail = ring_wrap(sendRing->tx_head - sendRing->tail, sendRing->length);

	//How many steps ahead of the tail of the sequence is the given sequence?
	sky_arq_sequence_t sequence_ahead_of_tail = sequence - sendRing->tail_sequence;

	/*If the distance between the tail and the first untransmitted package is larger than the distance between the tail and the sequence,
	the sequence cannot be scheduled for retransmission. (It has not been transmitted yet)*/
	if (sequence_ahead_of_tail >= tx_head_ahead_of_tail)
		return 0;
	return 1;
}


//This function employs two ring inexings with different modulos. If you are not the original author (Markus), get some coffee.
int sendRing_get_recall_ring_index(SkySendRing *sendRing, sky_arq_sequence_t recall_sequence)
{
	//If the sequence cannot be scheduled for retransmission, return a negative error code.
	if (!sendRing_can_recall(sendRing, recall_sequence))
		return SKY_RET_RING_CANNOT_RECALL;

	//How many steps ahead of the tail of the sequence is the given sequence?
	sky_arq_sequence_t sequence_ahead_of_tail = recall_sequence - sendRing->tail_sequence;

	//Get the index of the packet to be retranmitted.
	int index = ring_wrap(sendRing->tail + sequence_ahead_of_tail, sendRing->length);
	return index;
}

//Pushes a new packet to be sent. Returns the sequence it is associated with, or a negative error code.
int sendRing_push_packet_to_send(SkySendRing* sendRing, SkyElementBuffer* elementBuffer, const uint8_t* payload, unsigned int length)
{
	//If the ring is full, return a negative error code.
	if (sendRing_is_full(sendRing))
		return SKY_RET_RING_RING_FULL;

	//Store the payload in the element buffer. If the payload could not be stored, return a negative error code.
	int idx = sky_element_buffer_store(elementBuffer, payload, length);
	if (idx < 0)
		return idx;

	//Get the item from the ring with the current index.
	RingItem* item = &sendRing->buff[sendRing->head];

	//Set the item parameters.
	item->idx = idx;
	item->sequence = sendRing->head_sequence;

	//Advance the head.
	sendRing->storage_count++;
	sendRing->head = ring_wrap(sendRing->head+1, sendRing->length);
	sendRing->head_sequence++; // natural overflow
	return item->sequence;
}

//Schedule a sequence for retransmission. Returns 0 if successful, or a negative error code.
int sendRing_schedule_resend(SkySendRing *sendRing, sky_arq_sequence_t sequence)
{
	// If the resend list is full, return a negative error code.
	if (sendRing->resend_count >= ARQ_RESEND_SCHEDULE_DEPTH)
		return SKY_RET_RING_RESEND_FULL;

	// Check if the sequence can be scheduled for retransmission. If not, return a negative error code.
	if (!sendRing_can_recall(sendRing, sequence))
		return SKY_RET_RING_CANNOT_RECALL;

	// Check if the sequence is already in the resend list.
	for (unsigned int i = 0; i < sendRing->resend_count; i++) {
		if (sendRing->resend_list[i] == sequence)
			return SKY_RET_OK;
	}

	// Add the sequence to the resend list.
	sendRing->resend_list[sendRing->resend_count++] = sequence;
	return SKY_RET_OK;
}


int sendRing_schedule_resends_by_mask(SkySendRing *sendRing, sky_arq_sequence_t sequence, sky_arq_mask_t mask)
{
	int ret;

	// Schedule the first sequence for retransmission.
	if ((ret = sendRing_schedule_resend(sendRing, sequence)) != SKY_RET_OK)
		return ret;
	sequence++;

	// Loop through the mask and schedule all the sequences that are marked absence for retransmission.
	sky_arq_mask_t select_mask = 0x01;
	for (unsigned int i = 0; i < 8 * sizeof(sky_arq_mask_t); i++) {
		if ((mask & select_mask) == 0) {
			if ((ret = sendRing_schedule_resend(sendRing, sequence)) != SKY_RET_OK)
				return ret;
		}
		sequence++;
		select_mask <<= 1;
	}

	return SKY_RET_OK;
}

//Return the amount of free slots in the send ring.
int sendRing_count_free_send_slots(SkySendRing* sendRing)
{
	int n = ring_wrap(sendRing->tail - (sendRing->head + 1), sendRing->length);
	return n;
}

/*
Count the number of packets that are waiting to be sent.

If include_resend is not 0, the number of packets that are waiting to be resent is also counted.
*/
int sendRing_count_packets_to_send(SkySendRing* sendRing, int include_resend)
{
	int n = ring_wrap(sendRing->head - sendRing->tx_head, sendRing->length);
	if (include_resend)
		n += sendRing->resend_count;
	return n;
}

//Pop the first sequence in the resend list. Returns the popped sequence or a negative error code.
static int sendRing_pop_resend_sequence(SkySendRing *sendRing)
{
	// Nothing to resend.
	if (sendRing->resend_count == 0)
		return SKY_RET_RING_EMPTY;

	//Get the first sequence in the resend list.
	sky_arq_sequence_t r = sendRing->resend_list[0];

	//Decrement the resend count.
	sendRing->resend_count--;

	//If there are still sequences in the resend list, shift them one step.
	for (unsigned int i = 1; i < sendRing->resend_count; i++)
		sendRing->resend_list[i - 1] = sendRing->resend_list[i];

	//Return the popped sequence.
	return r;
}

//Read a new payload from the elementBuffer to target.
static int sendRing_read_new_packet_to_tx_(SkySendRing *sendRing, SkyElementBuffer *elementBuffer, uint8_t *tgt, sky_arq_sequence_t *sequence)
{
	// Check if there are packets to send. Do not include packets that are scheduled for retransmission.
	if (sendRing_count_packets_to_send(sendRing, 0) == 0)
		return SKY_RET_RING_EMPTY;

	// Get the item from the ring with the index of the first untransmitted packet.
	RingItem* item = &sendRing->buff[sendRing->tx_head];

	// Read the payload from the element buffer with the index given by the item.
	int read = sky_element_buffer_read(elementBuffer, tgt, item->idx, SKY_PAYLOAD_MAX_LEN + 100); // TODO: What is the +100?
	if (read < 0)
		return read;

	// Set the sequence to the sequence of the first untransmitted package.
	*sequence = sendRing->tx_sequence;

	// Advance the head and tx_sequence.
	sendRing->tx_head = ring_wrap(sendRing->tx_head+1, sendRing->length);
	sendRing->tx_sequence++; // Natural overflow

	return read;
}

/*
Read a payload that is scheduled for retransmission from the elementBuffer to target.
*/
static int sendRing_read_recall_packet_to_tx_(SkySendRing *sendRing, SkyElementBuffer *elementBuffer, uint8_t *tgt, sky_arq_sequence_t *sequence)
{
	//Get the sequence of the first packet in the resend list.
	int recall_seq = sendRing_pop_resend_sequence(sendRing);
	//If the sequence is negative, there are no packets to resend.
	if (recall_seq < 0)
		return SKY_RET_RING_EMPTY;

	//Get the index of the sequence in the ring.
	int recall_ring_index = sendRing_get_recall_ring_index(sendRing, recall_seq);
	if (recall_ring_index < 0)
		return SKY_RET_RING_CANNOT_RECALL;

	// Get the item from the ring with the index of the sequence.
	RingItem* item = &sendRing->buff[recall_ring_index];

	// Read the payload from the element buffer with the index given by the item.
	int read = sky_element_buffer_read(elementBuffer, tgt, item->idx, SKY_PAYLOAD_MAX_LEN + 100); // TODO: What is the +100?
	if (read < 0)
		return read;

	// Set sequence to be the sequence popped from the resend list.
	*sequence = recall_seq;

	// Return the amount of bytes read.
	return read;
}

/*
Read a payload from the elementBuffer to target. If include_resend is not 0, try to read a packet that is scheduled for retransmission.
Returns the number of bytes read or a negative error code.
*/
int sendRing_read_to_tx(SkySendRing *sendRing, SkyElementBuffer *elementBuffer, uint8_t *tgt, sky_arq_sequence_t *sequence, int include_resend)
{
	int read = SKY_RET_RING_EMPTY;
	//If include_resend is not 0, try to read a packet that is scheduled for retransmission.
	if (include_resend && (sendRing->resend_count > 0)) {
		read = sendRing_read_recall_packet_to_tx_(sendRing, elementBuffer, tgt, sequence);
		if (read >= 0) 
			return read;
	}
	read = sendRing_read_new_packet_to_tx_(sendRing, elementBuffer, tgt, sequence);
	return read;
}

//Writes the sequence and the length of the next packet to be sent to the pointers size and sequence.
int sendRing_peek_next_tx_size_and_sequence(SkySendRing *sendRing, SkyElementBuffer *elementBuffer, int include_resend, sky_arq_sequence_t *sequence)
{
	//Nothing to send
	if (sendRing_count_packets_to_send(sendRing, include_resend) == 0) {
		return SKY_RET_RING_EMPTY;
	}
	//If include_resend is not 0, try to read a packet that is scheduled for retransmission.
	if (include_resend && (sendRing->resend_count > 0)) {
		//Get the ïndex of the first sequence in the resend list.
		int idx = sendRing_get_recall_ring_index(sendRing, sendRing->resend_list[0]);
		//If the index was found, get the length of the payload from the element buffer with the index given by the item.
		if (idx >= 0) {
			RingItem* item = &sendRing->buff[idx];
			int length = sky_element_buffer_get_data_length(elementBuffer, item->idx);
			//Write the length and the sequence to the pointers.
			*sequence = item->sequence;
			return length;
		}
	}
	//Check that there are packets to send not including packets that are scheduled for retransmission.
	if (sendRing_count_packets_to_send(sendRing, 0) == 0) {
		return SKY_RET_RING_EMPTY;
	}
	//Get the item from the ring with the index of the first untransmitted packet.
	RingItem* item = &sendRing->buff[sendRing->tx_head];
	//Get the length of the payload from the element buffer with the index given by the item.
	int length = sky_element_buffer_get_data_length(elementBuffer, item->idx);
	//Write the length and the sequence to the pointers.
	*sequence = item->sequence;
	return length;
}


//Clears the tail of the ring up to the sequence given by new_tail_sequence. Returns the number of payloads cleared or a negative error code.
int sendRing_clean_tail_up_to(SkySendRing *sendRing, SkyElementBuffer *elementBuffer, sky_arq_sequence_t new_tail_sequence)
{
	//Cannot move tail ahead of the tx_head. (First untransmitted packet)
	sky_arq_sequence_t tx_ahead_of_tail = sendRing->tx_sequence - sendRing->tail_sequence;
	sky_arq_sequence_t peer_head_ahead_of_tail = new_tail_sequence - sendRing->tail_sequence;
	if (peer_head_ahead_of_tail > tx_ahead_of_tail) // Attempt to ack sequences that have not been sent.
		return SKY_RET_RING_INVALID_ACKNOWLEDGE;

	int n_cleared = 0;
	//Loop through the ring and clear everyting until the tail sequence is equal to the new tail sequence.
	while (sendRing->tail_sequence != new_tail_sequence) {
		//Get the item at the tail.
		RingItem* tail_item = &sendRing->buff[sendRing->tail];
		//Delete the payload from the element buffer that is associated with the item.
		sky_element_buffer_delete(elementBuffer, tail_item->idx);
		//Reset the item.
		tail_item->idx = EB_NULL_IDX;
		tail_item->sequence = 0;
		//Decrement the storage count and advance the tail.
		sendRing->storage_count--;
		sendRing->tail = ring_wrap(sendRing->tail+1, sendRing->length);
		sendRing->tail_sequence++; // Increment and the sequence will overflow naturally

		n_cleared++;
	}
	return n_cleared; //the number of payloads cleared.
}
//===== SEND RING ======================================================================================================
