
#include "skylink/skylink.h"
#include "skylink/reliable_vc.h"
#include "skylink/sequence_ring.h"
#include "skylink/element_buffer.h"
#include "skylink/frame.h"
#include "skylink/diag.h"
#include "skylink/utilities.h"

#include "sky_platform.h"

void sky_get_state(SkyHandle self, SkyState* state) {

	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++) {
		SkyVirtualChannel* vc = self->virtual_channels[i];
		SkyVCState* vc_state = &state->vc[i];

		vc_state->state = vc->arq_state_flag;
		vc_state->free_tx_slots = sendRing_count_free_send_slots(vc->sendRing);
		vc_state->tx_frames = sendRing_count_packets_to_send(vc->sendRing, 1);
		vc_state->rx_frames = rcvRing_count_readable_packets(vc->rcvRing);
		vc_state->session_identifier = vc->arq_session_identifier;
	}

}


static int compute_required_element_count(int elementsize, int total_ring_slots, int maximum_pl_size){
	int32_t n_per_pl = sky_element_buffer_element_requirement(elementsize, maximum_pl_size);
	int32_t required = (total_ring_slots * n_per_pl);
	return required;
}



//===== SKYLINK VIRTUAL CHANNEL  =======================================================================================
SkyVirtualChannel* sky_vc_create(SkyVCConfig* config) {

	if (config->rcv_ring_len < 6 || config->rcv_ring_len > 250)
		config->rcv_ring_len = 32;
	if(config->horizon_width > config->rcv_ring_len - 3)
		config->horizon_width = config->rcv_ring_len - 3;
	if (config->send_ring_len < 6 || config->send_ring_len > 250)
		config->send_ring_len = 32;
	if (config->element_size < 12 || config->element_size > 500)
		config->element_size = 36;


	SkyVirtualChannel* vchannel = SKY_MALLOC(sizeof(SkyVirtualChannel));
	SKY_ASSERT(vchannel != NULL);

	// Create send ring
	vchannel->sendRing = sky_send_ring_create(config->send_ring_len, 0);
	SKY_ASSERT(vchannel->sendRing != NULL);

	// Create receive ring
	vchannel->rcvRing = sky_rcv_ring_create(config->rcv_ring_len, config->horizon_width, 0);
	SKY_ASSERT(vchannel->rcvRing != NULL);

	// Create element buffer
	int32_t ring_slots = config->rcv_ring_len + config->send_ring_len -2;
	int32_t optimal_element_count = compute_required_element_count(config->element_size, ring_slots, SKY_PAYLOAD_MAX_LEN);
	vchannel->elementBuffer = sky_element_buffer_create(config->element_size, optimal_element_count);
	SKY_ASSERT(vchannel->elementBuffer != NULL);

	// Reset ARQ state
	sky_vc_wipe_to_arq_off_state(vchannel);
	return vchannel;
}


void sky_vc_destroy(SkyVirtualChannel* vchannel){
	sky_send_ring_destroy(vchannel->sendRing);
	sky_rcv_ring_destroy(vchannel->rcvRing);
	sky_element_buffer_destroy(vchannel->elementBuffer);
	SKY_FREE(vchannel);
}


void sky_vc_wipe_to_arq_off_state(SkyVirtualChannel* vchannel){
	sky_send_ring_wipe(vchannel->sendRing, vchannel->elementBuffer, 0);
	sky_rcv_ring_wipe(vchannel->rcvRing, vchannel->elementBuffer, 0);
	sky_element_buffer_wipe(vchannel->elementBuffer);
	vchannel->need_recall = 0;
	vchannel->arq_state_flag = ARQ_STATE_OFF;
	vchannel->arq_session_identifier = 0;
	vchannel->last_tx_tick = 0;
	vchannel->last_rx_tick = 0;
	vchannel->last_ctrl_send_tick = 0;
	vchannel->unconfirmed_payloads = 0;
	vchannel->handshake_send = 0;
}

