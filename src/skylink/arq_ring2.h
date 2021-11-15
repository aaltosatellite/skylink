//
// Created by elmore on 12.11.2021.
//

#ifndef SKYLINK_ARQ_RING2_H
#define SKYLINK_ARQ_RING2_H



#include "elementbuffer.h"
#include "conf.h"



#define ARQ_SEQUENCE_MODULO 			250
#define SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE	260
#define ARQ_MAXIMUM_HORIZON				16
#define ARQ_RESEND_SCHEDULE_DEPTH		16
#define ARQ_TIMEOUT_MS					5000 //todo: this should be in config struct?


#define ARQ_STATE_OFF		0
#define ARQ_STATE_IN_INIT	1
#define ARQ_STATE_ON		2


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
	uint16_t resend_list[ARQ_RESEND_SCHEDULE_DEPTH];
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
typedef struct {
	ElementBuffer* elementBuffer;
	SkySendRing* sendRing;
	SkyRcvRing* rcvRing;
	//uint8_t state_enforcement_need;
	//uint8_t resend_request_need;

	uint8_t arq_state;
	uint8_t need_recall;
	int32_t last_tx_ms;
	int32_t last_rx_ms;
} SkyArqRing2;






int sequence_wrap(int sequence);

// The obvious...
SkyArqRing2* new_arq_ring(SkyArrayConfig* config);
void destroy_arq_ring(SkyArqRing2* array);

// Cleans the rings, deletes all the packets from buffer, and initalizes to given sequence numbers.
void wipe_arq_ring(SkyArqRing2* array, int new_send_sequence, int new_rcv_sequence);

// (A) >> ( )
int skyArray_begin_arq_handshake(SkyArqRing2* array, int32_t now_ms);

// ( ) << (B)
int skyArray_arq_handshake_received(SkyArqRing2* array, int32_t now_ms);

// (C) -- ( )
int skyArray_arq_handshake_complete(SkyArqRing2* array, int32_t now_ms);

// Ar
void skyArray_poll_arq_state_timeout(SkyArqRing2* array, int32_t now_ms);




//=== SEND =============================================================================================================
//======================================================================================================================
// Push packet to buffer. Return the save address index, or -1.
int skyArray_push_packet_to_send(SkyArqRing2* array, void* payload, int length);

// Reads next message to be sent.
int skyArray_read_packet_for_tx(SkyArqRing2* array, void* tgt, int* sequence, int include_resend);

// Returns the number of messages in buffer.
int skyArray_count_packets_to_tx(SkyArqRing2* array, int include_resend);

// Return boolean wether a message of particular sequence is still recallable.
int skyArray_can_recall(SkyArqRing2* array, int sequence);

// Schedules packet of a sequence to be resent. Returns 0/-1 according to if the packet was recallable.
int skyArray_schedule_resend(SkyArqRing2* arqRing, int sequence);

// Returns the length of the next payload that would be read from transmit ring. -1 if there is nothing to read.
int skyArray_peek_next_tx_size(SkyArqRing2* arqRing, int include_resend);

// Return the sequence number that the next transmitted packet will have, save for unexpected intervening sequence resets.
int skyArray_get_next_transmitted_sequence(SkyArqRing2* arqRing);

// This is called with the head-rx sequence provided by an arq-control-extension
void skyArray_update_tx_sync(SkyArqRing2* array, int peer_rx_head_sequence_by_ctrl, int32_t now_ms);
//======================================================================================================================
//======================================================================================================================



//=== RECEIVE ==========================================================================================================
//======================================================================================================================
// Pushes latest radio received message in without ARQ. Fills in the next sequence.
int skyArray_push_rx_packet_monotonic(SkyArqRing2* array, void* src, int length);

// Pushes a radio received message of particular sequence to buffer.
int skyArray_push_rx_packet(SkyArqRing2* array, void* src, int length, int sequence, int32_t now_ms);

// Read next message to tgt buffer. Return number of bytes written on success, -1 on fail.
int skyArray_read_next_received(SkyArqRing2* array, void* tgt, int* sequence);

// How many messages there are in buffer as a continuous sequence, an thus readable by skyArray_read_next_received()
int skyArray_count_readable_rcv_packets(SkyArqRing2* array);

// Bitmap of which messages we have received after the last continuous sequence.
uint16_t skyArray_get_horizon_bitmap(SkyArqRing2* array);

// This is called with the head-tx sequence provided by an arq-control-extension
void skyArray_update_rx_sync(SkyArqRing2* array, int peer_tx_head_sequence_by_ctrl, int32_t now_ms);
//======================================================================================================================
//======================================================================================================================


#endif //SKYLINK_ARQ_RING2_H
