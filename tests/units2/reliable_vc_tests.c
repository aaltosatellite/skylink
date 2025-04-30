// Tests for skylink reliable virtual channel implementation.

#include "units.h"

/*
 * Test getting SkyState.
 */
TEST(sky_state)
{
	// Create protocol instance
	SkyConfig config = default_config;
	SkyHandle handle = sky_create(&config);

	SkyState state;
	sky_get_state(handle, &state);

	// Loop all vc's for initial state
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++) {
		SkyVirtualChannel *vc = handle->virtual_channels[i];
		SkyVCState *vc_state = &state.vc[i];
		ASSERT(vc_state->state == ARQ_STATE_OFF, "VC: %d state: %d", i, vc_state->state);
		ASSERT(vc_state->tx_frames == 0, "VC: %d tx_frames: %d", i, vc_state->tx_frames);
		ASSERT(vc_state->rx_frames == 0, "VC: %d rx_frames: %d", i, vc_state->rx_frames);
		ASSERT(vc_state->free_tx_slots == (config.vc[i].send_ring_len-1), "VC: %d free_tx_slots: %d", i, (config.vc[i].send_ring_len-1));
		ASSERT(vc_state->session_identifier == vc->arq_session_identifier, "VC: %d session_identifier: %d", i, state.vc[i].session_identifier);
	}

	// Time to add some tx and rx frames.
	// Add 1 tx frame to vc 0
	uint8_t payload[100];
	fillrand(payload, sizeof(payload));
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++)
	{
		SkyVirtualChannel *vc = handle->virtual_channels[i];
		for (int j = 0; j < i+1; j++){
			int sRing = sendRing_push_packet_to_send(vc->sendRing, vc->elementBuffer, (const uint8_t *)payload, sizeof(payload));
			int rRing = rcvRing_push_rx_packet(vc->rcvRing, vc->elementBuffer, (const uint8_t *)payload, sizeof(payload), j);
			ASSERT(sRing >= 0, "VC: %d sendRing_push_packet_to_send error: %d, J: %d", i, sRing, j);
			ASSERT(rRing >= 0, "VC: %d rcvRing_push_rx_packet error: %d, J: %d", i, rRing, j);
		}
	}

	sky_get_state(handle, &state);

	// Loop all VCs for updated state
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++)
	{
		SkyVirtualChannel *vc = handle->virtual_channels[i];
		SkyVCState *vc_state = &state.vc[i];
		ASSERT(vc_state->state == ARQ_STATE_OFF, "VC: %d state: %d", i, vc_state->state);
		ASSERT(vc_state->tx_frames == i+1, "VC: %d tx_frames: %d", i, vc_state->tx_frames);
		ASSERT(vc_state->rx_frames == i+1, "VC: %d rx_frames: %d", i, vc_state->rx_frames);
		ASSERT(vc_state->free_tx_slots == (config.vc[i].send_ring_len-1)-(i+1), "VC: %d free_tx_slots: %d", i, (config.vc[i].send_ring_len-1)-(i+1));
		ASSERT(vc_state->session_identifier == vc->arq_session_identifier, "VC: %d session_identifier: %d", i, vc_state->session_identifier);
	}

	sky_destroy(handle);
}

/*
 * Test creating a virtual channel. Check that it is created correctly.
 */