void sky_vc_wipe_to_arq_init_state(SkyVirtualChannel *vchannel)
{
	sky_rcv_ring_wipe(vchannel->rcvRing, vchannel->elementBuffer, 0);
	sky_send_ring_wipe(vchannel->sendRing, vchannel->elementBuffer, 0);
	vchannel->need_recall = 0;
	vchannel->arq_state_flag = ARQ_STATE_IN_INIT;
	vchannel->arq_session_identifier = (uint32_t)sky_get_tick_time();
	vchannel->last_tx_tick = sky_get_tick_time();
	vchannel->last_rx_tick = sky_get_tick_time();
	vchannel->last_ctrl_send_tick = 0;
	vchannel->unconfirmed_payloads = 0;
	vchannel->handshake_send = 0;
}

void sky_vc_wipe_to_arq_on_state(SkyVirtualChannel *vchannel, uint32_t identifier)
{
	sky_rcv_ring_wipe(vchannel->rcvRing, vchannel->elementBuffer, 0);
	sky_send_ring_wipe(vchannel->sendRing, vchannel->elementBuffer, 0);
	vchannel->need_recall = 0;
	vchannel->arq_state_flag = ARQ_STATE_ON;
	vchannel->arq_session_identifier = identifier;
	vchannel->last_tx_tick = sky_get_tick_time();
	vchannel->last_rx_tick = sky_get_tick_time();
	vchannel->last_ctrl_send_tick = 0;
	vchannel->unconfirmed_payloads = 0;
	vchannel->handshake_send = 1;
}

int sky_vc_arq_connect(SkyVirtualChannel *vchannel)
{
	sky_vc_wipe_to_arq_init_state(vchannel);
	return SKY_RET_OK;
}

int sky_vc_arq_disconnect(SkyVirtualChannel *vchannel)
{
	sky_vc_wipe_to_arq_off_state(vchannel);
	return SKY_RET_OK;
}



// Check ARQ connection timeout conditions
void sky_vc_check_timeouts(SkyVirtualChannel *vchannel, sky_tick_t now, sky_tick_t timeout)
{
	// If ARQ is off, so nothing to do here.
	if (vchannel->arq_state_flag == ARQ_STATE_OFF)
		return;

	// Check timeouts
	int tx_timeout = wrap_time_ticks(now - vchannel->last_tx_tick) > timeout;
	int rx_timeout = wrap_time_ticks(now - vchannel->last_rx_tick) > timeout;

	// Force connection to off state in case timeout has occurred.
	if (tx_timeout || rx_timeout)
		sky_vc_wipe_to_arq_off_state(vchannel);

}

//===== SKYLINK VIRTUAL CHANNEL ========================================================================================







//=== SEND =============================================================================================================
//======================================================================================================================

/*
 * sky_vc_write_packet_to_send()
 * sky_vc_write_to_send()
 * sky_vc_write_to_send_buffer()
 */

int sky_vc_push_packet_to_send(SkyVirtualChannel* vchannel, void* payload, int length){
	if(length > SKY_PAYLOAD_MAX_LEN){
		return SKY_RET_TOO_LONG_PAYLOAD;
	}
	return sendRing_push_packet_to_send(vchannel->sendRing, vchannel->elementBuffer, payload, length);
}

int sky_vc_send_buffer_is_full(SkyVirtualChannel* vchannel){
	return sendRing_is_full(vchannel->sendRing);
}

int sky_vc_schedule_resend(SkyVirtualChannel* arqRing, int sequence){
	return sendRing_schedule_resend(arqRing->sendRing, sequence);
}

int sky_vc_count_packets_to_tx(SkyVirtualChannel* vchannel, int include_resend){
	return sendRing_count_packets_to_send(vchannel->sendRing, include_resend);
}

int sky_vc_read_packet_for_tx(SkyVirtualChannel* vchannel, void* tgt, int* sequence, int include_resend){
	return sendRing_read_to_tx(vchannel->sendRing, vchannel->elementBuffer, tgt, sequence, include_resend);
}

