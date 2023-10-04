#ifndef __SKYLINK_SEQUENCE_RING_H__
#define __SKYLINK_SEQUENCE_RING_H__

#include "skylink/skylink.h"

/* */
#define ARQ_RESEND_SCHEDULE_DEPTH       16

/* */
#define ARQ_MAXIMUM_HORIZON             64


/* Sequence ring item */
typedef struct __attribute__((__packed__))
{
	// Element buffer position
	sky_element_idx_t idx;

	// ARQ sequence number
	sky_arq_sequence_t sequence;

} RingItem;

struct sky_send_ring_s
{
	RingItem* buff;     // The ring.
	int length;	        // Size of the ring
	int head;           // Ring index next packet pushed by upper stack will obtain.
	int tx_head;        // The first untransmitted packet. (if tx_head == head, nothing to transmit)
	int tail;           // Last packet in memory. Moves forward as sent packets are acknowledged as received. (if tail == head, ring is empty)
	int head_sequence;  // Sequence of the next packet (head).
	int tx_sequence;    // Sequence of the first untransmitted packet. That is, the packet pointed to by tx_head.
	int tail_sequence;  // Sequence of the packet at tail. Essentially "ring[wrap(tail)].sequence".
	int storage_count;  // Number of packets stored.
	unsigned int resend_count;   // Number of sequence numbers scheduled for resend.
	sky_arq_sequence_t resend_list[ARQ_RESEND_SCHEDULE_DEPTH];
};

struct sky_rcv_ring_s
{
	// The ring buffer memory allocation
	RingItem* buff;

	// Size of the ring buffer
	int length;

	// Width of out of order buffer. Up to this many+1 sequence numbers are accepted.
	// In a situation where head_sequence = 5, and horizon_width = 3, we have packet 4, but not 5.
	// We accept packts 5,6,7,8. We reject 9 and all with higher sequence number.
	// (head_sequence + horizon_width) = last sequence number that will be accepted.
	int horizon_width;

	// Ring index after packet up to which all packets have been received. At a vacant slot, when rcv is not choked*.
	int head;

	// Ring index AT last unread received packet. Moves forward as the upper stack reads from this. tail=head means empty.
	int tail;

	// Sequence number that a packet at head would have. Essentially "ring[wrap(head-1)].sequence + 1"
	sky_arq_sequence_t head_sequence;

	// Sequence number of tail
	sky_arq_sequence_t tail_sequence;

	// Number of packets stored.
	int storage_count;
};

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
 * Choice #2 was picked.
 */


/* Create a recieve ring instance */
SkyRcvRing *sky_rcv_ring_create(int length, int horizon_width, sky_arq_sequence_t initial_sequence);

/* Destroy a recieve ring. */
void sky_rcv_ring_destroy(SkyRcvRing* rcvRing);

/* Clear/Wipe all contents of the recieve ring. */
void sky_rcv_ring_wipe(SkyRcvRing *rcvRing, SkyElementBuffer *elementBuffer, sky_arq_sequence_t initial_sequence);

/* Returns the amount of packets that can be read from the ring. (>=0) */
int rcvRing_count_readable_packets(SkyRcvRing* rcvRing);

/* Reads a payload from ring to address pointed by 'target', if it's length is less than max_length.
 * Returns number of bytes read, or negative error code.
 * If length is more than max_length, the payload is dropped.
 */
int rcvRing_read_next_received(SkyRcvRing *rcvRing, SkyElementBuffer *elementBuffer, uint8_t *target, unsigned int max_length);

/* Pushes a payload received with "sequence". Returns how many steps the head advances (>=0) or negative error code. */
int rcvRing_push_rx_packet(SkyRcvRing *rcvRing, SkyElementBuffer *elementBuffer, const uint8_t *src, unsigned int length, sky_arq_sequence_t sequence);

/* Constructs a bitmap of horizon where 0 represents a missing packet, and 1 a packet that is present in the horizon. So perfectly clear state is 0 */
int rcvRing_get_horizon_bitmap(SkyRcvRing* rcvRing);

/* Returns a state code as to is the RcvRing "has received all packets the peer has sent", "is some packets behind" or "is way out of sync, irreversiby so" */
int rcvRing_get_sequence_sync_status(SkyRcvRing* rcvRing, sky_arq_sequence_t peer_tx_head_sequence_by_ctrl);



/* Create new send sequence number ring buffer */
SkySendRing *sky_send_ring_create(int length, sky_arq_sequence_t initial_sequence);

/* Destroy send ring */
void sky_send_ring_destroy(SkySendRing* sendRing);

/* Clear/wipe all the contents of the send ring */
void sky_send_ring_wipe(SkySendRing *sendRing, SkyElementBuffer *elementBuffer, sky_arq_sequence_t initial_sequence);

/* Returns boolean 1/0 whether the ring is full. (Full: tail == head+1. "Push_packet_to_send" will fail) */
int sendRing_is_full(SkySendRing* sendRing);

/* Whether a particular sequence can be scheduled for retransmission. Returns 1 if yes, 0 if not. So boolean. */
int sendRing_can_recall(SkySendRing *sendRing, sky_arq_sequence_t sequence);

/* Returns the ring index of a particular index, of negative error if the sequence is not present. */
int sendRing_get_recall_ring_index(SkySendRing *sendRing, sky_arq_sequence_t recall_sequence);

/* Pushes a new packet to be sent. Returns the (nonnegative) sequence it is associated with, or a negative error code. */
int sendRing_push_packet_to_send(SkySendRing* sendRing, SkyElementBuffer* elementBuffer, const uint8_t* payload, unsigned int length);

/* Schedules a particular sequence to be resent (if possible). Returns 0 if successful, negative error code otherwise. */
int sendRing_schedule_resend(SkySendRing *sendRing, sky_arq_sequence_t sequence);

/* Schedules resend for the sequence argument, and all the sequences pointed to by the mask if possible. */
int sendRing_schedule_resends_by_mask(SkySendRing *sendRing, sky_arq_sequence_t sequence, sky_arq_mask_t mask);

/* Calculates the number of free ring slots for packets. */
int sendRing_count_free_send_slots(SkySendRing* sendRing);

/* Returns the number of packets that wait sending (>=0) */
int sendRing_count_packets_to_send(SkySendRing* sendRing, int include_resend);

/* Reads a payload to target. Writes sequence number to "sequence" pointer. Returns the (nonnegative) number of bytes read, or a negative error code. */
int sendRing_read_to_tx(SkySendRing *sendRing, SkyElementBuffer *elementBuffer, uint8_t *target, sky_arq_sequence_t *sequence, int include_resend);

/* Writes sequence and length of the next payload to be sent into according pointer aguments. Returns 0 on success, negative error code otherwise. */
int sendRing_peek_next_tx_size_and_sequence(SkySendRing *sendRing, SkyElementBuffer *elementBuffer, int include_resend, sky_arq_sequence_t *sequence);

/* Deletes all payloads with sequences up to "new_tail_sequences". Accordingly, tail moves up to this sequence.
 * Returns the number of steps tail advances, or a negative errorcode if the sequence was not between tail and tx_head. */
int sendRing_clean_tail_up_to(SkySendRing *sendRing, SkyElementBuffer *elementBuffer, sky_arq_sequence_t new_tail_sequence);



#endif //__SKYLINK_SEQUENCE_RING_H__