TEST(vc_create)
{
	// Valid config.
	SkyVCConfig vcConfig;
	vcConfig.send_ring_len = 10;
	vcConfig.rcv_ring_len = 10;
	vcConfig.horizon_width = 4;
	vcConfig.require_authentication = 1;
	vcConfig.usable_element_size = 200;

	// Create a virtual channel instance
	SkyVirtualChannel *vc = sky_vc_create(&vcConfig);
	ASSERT(vc != NULL, "VC is NULL");
	ASSERT(vc->sendRing != NULL, "VC sendRing is NULL");
	ASSERT(vc->sendRing->length == 10, "VC sendRing length is not 10, it is: %d", vc->sendRing->length);
	ASSERT(vc->rcvRing != NULL, "VC rcvRing is NULL");
	ASSERT(vc->rcvRing->length == 10, "VC rcvRing length is not 10, it is: %d", vc->rcvRing->length);
	ASSERT(vc->rcvRing->horizon_width == 4, "VC rcvRing horizon_width is not 4, it is: %d", vc->rcvRing->horizon_width);
	ASSERT(vc->elementBuffer->element_usable_space == 200, "VC elementBuffer element_usable_space is not 200, it is: %d", vc->elementBuffer->element_usable_space);
	ASSERT(vc->arq_session_identifier == 0, "VC arq_session_identifier is not 0, it is: %d", vc->arq_session_identifier);
	ASSERT(vc->arq_state == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state);
	ASSERT(vc->elementBuffer != NULL, "VC elementBuffer is NULL");
	ASSERT(vc->handshake_send == 0, "VC handshake_send is not 0, it is: %d", vc->handshake_send);
	ASSERT(vc->last_ctrl_send_tick == 0, "VC last_ctrl_send_tick is not 0, it is: %d", vc->last_ctrl_send_tick);
	ASSERT(vc->last_rx_tick == 0, "VC last_rx_tick is not 0, it is: %d", vc->last_rx_tick);
	ASSERT(vc->last_tx_tick == 0, "VC last_tx_tick is not 0, it is: %d", vc->last_tx_tick);
	ASSERT(vc->unconfirmed_payloads == 0, "VC unconfirmed_payloads is not 0, it is: %d", vc->unconfirmed_payloads);

    // Free VC.
    sky_vc_destroy(vc);

    // Invalid config, less than minimum values.
    vcConfig.send_ring_len = 0;
    vcConfig.rcv_ring_len = 0;
    vcConfig.horizon_width = 0;
	vcConfig.usable_element_size = 0;

	vc = sky_vc_create(&vcConfig);
	ASSERT(vc != NULL, "VC is NULL");
	ASSERT(vc->sendRing != NULL, "VC sendRing is NULL");
	ASSERT(vc->sendRing->length == 32, "VC sendRing length is not 32, it is: %d", vc->sendRing->length);
	ASSERT(vc->rcvRing != NULL, "VC rcvRing is NULL");
	ASSERT(vc->rcvRing->length == 32, "VC rcvRing length is not 32, it is: %d", vc->rcvRing->length);
	ASSERT(vc->rcvRing->horizon_width == 0, "VC rcvRing horizon_width is not 29, it is: %d", vc->rcvRing->horizon_width);
	ASSERT(vc->elementBuffer->element_usable_space == 32, "VC elementBuffer element_usable_space is not 32, it is: %d", vc->elementBuffer->element_usable_space);
	ASSERT(vc->arq_session_identifier == 0, "VC arq_session_identifier is not 0, it is: %d", vc->arq_session_identifier);
	ASSERT(vc->arq_state == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state);
	ASSERT(vc->elementBuffer != NULL, "VC elementBuffer is NULL");
	ASSERT(vc->handshake_send == 0, "VC handshake_send is not 0, it is: %d", vc->handshake_send);
	ASSERT(vc->last_ctrl_send_tick == 0, "VC last_ctrl_send_tick is not 0, it is: %d", vc->last_ctrl_send_tick);
	ASSERT(vc->last_rx_tick == 0, "VC last_rx_tick is not 0, it is: %d", vc->last_rx_tick);
	ASSERT(vc->last_tx_tick == 0, "VC last_tx_tick is not 0, it is: %d", vc->last_tx_tick);
	ASSERT(vc->unconfirmed_payloads == 0, "VC unconfirmed_payloads is not 0, it is: %d", vc->unconfirmed_payloads);

    // Free VC.
    sky_vc_destroy(vc);

    // Invalid config, greater than maximum values.
    vcConfig.send_ring_len = 1000;
    vcConfig.rcv_ring_len = 1000;
    vcConfig.horizon_width = 1000;
	vcConfig.usable_element_size = 1000;

	vc = sky_vc_create(&vcConfig);
	ASSERT(vc != NULL, "VC is NULL");
	ASSERT(vc->sendRing != NULL, "VC sendRing is NULL");
	ASSERT(vc->sendRing->length == 32, "VC sendRing length is not 32, it is: %d", vc->sendRing->length);
	ASSERT(vc->rcvRing != NULL, "VC rcvRing is NULL");
	ASSERT(vc->rcvRing->length == 32, "VC rcvRing length is not 32, it is: %d", vc->rcvRing->length);
	ASSERT(vc->rcvRing->horizon_width == 29, "VC rcvRing horizon_width is not 29, it is: %d", vc->rcvRing->horizon_width);
	ASSERT(vc->elementBuffer->element_usable_space == 32, "VC elementBuffer element_usable_space is not 32, it is: %d", vc->elementBuffer->element_usable_space);
	ASSERT(vc->arq_session_identifier == 0, "VC arq_session_identifier is not 0, it is: %d", vc->arq_session_identifier);
	ASSERT(vc->arq_state == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state);
	ASSERT(vc->elementBuffer != NULL, "VC elementBuffer is NULL");
	ASSERT(vc->handshake_send == 0, "VC handshake_send is not 0, it is: %d", vc->handshake_send);
	ASSERT(vc->last_ctrl_send_tick == 0, "VC last_ctrl_send_tick is not 0, it is: %d", vc->last_ctrl_send_tick);
	ASSERT(vc->last_rx_tick == 0, "VC last_rx_tick is not 0, it is: %d", vc->last_rx_tick);
	ASSERT(vc->last_tx_tick == 0, "VC last_tx_tick is not 0, it is: %d", vc->last_tx_tick);
	ASSERT(vc->unconfirmed_payloads == 0, "VC unconfirmed_payloads is not 0, it is: %d", vc->unconfirmed_payloads);

	// Free VC
	sky_vc_destroy(vc);
}