int sky_vc_read_packet_for_tx_monotonic(SkyVirtualChannel* vchannel, void* tgt, int* sequence){
	int read = sendRing_read_to_tx(vchannel->sendRing, vchannel->elementBuffer, tgt, sequence, 0);
	if(read >= 0){
		sendRing_clean_tail_up_to(vchannel->sendRing, vchannel->elementBuffer, vchannel->sendRing->tx_sequence);
	}
	return read;
}

int sky_vc_can_recall(SkyVirtualChannel* vchannel, int sequence){
	return sendRing_can_recall(vchannel->sendRing, sequence);
}

void sky_vc_update_tx_sync(SkyVirtualChannel* vchannel, int peer_rx_head_sequence_by_ctrl, sky_tick_t now){
	int n_cleared = sendRing_clean_tail_up_to(vchannel->sendRing, vchannel->elementBuffer, peer_rx_head_sequence_by_ctrl);
	if(n_cleared > 0){
		vchannel->last_tx_tick = now;
	}
	if(vchannel->sendRing->tx_sequence == peer_rx_head_sequence_by_ctrl){
		vchannel->last_tx_tick = now;
	}
}

int sky_vc_peek_next_tx_size_and_sequence(SkyVirtualChannel* vchannel, int include_resend, int* length, int* sequence){
	return sendRing_peek_next_tx_size_and_sequence(vchannel->sendRing, vchannel->elementBuffer, include_resend, length, sequence);
}
//======================================================================================================================
//======================================================================================================================







//=== RECEIVE ==========================================================================================================
//======================================================================================================================

// sky_vc_count_available_packets()
int sky_vc_count_readable_rcv_packets(SkyVirtualChannel* vchannel){
	return rcvRing_count_readable_packets(vchannel->rcvRing);
}

// sky_vc_read_from_receive_buffer()
int sky_vc_read_next_received(SkyVirtualChannel* vchannel, void* tgt, int max_length){
	return rcvRing_read_next_received(vchannel->rcvRing, vchannel->elementBuffer, tgt, max_length);
}

int sky_vc_push_rx_packet_monotonic(SkyVirtualChannel* vchannel, const void* src, int length){
	int sequence = vchannel->rcvRing->head_sequence;
	return rcvRing_push_rx_packet(vchannel->rcvRing, vchannel->elementBuffer, src, length, sequence);
}

// sky_vc_write_to_receive_buffer()
int sky_vc_push_rx_packet(SkyVirtualChannel* vchannel, const void* src, int length, int sequence, sky_tick_t now){
	int r = rcvRing_push_rx_packet(vchannel->rcvRing, vchannel->elementBuffer, src, length, sequence);
	if(r > 0){ //head advanced at least by 1
		vchannel->last_rx_tick = now;
	}
	vchannel->unconfirmed_payloads++;
	return r;
}


void sky_vc_update_rx_sync(SkyVirtualChannel* vchannel, int peer_tx_head_sequence_by_ctrl, sky_tick_t now){
	int sync = rcvRing_get_sequence_sync_status(vchannel->rcvRing, peer_tx_head_sequence_by_ctrl);
	if(sync == SKY_RET_OK) { // SKY_RET_RING_SEQUENCES_IN_SYNC
		vchannel->last_rx_tick = now;
		vchannel->need_recall = 0;
	}
	else if(sync == SKY_RET_RING_SEQUENCES_OUT_OF_SYNC){
		vchannel->need_recall = 1;
	}
	else if(sync == SKY_RET_RING_SEQUENCES_DETACHED){
		//vc->arq_state_flag = ARQ_STATE_BROKEN;
	}
}
//======================================================================================================================
//======================================================================================================================


