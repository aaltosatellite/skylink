//
// Created by elmore on 14.10.2021.
//

#ifndef SKYLINK_SKYLINK_ARRAY_H
#define SKYLINK_SKYLINK_ARRAY_H

#include "elementbuffer.h"
#include "skylink/platform.h"
#include "skylink/diag.h"


//todo: define sequences as a new type (uint16 probably?) This would save 2*lots of bytes
struct sky_ring_item_s {
	idx_t idx;
	int sequence;
};
typedef struct sky_ring_item_s RingItem;



struct sky_send_ring_s {
	RingItem* buff;					//The ring.
	int length;						//Size of the ring
	int head;						//Ring index for next packet pushed by upper stack.
	int tx_head;					//The first untransmitted packet. (if tx_head == head, nothing to transmit)
	int tail;						//Last packet in memory. Moves forward as more is sent, lags behind n_reacall at max.
	int head_sequence;				//Sequence of the next packet, if ARQ enabled.
	int tx_sequence;				//sequence of the first untransmitted packet. That is, the packet pointed to by tx_head.
	int tail_sequence;				//sequence of the packet at tail. Essentially "ring[wrap(tail)].sequence".
	int n_recall;					//The max separation between tx_head and tail. (tx_sequence - n_recall) is the earliest packet that can be retransmitted by ARQ.
};
typedef struct sky_send_ring_s SkySendRing;


struct sky_rcv_ring_s {
	RingItem* buff;					//The ring.
	int length;						//Size of the ring
	int horizon_width;				// Width of out of order buffer. Up to this many+1 sequence numbers are accepted.
									// In situation where head_sequence = 5, and horizon_width = 3, we have packet 4, but not 5.
									// We accept packts 5,6,7,8. We reject 9 and all with higher sequence number.
									// (head_sequence + horizon_width) = last sequence number that will be accepted.
	int head;						//Ring index after packet up to which all packets have been received. At a vacant slot, when rcv is not choked*.
	int tail;						//Ring index AT last unread received packet. Moves forward as the upper stack reads from this. tail=head means empty.

	int head_sequence;				//Sequence number that a packet at head would have. Essentially "ring[wrap(head-1)].sequence + 1"
	int tail_sequence;				//Sequence number of tail.
};
typedef struct sky_rcv_ring_s SkyRcvRing;
/*
 * *RCV choke: A situation can occur where rcv-ring's horizon catches up with the tail. Due to how the head advances,
 * it can happen that if head is at 7, and we have packets 9 and 10, receiving packet 8 would advance the head by
 * 3 units, which could bring the horizon 3 units over tail. This would be bad.
 *
 * A number of ways to handle this exist:
 * #1: Horizon shrinks to avoid stomping over tail. This is complicated, and means that horizon length is not constant.
 * We would gain the benefit that head can move a little bit closer to tail, but this is useless since reading happens
 * from tail, and once we catch up with the head, it is free to move again.
 *
 * #2: The horizon window stops moving. This means that the horizon window retains it's original size, but there might
 * be received packets at or ahead of head. The side effect is that read-operation has to check if the head is at choked state,
 * and advance it manually if needed.
 *
 * Choise #2 was picked.
 */


struct skylink_array_s {
	ElementBuffer* elementBuffer;
	SkyRcvRing* rcvRing;
	SkySendRing* sendRing;
	uint8_t arq_on;
};
typedef struct skylink_array_s SkylinkArray;


struct sky_array_config {
	int element_size;
	int element_count;

	int rcv_ring_length;
	int horizon_width;

	int send_ring_length;
	int n_recall;

	uint8_t arq_on;
};
typedef struct sky_array_config SkyArrayConfig;


struct sky_array_alloc{
	SkylinkArray* skylinkArray;

	SkySendRing* sendRing;
	RingItem* sendbuff;

	SkyRcvRing* rcvRing;
	RingItem* rcvbuff;

	ElementBuffer* elementBuffer;
	uint8_t* element_pool;
};
typedef struct sky_array_alloc SkyArrayAllocation;


int init_send_ring(SkySendRing* sendRing, RingItem* ring, int length, int n_recall);
SkySendRing* new_send_ring(int length, int n_recall);
void destroy_send_ring(SkySendRing* sendRing);
void wipe_rcv_ring(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer);

int init_rcv_ring(SkyRcvRing* rcvRing, RingItem* ring, int length, int horizon_width);
SkyRcvRing* new_rcv_ring(int length, int horizon_width);
void destroy_rcv_ring(SkyRcvRing* rcvRing);
void wipe_send_ring(SkySendRing* sendRing, ElementBuffer* elementBuffer);

int init_skylink_array(SkyArrayAllocation* alloc, SkyArrayConfig* conf);
SkylinkArray* new_skylink_array(int element_size, int element_count, int rcv_ring_leng, int send_ring_len, uint8_t arq_on, int n_recall, int horizon_width);
void destroy_skylink_array(SkylinkArray* array);
void wipe_skylink_array(SkylinkArray* array);





//=== SEND =============================================================================================================
//======================================================================================================================
uint16_t skyArray_get_horizon_bitmap(SkylinkArray* array);

int skyArray_push_packet_to_send(SkylinkArray* array, void* payload, int length);

int skyArray_packets_to_tx(SkylinkArray* array);

int skyArray_read_packet_for_tx(SkylinkArray* array, void* tgt);

int skyArray_can_recall(SkylinkArray* array, int sequence);

int skyArray_recall(SkylinkArray* array, int sequence, void* tgt);
//======================================================================================================================
//======================================================================================================================




//=== RECEIVE ==========================================================================================================
//======================================================================================================================
int skyArray_get_readable_rcv_packet_count(SkylinkArray* array);

int skyArray_read_next_received(SkylinkArray* array, void* tgt, int* sequence);

int skyArray_rx_sequence_fits(SkylinkArray* array, int sequence);

int skyArray_push_rx_packet(SkylinkArray* array, void* src, int length, int sequence);
//======================================================================================================================
//======================================================================================================================




#endif //SKYLINK_SKYLINK_ARRAY_H