// Test changing arq states from off to init to on and back to off.
TEST(arq_state_change)
{
	// Create config
	SkyVCConfig vcConfig;
	vcConfig.send_ring_len = 10;
	vcConfig.rcv_ring_len = 10;
	vcConfig.horizon_width = 4;
	vcConfig.require_authentication = 1;
	vcConfig.usable_element_size = 200;

	// Create a virtual channel instance.
	SkyVirtualChannel *vc = sky_vc_create(&vcConfig);

	// Check arq state is off.
	ASSERT(vc->arq_state == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state);

	// Change arq state to init.
	sky_vc_wipe_to_arq_init_state(vc);
	ASSERT(vc->arq_state == ARQ_STATE_IN_INIT, "VC arq_state is not ARQ_STATE_IN_INIT, it is: %d", vc->arq_state);

	// Check values are changed.
	ASSERT(vc->need_recall == 0, "VC need_recall is not 0, it is: %d", vc->need_recall);
	ASSERT(vc->arq_session_identifier == (uint32_t)sky_get_tick_time(), "VC arq_session_identifier is not 0, it is: %d", vc->arq_session_identifier);
	ASSERT(vc->last_tx_tick == sky_get_tick_time(), "VC last_tx_tick is not 0, it is: %d", vc->last_tx_tick);
	ASSERT(vc->last_rx_tick == sky_get_tick_time(), "VC last_rx_tick is not 0, it is: %d", vc->last_rx_tick);
	ASSERT(vc->last_ctrl_send_tick == 0, "VC last_ctrl_send_tick is not 0, it is: %d", vc->last_ctrl_send_tick);
	ASSERT(vc->unconfirmed_payloads == 0, "VC unconfirmed_payloads is not 0, it is: %d", vc->unconfirmed_payloads);
	ASSERT(vc->handshake_send == 0, "VC handshake_send is not 0, it is: %d", vc->handshake_send);

	// Change arq state to on.
	sky_vc_wipe_to_arq_on_state(vc, 10);
	ASSERT(vc->arq_state == ARQ_STATE_ON, "VC arq_state is not ARQ_STATE_ON, it is: %d", vc->arq_state);

	// Check values are changed.
	ASSERT(vc->need_recall == 0, "VC need_recall is not 0, it is: %d", vc->need_recall);
	ASSERT(vc->arq_session_identifier == 10, "VC arq_session_identifier is not 10, it is: %d", vc->arq_session_identifier);
	ASSERT(vc->last_tx_tick == sky_get_tick_time(), "VC last_tx_tick is not 0, it is: %d", vc->last_tx_tick);
	ASSERT(vc->last_rx_tick == sky_get_tick_time(), "VC last_rx_tick is not 0, it is: %d", vc->last_rx_tick);
	ASSERT(vc->last_ctrl_send_tick == 0, "VC last_ctrl_send_tick is not 0, it is: %d", vc->last_ctrl_send_tick);
	ASSERT(vc->unconfirmed_payloads == 0, "VC unconfirmed_payloads is not 0, it is: %d", vc->unconfirmed_payloads);
	ASSERT(vc->handshake_send == 1, "VC handshake_send is not 1, it is: %d", vc->handshake_send);

	// Change arq state to off.
	sky_vc_wipe_to_arq_off_state(vc);
	ASSERT(vc->arq_state == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state);

	// Check values are changed.
	ASSERT(vc->need_recall == 0, "VC need_recall is not 0, it is: %d", vc->need_recall);
	ASSERT(vc->arq_session_identifier == 0, "VC arq_session_identifier is not 0, it is: %d", vc->arq_session_identifier);
	ASSERT(vc->last_tx_tick == 0, "VC last_tx_tick is not 0, it is: %d", vc->last_tx_tick);
	ASSERT(vc->last_rx_tick == 0, "VC last_rx_tick is not 0, it is: %d", vc->last_rx_tick);
	ASSERT(vc->last_ctrl_send_tick == 0, "VC last_ctrl_send_tick is not 0, it is: %d", vc->last_ctrl_send_tick);
	ASSERT(vc->unconfirmed_payloads == 0, "VC unconfirmed_payloads is not 0, it is: %d", vc->unconfirmed_payloads);
	ASSERT(vc->handshake_send == 0, "VC handshake_send is not 0, it is: %d", vc->handshake_send);

	// Free VC
	sky_vc_destroy(vc);
}

/*
 * Test checking for timeouts. Should wipe to off state if there is a timeout.
 * A timeout means that the current time subtracted by the last tx or rx tick is greater than the timeout value.
 */
TEST(check_timeouts)
{
	// Create config
	SkyDiagnostics diag;
	SkyVCConfig vcConfig;
	vcConfig.send_ring_len = 10;
	vcConfig.rcv_ring_len = 10;
	vcConfig.horizon_width = 4;
	vcConfig.require_authentication = 1;
	vcConfig.usable_element_size = 200;

	// Create a virtual channel instance.
	SkyVirtualChannel *vc = sky_vc_create(&vcConfig);

	// Check arq state is off.
	ASSERT(vc->arq_state == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state);
	// Should do nothing as arq state is off.
	// Can't really be tested since it is a void function where the only thing that happens is a return or a state change to arq off.
	sky_vc_check_timeouts(vc, 300, 120, &diag);

	// Change arq state to init.
	sky_vc_wipe_to_arq_init_state(vc);
	ASSERT(vc->arq_state == ARQ_STATE_IN_INIT, "VC arq_state is not ARQ_STATE_IN_INIT, it is: %d", vc->arq_state);

	// Test a situation where there is no timeout.
	sky_vc_check_timeouts(vc, 120, 300, &diag);
	// State should still be in init.
	ASSERT(vc->arq_state == ARQ_STATE_IN_INIT, "VC arq_state is not ARQ_STATE_IN_INIT, it is: %d", vc->arq_state);

	// Test a situation where there is a timeout.
	sky_vc_check_timeouts(vc, 300, 120, &diag);
	// State should be off.
	ASSERT(vc->arq_state == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state);

	// Free VC
	sky_vc_destroy(vc);
}