// sky_vc_has_content_to_send()
int sky_vc_content_to_send(SkyVirtualChannel* vchannel, SkyConfig* config, sky_tick_t now, uint16_t frames_sent_in_this_vc_window)
{
	switch (vchannel->arq_state_flag) {
	case ARQ_STATE_OFF:
		/*
		 * ARQ is off:
		 */

		// Transmit if there's something in the buffer
		if (sendRing_count_packets_to_send(vchannel->sendRing, 0) > 0)
			return 1;

		return 0;

	case ARQ_STATE_IN_INIT:
		/*
		 * ARQ is handshaking but we haven't received response yet.
		 */

		// Transmit if not too many idle frames have not been generated.
		if (frames_sent_in_this_vc_window < config->arq.idle_frames_per_window)
			return 1;
		return 0;

	case ARQ_STATE_ON:
		/*
		 * ARQ is on.
		 */

		// Yes, if we have something in the buffer
		if (sendRing_count_packets_to_send(vchannel->sendRing, 1) > 0)
			return 1;

		// Yes, if and we have need to retransmit something and not all idle frame are used.
		if (frames_sent_in_this_vc_window < config->arq.idle_frames_per_window && \
		    (rcvRing_get_horizon_bitmap(vchannel->rcvRing) || vchannel->need_recall))
			return 1;

		// Yes, if we need to response a ARQ handshake
		if (vchannel->handshake_send)
			return 1;

		// Can still generate more idle frames?
		if (frames_sent_in_this_vc_window < config->arq.idle_frames_per_window) {

			int b1 = wrap_time_ticks(now - vchannel->last_ctrl_send_tick) > config->arq.idle_frame_threshold;
			int b2 = wrap_time_ticks(now - vchannel->last_tx_tick) > config->arq.idle_frame_threshold;
			int b3 = wrap_time_ticks(now - vchannel->last_rx_tick) > config->arq.idle_frame_threshold;
			int b4 = vchannel->unconfirmed_payloads > 0;
			if (b1 || b2 || b3 || b4)
				return 1;
		}
	}

	return 0;
}

