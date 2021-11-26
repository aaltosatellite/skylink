//
// Created by elmore on 12.11.2021.
//

#ifndef SKYLINK_ARQ_RING_H
#define SKYLINK_ARQ_RING_H


#include "elementbuffer.h"
#include "conf.h"
#include "frame.h"
#include "packet_ring.h"




#define ARQ_STATE_OFF			0
#define ARQ_STATE_IN_INIT		1
#define ARQ_STATE_ON			2



// Notice: arrays are duplicated. This facilitates sequence number- and arq state switching on the fly without some absolutely brainmelting trickery.
// Otherwise the receive- and send-windowing, that relies on sequence numbering, would have to be able to handle discontinuities
// in sequences when needed, while discontinuities are the sole thing they are set to battle.
typedef struct {
	ElementBuffer* elementBuffer;
	SkySendRing* sendRing;
	SkyRcvRing* rcvRing;

	uint8_t arq_state_flag;
	uint8_t handshake_send;
	uint32_t arq_session_identifier;
	uint8_t need_recall;
	int32_t last_tx_ms;
	int32_t last_rx_ms;
	int32_t last_ctrl_send;
} SkyArqRing;








// The obvious...
SkyArqRing* new_arq_ring(SkyArrayConfig* config);
void destroy_arq_ring(SkyArqRing* array);

// Cleans the rings, deletes all the packets from buffer, and initalizes to given sequence numbers.
void skyArray_wipe_to_arq_off_state(SkyArqRing* array);

// (A) >> ( )
int skyArray_wipe_to_arq_init_state(SkyArqRing* array, int32_t now_ms);

// ---
void skyArray_wipe_to_arq_on_state(SkyArqRing* array, uint32_t identifier, int32_t now_ms);

// asd
int skyArray_handle_handshake(SkyArqRing* array, uint8_t peer_state, uint32_t identifier, int32_t now_ms);

// asd
void skyArray_poll_arq_state_timeout(SkyArqRing* array, int32_t now_ms, int32_t timeout_ms);




//=== SEND =============================================================================================================
//======================================================================================================================
// Push packet to buffer. Return the save address index, or -1.
int skyArray_push_packet_to_send(SkyArqRing* array, void* payload, int length);

// Returns boolean 1/0 wether the send ring is full.
int skyArray_send_buffer_is_full(SkyArqRing* array);

// Reads next message to be sent.
int skyArray_read_packet_for_tx(SkyArqRing* array, void* tgt, int* sequence, int include_resend);

// Returns the number of messages in buffer.
int skyArray_count_packets_to_tx(SkyArqRing* array, int include_resend);

// Return boolean wether a message of particular sequence is still recallable.
int skyArray_can_recall(SkyArqRing* array, int sequence);

// Schedules packet of a sequence to be resent. Returns 0/-1 according to if the packet was recallable.
int skyArray_schedule_resend(SkyArqRing* arqRing, int sequence);

// This is called with the head-rx sequence provided by an arq-control-extension
void skyArray_update_tx_sync(SkyArqRing* array, int peer_rx_head_sequence_by_ctrl, int32_t now_ms);

// Fills the length and sequence of the next packet to be transmitted, if the ring is not empty. Returns 0/errorcode.
int skyArray_peek_next_tx_size_and_sequence(SkyArqRing* array, int include_resend, int* length, int* sequence);

//-----
int skyArray_content_to_send(SkyArqRing* array, SkyConfig* config, int32_t now_ms, uint16_t frames_sent_in_this_vc_window);

//-----
int skyArray_fill_frame(SkyArqRing* array, SkyConfig* config, SkyRadioFrame* frame, int32_t now_ms, uint16_t frames_sent_in_this_vc_window);
//======================================================================================================================
//======================================================================================================================



//=== RECEIVE ==========================================================================================================
//======================================================================================================================
// Pushes latest radio received message in without ARQ. Fills in the next sequence.
int skyArray_push_rx_packet_monotonic(SkyArqRing* array, void* src, int length);

// Pushes a radio received message of particular sequence to buffer.
int skyArray_push_rx_packet(SkyArqRing* array, void* src, int length, int sequence, int32_t now_ms);

// Read next message to tgt buffer. Return number of bytes written on success, -1 on fail.
int skyArray_read_next_received(SkyArqRing* array, void* tgt, int* sequence);

// How many messages there are in buffer as a continuous sequence, an thus readable by skyArray_read_next_received()
int skyArray_count_readable_rcv_packets(SkyArqRing* array);

// This is called with the head-tx sequence provided by an arq-control-extension
void skyArray_update_rx_sync(SkyArqRing* array, int peer_tx_head_sequence_by_ctrl, int32_t now_ms);

//-----
void skyArray_process_content(SkyArqRing* array,
							  void* pl,
							  int len_pl,
							  SkyPacketExtension* ext_seq,
							  SkyPacketExtension* ext_ctrl,
							  SkyPacketExtension* ext_handshake,
							  SkyPacketExtension* ext_rrequest,
							  timestamp_t now_ms);
//======================================================================================================================
//======================================================================================================================


#endif //SKYLINK_ARQ_RING_H