/*
 * Test checking if there is content to send in a virtual channel. Depends on arq state so test all states.
 */
TEST(vc_content_to_send)
{
	// Create config
	SkyConfig config = default_config;
	config.arq.idle_frames_per_window = 4;
	//SkyHandle handle = sky_create(&config);
	SkyVirtualChannel* vc = sky_vc_create(&config.vc[0]);

	SkyDiagnostics diag;
	// ARQ OFF, simply tests if there is content to send.:
	// Check that there is no content to send.
	int ret = sky_vc_content_to_send(vc, &config, 0, 0);
	ASSERT(ret == 0, "VC sendable content is not 0, it is: %d", ret);

	// Add content to send.
	uint8_t payload[100];
	fillrand(payload, 100);
	int sRing = sendRing_push_packet_to_send(vc->sendRing, vc->elementBuffer, (const uint8_t*)payload, sizeof(payload));
	ASSERT(sRing >= 0, "VC sendRing_push_packet_to_send error: %d", sRing);

	// Check that there is content to send.
	ret = sky_vc_content_to_send(vc, &config, 0, 0);
	ASSERT(ret == 1, "VC sendable content is not 1, it is: %d", ret);

	// ARQ IN INIT, tests if frames_sent_in_this_vc_window < config->arq.idle_frames_per_window:
	// Change arq state to init.
	sky_vc_wipe_to_arq_init_state(vc);

	ASSERT(vc->arq_state == ARQ_STATE_IN_INIT, "VC arq_state is not ARQ_STATE_IN_INIT, it is: %d", vc->arq_state);
	// Check that there is no content to send.
	ret = sky_vc_content_to_send(vc, &config, 0, 5);
	ASSERT(ret == 0, "VC sendable content is not 0, it is: %d", ret);

	// Frame sent in this vc window is given as a parameter, check when this is smaller than the idle frames per window.
	ret = sky_vc_content_to_send(vc, &config, 0, 3);
	ASSERT(ret == 1, "VC sendable content is not 1, it is: %d", ret);

	// Check when this is equal to the idle frames per window.
	ret = sky_vc_content_to_send(vc, &config, 0, 4);
	ASSERT(ret == 0, "VC sendable content is not 0, it is: %d", ret);

	units_advance_ticks(100);

	// ARQ ON:
	// Returns 1 if there is content to send or something needs to be retransmitted or we need to send a handshake or if idle frames per window is not reached.
	// 0 otherwise.
	// Change arq state to on.
	sky_vc_wipe_to_arq_on_state(vc, 10);
	// Turn off handshake send:
	vc->handshake_send = 0;
	ASSERT(vc->arq_state == ARQ_STATE_ON, "VC arq_state is not ARQ_STATE_ON, it is: %d", vc->arq_state);

	// Add something to send:
	sRing = sendRing_push_packet_to_send(vc->sendRing, vc->elementBuffer, (const uint8_t*)payload, 100);
	sky_arq_sequence_t seq = sRing;
	ret = sky_vc_content_to_send(vc, &config, 0, 5);
	ASSERT(ret == 1, "VC sendable content is not 1, it is: %d", ret);

	// Remove content to send.
	uint8_t dump[1000];
	sendRing_read_to_tx(vc->sendRing, vc->elementBuffer, dump, &seq, 1, &diag);

	// Send ring head and tx head.
	ASSERT(vc->sendRing->head == 1, "Head was not 1 it was %d", vc->sendRing->head);
	ASSERT(vc->sendRing->tx_head == 1, "TX Head was not 1 it was %d", vc->sendRing->tx_head);

	// There should be no content to send.
	ret = sky_vc_content_to_send(vc, &config, 0, 5);
	ASSERT(ret == 0, "VC sendable content is not 0, it is: %d", ret);

	// Handshake should be sent.
	vc->handshake_send = 1;
	ret = sky_vc_content_to_send(vc, &config, 0, 5);
	ASSERT(ret == 1, "VC sendable content is not 1, it is: %d", ret);

	// Handshake should not be sent.
	vc->handshake_send = 0;
	ret = sky_vc_content_to_send(vc, &config, 0, 5);
	ASSERT(ret == 0, "VC sendable content is not 0, it is: %d", ret);

	// Idle frames per window not reached:
	ret = sky_vc_content_to_send(vc, &config, 10000, 3);
	ASSERT(ret == 1, "VC sendable content is not 1, it is: %d", ret);
	ret = sky_vc_content_to_send(vc, &config, 0, 4);
	ASSERT(ret == 0, "VC sendable content is not 0, it is: %d", ret);

	// VC needs to recall:
	vc->need_recall = 1;
	ret = sky_vc_content_to_send(vc, &config, 0, 3);
	ASSERT(ret == 1, "VC sendable content is not 1, it is: %d", ret);

	// VC does not need to recall:
	vc->need_recall = 0;
	ret = sky_vc_content_to_send(vc, &config, 0, 5);
	ASSERT(ret == 0, "VC sendable content is not 0, it is: %d", ret);

	// RCV ring horizon bitmap is not 0: (PL to be read in rcv ring)
	// Add a payload to the rcv ring.
	int rRing = rcvRing_push_rx_packet(vc->rcvRing, vc->elementBuffer, (const uint8_t*)payload, 100, 2);
	ASSERT(rRing >= 0, "VC rcvRing_push_rx_packet error: %d", rRing);

	// Check that there is content to send.
	ret = sky_vc_content_to_send(vc, &config, 0, 3);
	ASSERT(ret == 1, "VC sendable content is not 1, it is: %d", ret);

	// Read the payload to not have content to send.
	rcvRing_read_next_received(vc->rcvRing, vc->elementBuffer, dump, 200);

	// Check that there is no content to send.
	ret = sky_vc_content_to_send(vc, &config, 0, 5);
	ASSERT(ret == 0, "VC sendable content is not 0, it is: %d", ret);


	sky_vc_destroy(vc);
}