int sky_vc_fill_frame(SkyVirtualChannel *vchannel, SkyConfig *config, SkyTransmitFrame *tx_frame, sky_tick_t now, uint16_t frames_sent_in_this_vc_window)
{

	switch (vchannel->arq_state_flag) {
	case ARQ_STATE_OFF: {
		/*
		 */

		// Try to read a new frame
		int length, sequence;
		if (sendRing_peek_next_tx_size_and_sequence(vchannel->sendRing, vchannel->elementBuffer, 0, &length, &sequence) >= 0)
		{
			SKY_ASSERT(length <= sky_frame_get_space_left(tx_frame->frame))

			int read = sky_vc_read_packet_for_tx_monotonic(vchannel, tx_frame->ptr, &sequence);
			SKY_ASSERT(read >= 0)
			tx_frame->frame->length += read;
			tx_frame->ptr += read;
			tx_frame->hdr->flags |= SKY_FLAG_HAS_PAYLOAD;
			return 1;
		}

		return 0;
	}
	case ARQ_STATE_IN_INIT:
		/*
		 * ARQ is handshaking but we haven't received response yet.
		 */

		// Transmit handshake as an idle frame if we haven't spam enough in this window
		if (frames_sent_in_this_vc_window < config->arq.idle_frames_per_window) {

			// Add only a ARQ handshake extension to the packet
			sky_frame_add_extension_arq_handshake(tx_frame, ARQ_STATE_IN_INIT, vchannel->arq_session_identifier);
			return 1;
		}

		return 0;

	case ARQ_STATE_ON: {
		/*
		 * ARQ is on,
		 */

		int ret = 0;

		tx_frame->hdr->flag_arq_on = 1; // TODO: SKY_FLAG_ARQ_ON

		// Add ARQ handshake response if it is pending.
		if (vchannel->handshake_send > 0) {
			sky_frame_add_extension_arq_handshake(tx_frame, ARQ_STATE_ON, vchannel->arq_session_identifier);
			vchannel->handshake_send--;
			ret = 1;
		}

		// Add ARQ retransmit request extension if we see that there are missing frames.
		uint16_t mask = rcvRing_get_horizon_bitmap(vchannel->rcvRing);
		if ((frames_sent_in_this_vc_window < config->arq.idle_frames_per_window) && (mask || vchannel->need_recall)){
			sky_frame_add_extension_arq_request(tx_frame, vchannel->rcvRing->head_sequence, mask);
			vchannel->need_recall = 0;
			ret = 1;
		}

		// Add ARQ Control/Sync extension if TBD
		int payload_to_send = sendRing_count_packets_to_send(vchannel->sendRing, 1) > 0;
		int b0 = frames_sent_in_this_vc_window < config->arq.idle_frames_per_window;
		int b1 = wrap_time_ticks(now - vchannel->last_ctrl_send_tick) > config->arq.idle_frame_threshold;
		int b2 = wrap_time_ticks(now - vchannel->last_tx_tick) > config->arq.idle_frame_threshold;
		int b3 = wrap_time_ticks(now - vchannel->last_rx_tick) > config->arq.idle_frame_threshold;
		int b4 = vchannel->unconfirmed_payloads > 0;
		if ((b0 && (b1 || b2 || b3 || b4)) || payload_to_send){
			sky_frame_add_extension_arq_ctrl(tx_frame, vchannel->sendRing->tx_sequence, vchannel->rcvRing->head_sequence);
			vchannel->last_ctrl_send_tick = now;
			vchannel->unconfirmed_payloads = 0;
			ret = 1;
		}

		// If we have something to be send copy it to frame.
		if (sendRing_count_packets_to_send(vchannel->sendRing, 1) > 0)
		{
			// Peek length and the sequence number of the next frame
			int packet_sequence = -1;
			int packet_length = 0; // TODO: packet_length could be in the return value
			ret = sendRing_peek_next_tx_size_and_sequence(vchannel->sendRing, vchannel->elementBuffer, 1, &packet_length, &packet_sequence);
			if (ret < 0)
				return ret;

			// Does the packet fit in remaining space?
			int required_length = packet_length + (int)sizeof(ExtARQSeq) + 1;
			if (required_length <= sky_frame_get_space_left(tx_frame->frame) && packet_sequence >= 0)
			{
				// Add ARQ sequence number extension
				sky_frame_add_extension_arq_sequence(tx_frame, packet_sequence);

				// Copy the packet inside the frame
				int read = sendRing_read_to_tx(vchannel->sendRing, vchannel->elementBuffer, tx_frame->ptr, &packet_sequence, 1);
				SKY_ASSERT(read >= 0);
				tx_frame->ptr += read;
				tx_frame->frame->length += read;
				tx_frame->hdr->flags |= SKY_FLAG_HAS_PAYLOAD;
				tx_frame->hdr->flag_has_payload = 1;

				ret = 1;
			}
			else {
				/* If the payload for some reason is too large, remove is nonetheless. */
				uint8_t tmp_tgt[300];
				sendRing_read_to_tx(vchannel->sendRing, vchannel->elementBuffer, tmp_tgt, &packet_sequence, 1);
				SKY_PRINTF(SKY_DIAG_BUG, "Too larger packet to fit! Discarding it!");
				return SKY_RET_NO_SPACE_FOR_PAYLOAD;
			}

		}

		return ret;
	}
	}

	return 0;
}


int sky_vc_handle_handshake(SkyVirtualChannel* vchannel, uint8_t peer_state, uint32_t identifier)
{
	switch (vchannel->arq_state_flag) {
	case ARQ_STATE_OFF:
		/*
		 * Our ARQ state is off and we received a handshake.
		 * Accept the handshake and set the handshake response flag-
		 */
		sky_vc_wipe_to_arq_on_state(vchannel, identifier);
		vchannel->handshake_send = 1;
		return 1;

	case ARQ_STATE_IN_INIT:
		/*
		 * Our ARQ has been initialized (we are the initiator)
		 * and we received the handshake form the peer.
		 */
		if (identifier == vchannel->arq_session_identifier) {
			// Matching session identifier so ARQ is now connected.
			vchannel->arq_state_flag = ARQ_STATE_ON;
			vchannel->handshake_send = 0;
			return 1;
		}
		else if (identifier > vchannel->arq_session_identifier) { // TODO: Overflow not considered!
			// A newer identifier is received.
			sky_vc_wipe_to_arq_on_state(vchannel, identifier);
			vchannel->handshake_send = 1;
			return 1;
		}
		else {
			// Invalid response identity
			return 0;
		}

	case ARQ_STATE_ON:
		/*
		 * Our ARQ is on and we received a new handshake.
		 */
		if (identifier == vchannel->arq_session_identifier) {
			// Matching session identifier matches so this is just redundant re-transmitted handshake.
			vchannel->handshake_send = 0;
			if (peer_state == ARQ_STATE_IN_INIT)
				vchannel->handshake_send = 1;
			return 0;
		}
		else {
			// The peer is trying to reconnect to us so just accept the
			sky_vc_wipe_to_arq_on_state(vchannel, identifier);
			vchannel->handshake_send = 1;
			return 1;
		}
		return 0;
	}

	return -1; // Invalid state
}


