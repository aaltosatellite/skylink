#ifndef __SKYLINK_RELIABLE_VC_H__
#define __SKYLINK_RELIABLE_VC_H__


#include "skylink/elementbuffer.h"
#include "skylink/conf.h"
#include "skylink/frame.h"
#include "skylink/sequence_ring.h"




#define ARQ_STATE_OFF			0
#define ARQ_STATE_IN_INIT		1
#define ARQ_STATE_ON			2



struct sky_virtual_channel {
	ElementBuffer* elementBuffer;		// Storage structure shared by the sendRing and rcvRing.
	SkySendRing* sendRing;				// Sequence ring tracking sent payloads and their sequence numbering.
	SkyRcvRing* rcvRing;				// Sequence ring tracking received payloads and their sequence numbering.

	uint8_t arq_state_flag;				// A flag with 3 valid states: OFF/INIT/ON.
	uint8_t handshake_send;				// A flag set to indicate a need to send a handshake extension on next transmission window.
	uint32_t arq_session_identifier;	// A unique identifier of the current arq session, if arq is on.
	uint8_t need_recall;				// A flag set to indicate a need to send rend recall extension. *1
	tick_t last_tx_tick;				// Tick of last time peer confirmed new payloads received, or being in sync with us.
	tick_t last_rx_tick;				// Tick of last time a new continuous payloads was received, or we confirmed sync with peer.
	tick_t last_ctrl_send_tick;		// Tick of last time a control extension was transmitted.
	int16_t unconfirmed_payloads;
};
typedef struct sky_virtual_channel SkyVirtualChannel;
// *1 In the case where a received control extension reveals that the latest received payload is not the latest
// the peer has sent, we need to recall this packet, despite our horizon being empty.






// The obvious...
SkyVirtualChannel* new_virtual_channel(SkyVCConfig* config);
void destroy_virtual_channel(SkyVirtualChannel* vchannel);

// Cleans the rings, deletes all the packets from buffer, and initalizes to given sequence numbers.
void sky_vc_wipe_to_arq_off_state(SkyVirtualChannel* vchannel);

// FG
int sky_vc_wipe_to_arq_init_state(SkyVirtualChannel* vchannel);

// Sets the virtual channel to reliable transmission state
void sky_vc_wipe_to_arq_on_state(SkyVirtualChannel* vchannel, uint32_t identifier);

// Processes a handshake received in a packet.
int sky_vc_handle_handshake(SkyVirtualChannel* vchannel, uint8_t peer_state, uint32_t identifier);

// If too much time has passed since previous successful communication, fall back to non-reliable state.
void sky_vc_poll_arq_state_timeout(SkyVirtualChannel* vchannel, tick_t now, tick_t timeout);




//=== SEND =============================================================================================================
//======================================================================================================================
// Push packet to buffer. Return the save address index, or -1.
int sky_vc_push_packet_to_send(SkyVirtualChannel* vchannel, void* payload, int length);

// Returns boolean 1/0 wether the send ring is full.
int sky_vc_send_buffer_is_full(SkyVirtualChannel* vchannel);

// Reads next message to be sent.
int sky_vc_read_packet_for_tx(SkyVirtualChannel* vchannel, void* tgt, int* sequence, int include_resend);

// Returns the number of messages in buffer.
int sky_vc_count_packets_to_tx(SkyVirtualChannel* vchannel, int include_resend);

// Return boolean wether a message of particular sequence is still recallable.
int sky_vc_can_recall(SkyVirtualChannel* vchannel, int sequence);

// Schedules packet of a sequence to be resent. Returns 0/-1 according to if the packet was recallable.
int sky_vc_schedule_resend(SkyVirtualChannel* arqRing, int sequence);

// This is called with the head-rx sequence provided by an arq-control-extension
void sky_vc_update_tx_sync(SkyVirtualChannel* vchannel, int peer_rx_head_sequence_by_ctrl, tick_t now);

// Fills the length and sequence of the next packet to be transmitted, if the ring is not empty. Returns 0/errorcode.
int sky_vc_peek_next_tx_size_and_sequence(SkyVirtualChannel* vchannel, int include_resend, int* length, int* sequence);

// Returns boolean 0/1 as to if there is content to be sent on this virtual channel.
int sky_vc_content_to_send(SkyVirtualChannel* vchannel, SkyConfig* config, tick_t now, uint16_t frames_sent_in_this_vc_window);

// Fills the frame with a packet if there is something to send. Returns boolean 0/1 as to if it actually wrote a frame.
int sky_vc_fill_frame(SkyVirtualChannel* vchannel, SkyConfig* config, SkyRadioFrame* frame, tick_t now, uint16_t frames_sent_in_this_vc_window);
//======================================================================================================================
//======================================================================================================================



//=== RECEIVE ==========================================================================================================
//======================================================================================================================
// Pushes latest radio received message in without ARQ. Fills in the next sequence.
int sky_vc_push_rx_packet_monotonic(SkyVirtualChannel* vchannel, const void* src, int length);

// Pushes a radio received message of particular sequence to buffer.
int sky_vc_push_rx_packet(SkyVirtualChannel* vchannel, const void* src, int length, int sequence, tick_t now);

// Read next message to tgt buffer. Return number of bytes written on success, or negative error code.
int sky_vc_read_next_received(SkyVirtualChannel* vchannel, void* tgt, int max_length);

// How many messages there are in buffer as a continuous sequence, an thus readable by sky_vc_read_next_received()
int sky_vc_count_readable_rcv_packets(SkyVirtualChannel* vchannel);

// This is called with the head-tx sequence provided by an arq-control-extension
void sky_vc_update_rx_sync(SkyVirtualChannel* vchannel, int peer_tx_head_sequence_by_ctrl, tick_t now);

// Processes the content
int sky_vc_process_content(SkyVirtualChannel *vchannel,
							const uint8_t *payload,
							int payload_len,
							SkyParsedExtensions *exts,
							tick_t now);
//======================================================================================================================
//======================================================================================================================


#endif //__SKYLINK_RELIABLE_VC_H__