/*
 * Test Filling a frame with a packet if there is something to send. Execution depends on ARQ state.
 */
TEST(fill_frame)
{
	SkyDiagnostics diag;
	// Create new virtual channel instance
	SkyConfig config = default_config;
	config.arq.idle_frames_per_window = 4;
	SkyVirtualChannel *vc = sky_vc_create(&config.vc[0]);

	SkyTransmitFrame TXframe;
	SkyRadioFrame frame;
	units_init_tx_frame(&frame, &TXframe);
	unsigned int init_len = TXframe.frame->length;

	// ARQ OFF:
	// Nothing in send ring:
	int ret = sky_vc_fill_frame(vc, &config, &TXframe, 0, 0, &diag);
	ASSERT(ret == 0, "sky_vc_fill_frame() should return 0 when arq is off and nothing to send, %d", ret);

	// Add payload to send ring.
	uint8_t payload[100]; fillrand(payload, 100);
	int sRing = sendRing_push_packet_to_send(vc->sendRing, vc->elementBuffer, (const uint8_t*)payload, 100);
	ASSERT(sRing >= 0, "VC sendRing_push_packet_to_send error: %d", sRing);

	// sky_vc_fill_frame() should be able to fill a frame.
	ret = sky_vc_fill_frame(vc, &config, &TXframe, 0, 0, &diag);
	ASSERT(ret == 1, "sky_vc_fill_frame() should return 1 when arq is off and there is something to send, %d", ret);

	// Check that frame raw is the same as the payload in for loop. Init_tx sets identity etc. so there is already some data before payload.
	//for(unsigned int i = init_len; i < TXframe.frame->length ; i++){
	//	ASSERT(TXframe.frame->raw[i] == i - init_len, "%d is not equal to %d", TXframe.frame->raw[i], i - init_len);
	//}
	ASSERT_MEMORY(&TXframe.frame->raw[init_len], payload, 100);
	// Make frame reusable.
	TXframe.ptr -= 100;
	TXframe.frame->length -= 100;
	ASSERT(TXframe.frame->length == init_len, "Frame length should be %d, it was %d", init_len, TXframe.frame->length);
	// IN INIT:
	sky_vc_wipe_to_arq_init_state(vc);
	// No idle frames to be sent, should return 0 and length should not be changed.
	ret = sky_vc_fill_frame(vc, &config, &TXframe, 0, 4, &diag);
	ASSERT(ret == 0, "There was an idle frame to be sent when there shouldn't be one.");
	// Idle frame should be sent. ARQ handshake extension
	ret = sky_vc_fill_frame(vc, &config, &TXframe, 0, 3, &diag);
	ASSERT(ret == 1, "There was no idle frame to be sent when there should be one.");
	// Check that extension was added properly to the frame by testing that length is increased by sizeof(ExtARQHandshake) + 1.
	ASSERT(TXframe.frame->length == init_len + sizeof(ExtARQHandshake) + 1, "Frame length should be %d, it was %d", init_len + sizeof(ExtARQHandshake) + 1, TXframe.frame->length);
	// Make frame reusable.
	TXframe.ptr -= sizeof(ExtARQHandshake) + 1;
	TXframe.frame->length -= sizeof(ExtARQHandshake) + 1;
	// ON:
	sky_vc_wipe_to_arq_on_state(vc, 10);
	// To check: handshake, bitmap (missing frames), ARQ control/sync, idle frames needed, something to send, payload too large.
	// Handshake should be on by default:
	ret = sky_vc_fill_frame(vc, &config, &TXframe, 0, 5, &diag);
	ASSERT(ret == 1, "There was no handshake to be sent when there should be one.");
	// Check that extension was added properly to the frame by testing that length is increased by sizeof(ExtARQHandshake) + 1.
	ASSERT(TXframe.frame->length == init_len + sizeof(ExtARQHandshake) + 1, "Frame length should be %d, it was %d", init_len + sizeof(ExtARQHandshake) + 1, TXframe.frame->length);
	// Make frame reusable.
	TXframe.ptr -= sizeof(ExtARQHandshake) + 1;
	TXframe.frame->length -= sizeof(ExtARQHandshake) + 1;

	// Add missing frame to receive ring.
	int rRing = rcvRing_push_rx_packet(vc->rcvRing, vc->elementBuffer, (const uint8_t*)payload, 100, 2);
	ASSERT(rRing >= 0, "VC rcvRing_push_rx_packet error: %d", rRing);

	// Check horizon bitmap is not 0.
	ASSERT(rcvRing_get_horizon_bitmap(vc->rcvRing) > 0, "Horizon bitmap is 0");

	// Check that arq request is sent. Frames sent this window should also be smaller than idle frames per window.
	ret = sky_vc_fill_frame(vc, &config, &TXframe, 0, 3, &diag);
	ASSERT(ret == 1, "There was no arq request to be sent when there should be one. %d" , ret);

	// Check that extension was added properly to the frame by testing that length is increased by sizeof(ExtARQRequest) + 1.
	ASSERT(TXframe.frame->length == init_len + sizeof(ExtARQReq) + 1, "Frame length should be %d, it was %d", init_len + sizeof(ExtARQReq) + 1, TXframe.frame->length);
	// Make frame reusable.
	TXframe.ptr -= sizeof(ExtARQReq) + 1;
	TXframe.frame->length -= sizeof(ExtARQReq) + 1;
	// Need idle frames: (idle frames per window is 4 Should add ARQCtrl extension.
	// Wipe RCV ring:
	sky_rcv_ring_wipe(vc->rcvRing, vc->elementBuffer, 0);
	ret = sky_vc_fill_frame(vc, &config, &TXframe, 10000, 3, &diag);
	ASSERT(ret == 1, "There was no idle frame to be sent when there should be one. %d", ret);
	// Check that extension was added properly to the frame by testing that length is increased by sizeof(ExtARQCtrl) + 1.
	ASSERT(TXframe.frame->length == init_len + sizeof(ExtARQCtrl) + 1, "Frame length should be %d, it was %d", init_len + sizeof(ExtARQCtrl) + sizeof(ExtARQSeq) + 2, TXframe.frame->length);
	// Make frame reusable.
	TXframe.ptr -= sizeof(ExtARQCtrl) + 1;
	TXframe.frame->length -= sizeof(ExtARQCtrl) + 1;
	// Something to send:
	// Add payload to send ring.
	sRing = sendRing_push_packet_to_send(vc->sendRing, vc->elementBuffer, (const uint8_t*)payload, 100);
	ASSERT(sRing >= 0, "VC sendRing_push_packet_to_send error: %d", sRing);
	// sky_vc_fill_frame() should be able to fill a frame.
	ret = sky_vc_fill_frame(vc, &config, &TXframe, 0, 4, &diag);
	ASSERT(ret == 1, "sky_vc_fill_frame() should return 1 when arq is off and there is something to send, %d", ret);
	ASSERT(TXframe.frame->length == init_len + sizeof(ExtARQCtrl) + sizeof(ExtARQSeq) + 2 + 100, "Frame length should be %d, it was %d", init_len + sizeof(ExtARQCtrl) + 1 + 100, TXframe.frame->length);
	// Check that frame raw is the same as the payload in for loop. Init_tx sets identity etc. so there is already some data before payload.
	ASSERT_MEMORY(&TXframe.frame->raw[init_len + sizeof(ExtARQCtrl) + sizeof(ExtARQSeq) + 2], payload, 100);
	// Make frame reusable.
	TXframe.ptr -= 100 + sizeof(ExtARQCtrl) + sizeof(ExtARQSeq) + 2;
	TXframe.frame->length -= 100 + sizeof(ExtARQCtrl) + sizeof(ExtARQSeq) + 2;
	ASSERT(TXframe.frame->length == init_len, "Frame length should be %d, it was %d", init_len, TXframe.frame->length);
	// Payload too large:
	// Add payload to send ring.
	fillrand(payload, 100);
	sRing = sendRing_push_packet_to_send(vc->sendRing, vc->elementBuffer, (const uint8_t*)payload, 1000);
	ASSERT(sRing >= 0, "VC sendRing_push_packet_to_send error: %d", sRing);
	// sky_vc_fill_frame() should be able to fill a frame.
	// Packets in send ring:
	ASSERT(sendRing_count_packets_to_send(vc->sendRing, 0) == 1, "There should be 1 packet in the send ring, there is %d", sendRing_count_packets_to_send(vc->sendRing, 0));
	ret = sky_vc_fill_frame(vc, &config, &TXframe, 0, 4, &diag);
	ASSERT(ret == -40, "sky_vc_fill_frame() should return -40 when the payload is too large, %d", ret);
	// Currently the payload is still in the send ring.
	// ASSERT(sendRing_count_packets_to_send(vc->sendRing, 0) == 0, "There should be 0 packets in the send ring, there is %d", sendRing_count_packets_to_send(vc->sendRing, 0));

	sky_vc_destroy(vc);
}