int sky_vc_process_frame(SkyVirtualChannel *vchannel, SkyParsedFrame *parsed, sky_tick_t now)
{

	/* Handle incoming ARQ handshake first in any state.
	 * Our state machine might advance during handshake handling. */
	if (parsed->arq_handshake != NULL) {
		const ExtARQHandshake *handshake = &parsed->arq_handshake->ARQHandshake;
		sky_vc_handle_handshake(vchannel, handshake->peer_state, handshake->identifier);
	}

	switch (vchannel->arq_state_flag) {
	case ARQ_STATE_OFF:
		/*
		 * ARQ is off.
		 * Just pass the payload to buffer.
		 */
		if (parsed->payload_len > 0)
			sky_vc_push_rx_packet_monotonic(vchannel, parsed->payload, parsed->payload_len);
		break;

	case ARQ_STATE_IN_INIT:
		/* ARQ is trying to handshake.
		 * but we received a frame without response or with invalid response.
		 * Ignore rest of the frame due to state missmatch. */
		break;

	case ARQ_STATE_ON:
		/*
		 * ARQ is on, so parse and handle ARQ related extension headers and
		 */

		/* Handle ARQ control extension */
		if (parsed->arq_ctrl != NULL)
		{
			sky_arq_sequence_t rx_sequence = sky_ntoh16(parsed->arq_ctrl->ARQCtrl.rx_sequence);
			sky_arq_sequence_t tx_sequence = sky_ntoh16(parsed->arq_ctrl->ARQCtrl.rx_sequence);
			SKY_PRINTF(SKY_DIAG_ARQ | SKY_DIAG_DEBUG, "Received ARQ CTRL %d %d", (int)rx_sequence, (int)tx_sequence);
			sky_vc_update_tx_sync(vchannel, rx_sequence, now);
			sky_vc_update_rx_sync(vchannel, tx_sequence, now);
		}

		/* Handle ARQ data packet */
		if (parsed->payload_len > 0) // TODO: Only non-zero and positive lengths?
		{
			/* Make sure we received ARQ sequence number header. */
			if (parsed->arq_sequence == NULL)
			{
				SKY_PRINTF(SKY_DIAG_ARQ | SKY_DIAG_BUG, "ARQ is on but received a frame without ARQ sequence!");
				return -1; // Ignore mallformed frame
			}

			sky_arq_sequence_t packet_sequence = sky_ntoh16(parsed->arq_sequence->ARQSeq.sequence);
			SKY_PRINTF(SKY_DIAG_ARQ | SKY_DIAG_DEBUG, "Received ARQ packet %d", (int)packet_sequence);
			sky_vc_push_rx_packet(vchannel, parsed->payload, parsed->payload_len, packet_sequence, now);
		}

		/* Handle retransmit request received */
		if (parsed->arq_request != NULL)
		{
			sky_arq_sequence_t window_start = sky_ntoh16(parsed->arq_request->ARQReq.sequence);
			uint16_t window_mask = sky_ntoh16(parsed->arq_request->ARQReq.mask);
			SKY_PRINTF(SKY_DIAG_ARQ | SKY_DIAG_DEBUG, "Received ARQ Request: %d %04x", (int)window_start, (int)window_mask);
			sendRing_schedule_resends_by_mask(vchannel->sendRing, window_start, window_mask);
		}

		break;

	default:
		return -1;
	}
	return 0;
}
