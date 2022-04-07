//
// Created by elmore on 20.11.2021.
//

#ifndef SKYLINK_SEQUENCE_RING_H
#define SKYLINK_SEQUENCE_RING_H

#include "diag.h"
#include "elementbuffer.h"
#include "conf.h"
#include "frame.h"

#ifdef SKY_DEBUG
#define ARQ_SEQUENCE_MODULO 			517
#else
#define ARQ_SEQUENCE_MODULO 			32000
#endif
#define ARQ_RESEND_SCHEDULE_DEPTH		16
#define ARQ_MAXIMUM_HORIZON				128


//todo: define sequences as a new type (uint16 probably?) This would save 2*lots of bytes
struct sky_ring_item_s {
	idx_t idx;
	arq_seq_t sequence;
}__attribute__((__packed__));
typedef struct sky_ring_item_s RingItem;


struct sky_send_ring_s {
	RingItem* buff;					// The ring.
	int length;						// Size of the ring
	int head;						// Ring index next packet pushed by upper stack will obtain.
	int tx_head;					// The first untransmitted packet. (if tx_head == head, nothing to transmit)
	int tail;						// Last packet in memory. Moves forward as sent packets are acknowledged as received. (if tail == head, ring is empty)
	int head_sequence;				// Sequence of the next packet (head).
	int tx_sequence;				// sequence of the first untransmitted packet. That is, the packet pointed to by tx_head.
	int tail_sequence;				// sequence of the packet at tail. Essentially "ring[wrap(tail)].sequence".
	int storage_count;				// Number of packets stored.
	int resend_count;				// Number of sequence numbers scheduled for resend.
	arq_seq_t resend_list[ARQ_RESEND_SCHEDULE_DEPTH];
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


int wrap_sequence(int sequence);




/* The obvious... */
SkyRcvRing* new_rcv_ring(int length, int horizon_width, int initial_sequence);
void destroy_rcv_ring(SkyRcvRing* rcvRing);
void wipe_rcv_ring(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer, int initial_sequence);

/* Returns the amound of packets that can be red from the ring. (>=0) */
int rcvRing_count_readable_packets(SkyRcvRing* rcvRing);

/* Reads a payload from ring to address pointed by tgt, if it's length is less than max_length.
 * Returns number of bytes read, or negative error code.
 * If length is more than max_length, the payload is dropped.
 */
int rcvRing_read_next_received(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer, void* tgt, int max_length);

/* Pushes a payload received with "sequence". Returns how many steps the head advances (>=0) or negative error code. */
int rcvRing_push_rx_packet(SkyRcvRing* rcvRing, ElementBuffer* elementBuffer, void* src, int length, int sequence);

/* Constructs a bitmap of horizon where 0 represents a missing packet, and 1 a packet that is present in the horizon. So perfectly clear state is 0 */
int rcvRing_get_horizon_bitmap(SkyRcvRing* rcvRing);

/* Returns a state code as to is the RcvRing "has received all packets the peer has sent", "is some packets behind" or "is way out of sync, irreversiby so" */
int rcvRing_get_sequence_sync_status(SkyRcvRing* rcvRing, int peer_tx_head_sequence_by_ctrl);



/* The obvious... */
void wipe_send_ring(SkySendRing* sendRing, ElementBuffer* elementBuffer, int initial_sequence);
SkySendRing* new_send_ring(int length, int initial_sequence);
void destroy_send_ring(SkySendRing* sendRing);

/* Returns boolean 1/0 wether the ring is full. (Full: tail == head+1. "Push_packet_to_send" will fail) */
int sendRing_is_full(SkySendRing* sendRing);

/* Wether a particular sequence can be scheduled for retransmission. Returns 1 if yes, 0 if not. So boolean. */
int sendRing_can_recall(SkySendRing* sendRing, int sequence);

/* Returns the ring index of a particular index, of negative error if the sequence is not present. */
int sendRing_get_recall_ring_index(SkySendRing* sendRing, int recall_sequence);

/* Pushes a new packet to be sent. Returns the (nonnegative) sequence it is associated with, or a negative error code. */
int sendRing_push_packet_to_send(SkySendRing* sendRing, ElementBuffer* elementBuffer, void* payload, int length);

/* Schedules a particular sequence to be resent (if possible). Returns 0 if successful, negative error code otherwise. */
int sendRing_schedule_resend(SkySendRing* sendRing, int sequence);

/* Schedules resend for the sequence argument, and all the sequences pointed to by the mask if possible. */
int sendRing_schedule_resends_by_mask(SkySendRing* sendRing, int sequence, uint16_t mask);

/* Calculates the number of free ring slots for packets. */
int sendRing_count_free_send_slots(SkySendRing* sendRing);

/* Returns the number of packets that wait sending (>=0) */
int sendRing_count_packets_to_send(SkySendRing* sendRing, int include_resend);

/* Reads a payload to target. Writes sequence number to "sequence" pointer. Returns the (nonnegative) number of bytes read, or negative errotr code. */
int sendRing_read_to_tx(SkySendRing* sendRing, ElementBuffer* elementBuffer, void* tgt, int* sequence, int include_resend);

/* Writes sequence and length of the next payload to be sent into according pointer aguments. Returns 0 on success, negative error code otherwise. */
int sendRing_peek_next_tx_size_and_sequence(SkySendRing* sendRing, ElementBuffer* elementBuffer, int include_resend, int* size, int* sequence);

/* Deletes all payloads with sequences up to "new_tail_sequences". Accordingly, tail moves up to this sequence.
 * Returns the number of steps tail advances, or a negative errorcode if the sequence was not between tail and tx_head. */
int sendRing_clean_tail_up_to(SkySendRing* sendRing, ElementBuffer* elementBuffer, int new_tail_sequence);




#endif //SKYLINK_SEQUENCE_RING_H