/*
 * Test handling a handshake. Execution depends on ARQ state.
 */
TEST(handle_handshake)
{
	// Create new virtual channel instance
	SkyConfig config = default_config;
	SkyVirtualChannel *vc = sky_vc_create(&config.vc[0]);

	// ARQ OFF, should wipe to on:
	sky_vc_wipe_to_arq_off_state(vc);
	ASSERT(vc->arq_state == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state);

	// Check that handshake is handled properly.
	int ret = sky_vc_handle_handshake(vc, ARQ_STATE_ON, 0);
	ASSERT(ret == 1, "sky_vc_handle_handshake() should return 1 when arq is off and handshake is received, %d", ret);
	ASSERT(vc->arq_state == ARQ_STATE_ON, "VC arq_state is not ARQ_STATE_ON, it is: %d", vc->arq_state);
	// IN INIT:
	//if identifier == vc->arq_session_identifier, should wipe to on with handshake send off.
	//if identifier > vc->arq_session_identifier, should wipe to on with handshake on:
	//if identifier < vc->arq_session_identifier, returns 0.
	sky_vc_wipe_to_arq_init_state(vc);
	vc->arq_session_identifier = 20;
	ASSERT(vc->arq_state == ARQ_STATE_IN_INIT, "VC arq_state is not ARQ_STATE_IN_INIT, it is: %d", vc->arq_state);
	// Check that handshake is handled properly.
	ret = sky_vc_handle_handshake(vc, ARQ_STATE_ON, 0);
	// Should return 0 as identifier is smaller.
	ASSERT(ret == 0, "sky_vc_handle_handshake() should return 0 when arq is in init and handshake is received with smaller identifier, %d", ret);
	ASSERT(vc->arq_state == ARQ_STATE_IN_INIT, "VC arq_state is not ARQ_STATE_IN_INIT, it is: %d", vc->arq_state);
	// Check that handshake is handled properly when identifier is the same:
	ret = sky_vc_handle_handshake(vc, ARQ_STATE_ON, 20);
	// Should return 1 as identifier is the same.
	ASSERT(ret == 1, "sky_vc_handle_handshake() should return 1 when arq is in init and handshake is received with same identifier, %d", ret);
	ASSERT(vc->arq_state == ARQ_STATE_ON, "VC arq_state is not ARQ_STATE_ON, it is: %d", vc->arq_state);
	ASSERT(vc->handshake_send == 0, "VC handshake_send is not 0, it is: %d", vc->handshake_send);
	// Check that handshake is handled properly when identifier is larger:
	sky_vc_wipe_to_arq_init_state(vc);
	ret = sky_vc_handle_handshake(vc, ARQ_STATE_ON, 30);
	// Should return 1 as identifier is larger.(
	ASSERT(ret == 1, "sky_vc_handle_handshake() should return 1 when arq is in init and handshake is received with larger identifier, %d", ret);
	ASSERT(vc->arq_state == ARQ_STATE_ON, "VC arq_state is not ARQ_STATE_ON, it is: %d", vc->arq_state);
	ASSERT(vc->handshake_send == 1, "VC handshake_send is not 1, it is: %d", vc->handshake_send);
	// Identifier should be updated:
	ASSERT(vc->arq_session_identifier == 30, "VC arq_session_identifier is not 30, it is: %d", vc->arq_session_identifier);
	// ON:
	// if identifier == vc->arq_session_identifier, depends on the peer state, handshake 0 if peer not in init. Returns 0.
	// else rewiped to on with handshake on.
	// ARQ already on.
	// Peer state on with same identifier:)
	ret = sky_vc_handle_handshake(vc, ARQ_STATE_ON, 30);
	ASSERT(ret == 0, "sky_vc_handle_handshake() should return 0 when arq is on and handshake is received with different identifier, %d", ret);
	ASSERT(vc->handshake_send == 0, "VC handshake_send is not 0, it is: %d", vc->handshake_send);
	// Peer state init with same identifier:
	ret = sky_vc_handle_handshake(vc, ARQ_STATE_IN_INIT, 30);
	ASSERT(ret == 0, "sky_vc_handle_handshake() should return 0 when arq is on and handshake is received with different identifier, %d", ret);
	ASSERT(vc->handshake_send == 1, "VC handshake_send is not 1, it is: %d", vc->handshake_send);
	// Different identifier:
	vc->handshake_send = 0;
	ret = sky_vc_handle_handshake(vc, ARQ_STATE_ON, 31);
	ASSERT(ret == 1, "sky_vc_handle_handshake() should return 1 when arq is on and handshake is received with different identifier, %d", ret);
	ASSERT(vc->handshake_send == 1, "VC handshake_send is not 1, it is: %d", vc->handshake_send);

	// Identifier should be updated:
	ASSERT(vc->arq_session_identifier == 31, "VC arq_session_identifier is not 31, it is: %d", vc->arq_session_identifier);

	sky_vc_destroy(vc);
}

