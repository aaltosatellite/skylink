//
// Created by elmore on 12.11.2021.
//

#ifndef SKYLINK_RELIABLE_VC_H
#define SKYLINK_RELIABLE_VC_H


#include "elementbuffer.h"
#include "conf.h"
#include "frame.h"
#include "sequence_ring.h"




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
} SkyVirtualChannel;








// The obvious...
SkyVirtualChannel* new_arq_ring(SkyVCConfig* config);
void destroy_arq_ring(SkyVirtualChannel* array);

// Cleans the rings, deletes all the packets from buffer, and initalizes to given sequence numbers.
void sky_vc_wipe_to_arq_off_state(SkyVirtualChannel* array);

// (A) >> ( )
int sky_vc_wipe_to_arq_init_state(SkyVirtualChannel* array, int32_t now_ms);

// ---
void sky_vc_wipe_to_arq_on_state(SkyVirtualChannel* array, uint32_t identifier, int32_t now_ms);

// asd
int sky_vc_handle_handshake(SkyVirtualChannel* array, uint8_t peer_state, uint32_t identifier, int32_t now_ms);

// asd
void sky_vc_poll_arq_state_timeout(SkyVirtualChannel* array, int32_t now_ms, int32_t timeout_ms);




//=== SEND =============================================================================================================
//======================================================================================================================
// Push packet to buffer. Return the save address index, or -1.
int sky_vc_push_packet_to_send(SkyVirtualChannel* array, void* payload, int length);

// Returns boolean 1/0 wether the send ring is full.
int sky_vc_send_buffer_is_full(SkyVirtualChannel* array);

// Reads next message to be sent.
int sky_vc_read_packet_for_tx(SkyVirtualChannel* array, void* tgt, int* sequence, int include_resend);

// Returns the number of messages in buffer.
int sky_vc_count_packets_to_tx(SkyVirtualChannel* array, int include_resend);

// Return boolean wether a message of particular sequence is still recallable.
int sky_vc_can_recall(SkyVirtualChannel* array, int sequence);

// Schedules packet of a sequence to be resent. Returns 0/-1 according to if the packet was recallable.
int sky_vc_schedule_resend(SkyVirtualChannel* arqRing, int sequence);

// This is called with the head-rx sequence provided by an arq-control-extension
void sky_vc_update_tx_sync(SkyVirtualChannel* array, int peer_rx_head_sequence_by_ctrl, int32_t now_ms);

// Fills the length and sequence of the next packet to be transmitted, if the ring is not empty. Returns 0/errorcode.
int sky_vc_peek_next_tx_size_and_sequence(SkyVirtualChannel* array, int include_resend, int* length, int* sequence);

//-----
int sky_vc_content_to_send(SkyVirtualChannel* array, SkyConfig* config, int32_t now_ms, uint16_t frames_sent_in_this_vc_window);

//-----
int sky_vc_fill_frame(SkyVirtualChannel* array, SkyConfig* config, SkyRadioFrame* frame, int32_t now_ms, uint16_t frames_sent_in_this_vc_window);
//======================================================================================================================
//======================================================================================================================



//=== RECEIVE ==========================================================================================================
//======================================================================================================================
// Pushes latest radio received message in without ARQ. Fills in the next sequence.
int sky_vc_push_rx_packet_monotonic(SkyVirtualChannel* array, void* src, int length);

// Pushes a radio received message of particular sequence to buffer.
int sky_vc_push_rx_packet(SkyVirtualChannel* array, void* src, int length, int sequence, int32_t now_ms);

// Read next message to tgt buffer. Return number of bytes written on success, -1 on fail.
int sky_vc_read_next_received(SkyVirtualChannel* array, void* tgt, int* sequence);

// How many messages there are in buffer as a continuous sequence, an thus readable by sky_vc_read_next_received()
int sky_vc_count_readable_rcv_packets(SkyVirtualChannel* array);

// This is called with the head-tx sequence provided by an arq-control-extension
void sky_vc_update_rx_sync(SkyVirtualChannel* array, int peer_tx_head_sequence_by_ctrl, int32_t now_ms);

//-----
void sky_vc_process_content(SkyVirtualChannel* array,
							void* pl,
							int len_pl,
							SkyPacketExtension* ext_seq,
							SkyPacketExtension* ext_ctrl,
							SkyPacketExtension* ext_handshake,
							SkyPacketExtension* ext_rrequest,
							timestamp_t now_ms);
//======================================================================================================================
//======================================================================================================================


#endif //SKYLINK_RELIABLE_VC_H
