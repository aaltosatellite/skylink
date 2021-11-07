//
// Created by elmore on 14.10.2021.
//

#ifndef SKYLINK_ARQ_RING_H
#define SKYLINK_ARQ_RING_H


#include "elementbuffer.h"
#include "conf.h"




#define ARQ_SEQUENCE_MODULO 			250
#define SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE	260


//todo: define sequences as a new type (uint16 probably?) This would save 2*lots of bytes
struct sky_ring_item_s {
	idx_t idx;
	int16_t sequence;
};
typedef struct sky_ring_item_s RingItem;


struct sky_send_ring_s {
	RingItem* buff;					// The ring.
	int length;						// Size of the ring
	int n_recall;					// The max separation between tx_head and tail. (tx_sequence - n_recall) is the earliest packet that can be retransmitted by ARQ.
	int head;						// Ring index for next packet pushed by upper stack.
	int tx_head;					// The first untransmitted packet. (if tx_head == head, nothing to transmit)
	int tail;						// Last packet in memory. Moves forward as more is sent, lags behind n_reacall at max.
	int head_sequence;				// Sequence of the next packet, if ARQ enabled.
	int tx_sequence;				// sequence of the first untransmitted packet. That is, the packet pointed to by tx_head.
	int tail_sequence;				// sequence of the packet at tail. Essentially "ring[wrap(tail)].sequence".
	int storage_count;				// Number of packets stored.
	int resend_count;
	uint8_t resend_list[16];
};
typedef struct sky_send_ring_s SkySendRing;


struct sky_rcv_ring_s {
	RingItem* buff;					// The ring.
	int length;						// Size of the ring
	int horizon_width;				// Width of out of order buffer. Up to this many+1 sequence numbers are accepted.
									// In situation where head_sequence = 5, and horizon_width = 3, we have packet 4, but not 5.
									// We accept packts 5,6,7,8. We reject 9 and all with higher sequence number.
									// (head_sequence + horizon_width) = last sequence number that will be accepted.
	int head;						// Ring index after packet up to which all packets have been received. At a vacant slot, when rcv is not choked*.
	int tail;						// Ring index AT last unread received packet. Moves forward as the upper stack reads from this. tail=head means empty.

	int head_sequence;				// Sequence number that a packet at head would have. Essentially "ring[wrap(head-1)].sequence + 1"
	int tail_sequence;				// Sequence number of tail.

	int storage_count;				// Number of packets stored.
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


// Notice: arrays are duplicated. This facilitates sequence number- and arq state switching on the fly without some absolutely brainmelting trickery.
// Otherwise the receive- and send-windowing, that relies on sequence numbering, would have to be able to handle discontinuities
// in sequences when needed, while discontinuities are the sole thing they are set to battle.
struct arq_ring_s {
	ElementBuffer* elementBuffer;
	SkySendRing* primarySendRing;
	SkyRcvRing* primaryRcvRing;
	SkyRcvRing* secondaryRcvRing;
	uint8_t state_enforcement_need;
	uint8_t resend_request_need;
};
typedef struct arq_ring_s SkyArqRing;







int sequence_wrap(int sequence);

SkyArqRing* new_arq_ring(SkyArrayConfig* config);
void destroy_arq_ring(SkyArqRing* array);
void wipe_arq_ring(SkyArqRing* array, int new_send_sequence, int new_rcv_sequence);

//swaps the send rings, and wipes the new primary ring to the new sequence.
int skyArray_set_send_sequence(SkyArqRing* array, uint16_t sequence);

//swaps the recive rings, and wipes the new primary ring to the new sequence.
int skyArray_set_receive_sequence(SkyArqRing* array, uint16_t sequence, int wipe_all);

//If the secondary rings contain stuff, but are inactive, frees the space held by them (wipes them)
void skyArray_clean_unreachable(SkyArqRing* array);




//=== SEND =============================================================================================================
//======================================================================================================================
//push packet to buffer. Return the save address index, or -1.
int skyArray_push_packet_to_send(SkyArqRing* array, void* payload, int length);

//reads next message to be sent.
int skyArray_read_packet_for_tx(SkyArqRing* array, void* tgt, int* sequence, int include_resend);

//returns the number of messages in buffer.
int skyArray_count_packets_to_tx(SkyArqRing* array, int include_resend);

//return boolean wether a message of particular sequence is still recallable.
int skyArray_can_recall(SkyArqRing* array, int sequence);

//schedules packet of a sequence to be resent. Returns 0/-1 according to if the packet was recallable.
int skyArray_schedule_resend(SkyArqRing* arqRing, int sequence);

//returns the length of the next payload that would be read from transmit ring. -1 if there is nothing to read.
int skyArray_peek_next_tx_size(SkyArqRing* arqRing, int include_resend);

//return the sequence number that the next transmitted packet will have, save for unexpected intervening sequence resets.
int skyArray_get_next_transmitted_sequence(SkyArqRing* arqRing);
//======================================================================================================================
//======================================================================================================================



//=== RECEIVE ==========================================================================================================
//======================================================================================================================
//pushes latest radio received message in without ARQ. Fills in the next sequence.
int skyArray_push_rx_packet_monotonic(SkyArqRing* array, void* src, int length);

//pushes a radio received message of particular sequence to buffer.
int skyArray_push_rx_packet(SkyArqRing* array, void* src, int length, int sequence);

//read next message to tgt buffer. Return number of bytes written on success, -1 on fail.
int skyArray_read_next_received(SkyArqRing* array, void* tgt, int* sequence);

//how many messages there are in buffer as a continuous sequence, an thus readable by skyArray_read_next_received()
int skyArray_count_readable_rcv_packets(SkyArqRing* array);

//bitmap of which messages we have received after the last continuous sequence.
uint16_t skyArray_get_horizon_bitmap(SkyArqRing* array);
//======================================================================================================================
//======================================================================================================================



#endif //SKYLINK_ARQ_RING_H