/*
 * Test processing parsed frames. Execution depends on ARQ state.
 */
TEST(process_frame)
{
	// TODO: different arq sequence?
	// Create new virtual channel instance
	SkyConfig config = default_config;
	config.arq.idle_frames_per_window = 4;
	SkyVirtualChannel *vc = sky_vc_create(&config.vc[0]);

	// Add payload and extensions to frame
	uint8_t payload[100];
	fillrand(payload, sizeof(payload));
	int sRing = sendRing_push_packet_to_send(vc->sendRing, vc->elementBuffer, (const uint8_t*)payload, sizeof(payload));
	ASSERT(sRing >= 0, "VC sendRing_push_packet_to_send error: %d", sRing);

	// Create a frame manually.
	SkyTransmitFrame TXframe;
	SkyRadioFrame frame;
	units_init_tx_frame(&frame, &TXframe);
	sky_frame_add_extension_arq_sequence(&TXframe, 0);
	sky_frame_add_extension_arq_request(&TXframe, 1, 2);
	sky_frame_add_extension_arq_ctrl(&TXframe, 0, 0);
	sky_frame_add_extension_arq_handshake(&TXframe, 1, 2);
	ASSERT(sky_frame_extend_with_payload(&TXframe, (const uint8_t*)payload, sizeof(payload)) == SKY_RET_OK);

	// Parse generated frame
	SkyParsedFrame parsed;
	int ret = units_start_parsing(TXframe.frame, &parsed);
	ASSERT(ret == 0, "units_start_parsing() should return 0, it returned %d", ret);
	ret = sky_frame_parse_extension_headers(TXframe.frame, &parsed);
	ASSERT(ret == 0, "sky_frame_parse_extension_headers() should return 0, it returned %d", ret);
	ASSERT(parsed.payload_len == sizeof(payload));
	ASSERT_MEMORY(parsed.payload, payload, sizeof(payload));

	// ARQ OFF Just pass the payload to buffer:
	sky_vc_wipe_to_arq_off_state(vc);
	ASSERT(vc->arq_state == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state);

	// Check that payload is processed properly, has handshake so arq state should be turned on. Test ARQ ON first.
	ret = sky_vc_process_frame(vc, &parsed, 6);
	ASSERT(ret == 0, "sky_vc_process_frame() should return 0 when arq is off and payload is received, %d", ret);

	// Arq state should be on.
	ASSERT(vc->arq_state == ARQ_STATE_ON, "VC arq_state is not ARQ_STATE_ON, it is: %d", vc->arq_state);
	// Check that last tx/rx tick is updated.
	ASSERT(vc->last_tx_tick == 6, "VC last_tx_tick is not 1, it is: %d", vc->last_tx_tick);
	ASSERT(vc->last_rx_tick == 6, "VC last_rx_tick is not 1, it is: %d", vc->last_rx_tick);
	// Check that payload is in the rcv ring.
	ASSERT(vc->unconfirmed_payloads == 1, "There should be 1 payload in the unconfirmed payloads, there is %d", vc->unconfirmed_payloads);
	ASSERT(rcvRing_count_readable_packets(vc->rcvRing) == 1, "There should be 1 payload in the rcv ring, there is %d", rcvRing_count_readable_packets(vc->rcvRing));

	// Wipe ring to enable pushing with same sequence.
	sky_rcv_ring_wipe(vc->rcvRing, vc->elementBuffer, 0);
	// ARQ request with mask 2, should schedule resends using this mask.
	// Remove arq handshake extension from parsed frame.
	parsed.arq_handshake = NULL;
	// Set tx head to 2.
	vc->sendRing->tx_head = 2;
	// Check that payload is processed properly.
	ret = sky_vc_process_frame(vc, &parsed, 0);
	ASSERT(ret == 0, "sky_vc_process_frame() should return 0 when successful, %d", ret);
	// Check sendring resend list and resend count.:
	// Process frame again with new tx head to allow resend list to fill. (Sendring is wiped when processing with handshake)
	ASSERT(vc->sendRing->resend_list[0] == 1, "Resend list[0] should be 1, it is: %d", vc->sendRing->resend_list[0]);
	ASSERT(vc->sendRing->resend_count == 1, "Resend count should be 1, it is: %d", vc->sendRing->resend_count);

	sky_vc_wipe_to_arq_off_state(vc);
	// No handshake, should return 0.
	ret = sky_vc_process_frame(vc, &parsed, 0);
	ASSERT(ret == 0, "sky_vc_process_frame() should return 0 when arq is off and payload is received, %d", ret);
	// Arq state should be off.
	ASSERT(vc->arq_state == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state);
	// Check that payload is in the rcv ring.
	ASSERT(rcvRing_count_readable_packets(vc->rcvRing) == 1, "There should be 1 payload in the rcv ring, there is %d", rcvRing_count_readable_packets(vc->rcvRing));
	// Wipe ring to enable pushing with same sequence.
	sky_rcv_ring_wipe(vc->rcvRing, vc->elementBuffer, 0);
	// IN INIT Should just return 0 after checking state.:
	sky_vc_wipe_to_arq_init_state(vc);
	ASSERT(vc->arq_state == ARQ_STATE_IN_INIT, "VC arq_state is not ARQ_STATE_IN_INIT, it is: %d", vc->arq_state);
	// Check that payload is processed properly.
	ret = sky_vc_process_frame(vc, &parsed, 0);
	ASSERT(ret == 0, "sky_vc_process_frame() should return 0 when arq is in init and payload is received, %d", ret);

	sky_vc_destroy(vc);
}

// Possible bug note. If a payload is too long, is there any way to remove it except wiping?
// tx_head is not incremented due to possibly premature error return so it will be stuck in the send ring.
// Should large payloads be prevented when pushing to send ring or should they be handled in a different way?