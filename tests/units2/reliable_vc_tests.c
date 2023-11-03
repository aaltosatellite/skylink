// Tests for skylink reliable virtual channel implementation.

#include "units.h"

// Test getting sky state.
TEST(sky_state){
    // Create config
    SkyConfig* config = malloc(sizeof(SkyConfig));
    default_config(config);
    SkyHandle handle = sky_create(config);
    SkyState* state = malloc(sizeof(SkyState));
    sky_get_state(handle, state);
    // Loop all vc's for initial state
    for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++){
        ASSERT(state->vc[i].state == ARQ_STATE_OFF, "VC: %d state: %d", i, state->vc[i].state);
        ASSERT(state->vc[i].tx_frames == 0, "VC: %d tx_frames: %d", i, state->vc[i].tx_frames);
        ASSERT(state->vc[i].rx_frames == 0, "VC: %d rx_frames: %d", i, state->vc[i].rx_frames);
        ASSERT(state->vc[i].free_tx_slots == (config->vc[i].send_ring_len-1), "VC: %d free_tx_slots: %d", i, (config->vc[i].send_ring_len-1));
        ASSERT(state->vc[i].session_identifier == handle->virtual_channels[i]->arq_session_identifier, "VC: %d session_identifier: %d", i, state->vc[i].session_identifier);
    }
    // Time to add some tx and rx frames.
    // Add 1 tx frame to vc 0
    uint8_t *pl = create_payload(100);
    const uint8_t *const_pl = pl;
    for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++){
        for (int j = 0; j < i+1; j++){
            int sRing = sendRing_push_packet_to_send(handle->virtual_channels[i]->sendRing, handle->virtual_channels[i]->elementBuffer, const_pl, 100);
            int rRing = rcvRing_push_rx_packet(handle->virtual_channels[i]->rcvRing, handle->virtual_channels[i]->elementBuffer, const_pl, 100, j);
            ASSERT(sRing >= 0, "VC: %d sendRing_push_packet_to_send error: %d, J: %d", i, sRing, j);
            ASSERT(rRing >= 0, "VC: %d rcvRing_push_rx_packet error: %d, J: %d", i, rRing, j);
        }
    }
    sky_get_state(handle, state);
    // Loop all vc's for updated state
    for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++){
        ASSERT(state->vc[i].state == ARQ_STATE_OFF, "VC: %d state: %d", i, state->vc[i].state);
        ASSERT(state->vc[i].tx_frames == i+1, "VC: %d tx_frames: %d", i, state->vc[i].tx_frames);
        ASSERT(state->vc[i].rx_frames == i+1, "VC: %d rx_frames: %d", i, state->vc[i].rx_frames);
        ASSERT(state->vc[i].free_tx_slots == (config->vc[i].send_ring_len-1)-(i+1), "VC: %d free_tx_slots: %d", i, (config->vc[i].send_ring_len-1)-(i+1));
        ASSERT(state->vc[i].session_identifier == handle->virtual_channels[i]->arq_session_identifier, "VC: %d session_identifier: %d", i, state->vc[i].session_identifier);
    }
    // Free payload.
    free(pl);
    // Free config.
    free(config);
    // Free state
    free(state);
    // Destroy handle.
    sky_destroy(handle);
}

// Test creating a virtual channel. Check that it is created correctly.
TEST(vc_create){
    // Valid config.
    SkyVCConfig *vcConfig = malloc(sizeof(SkyVCConfig));
    vcConfig->send_ring_len = 10;
    vcConfig->rcv_ring_len = 10;
    vcConfig->horizon_width = 4;
    vcConfig->require_authentication = 1;
    vcConfig->usable_element_size = 200;

    // Create VC.
    SkyVirtualChannel *vc = sky_vc_create(vcConfig);
    ASSERT(vc != NULL, "VC is NULL");
    ASSERT(vc->sendRing != NULL, "VC sendRing is NULL");
    ASSERT(vc->sendRing->length == 10, "VC sendRing length is not 10, it is: %d", vc->sendRing->length);
    ASSERT(vc->rcvRing != NULL, "VC rcvRing is NULL");
    ASSERT(vc->rcvRing->length == 10, "VC rcvRing length is not 10, it is: %d", vc->rcvRing->length);
    ASSERT(vc->rcvRing->horizon_width == 4, "VC rcvRing horizon_width is not 4, it is: %d", vc->rcvRing->horizon_width);
    ASSERT(vc->elementBuffer->element_usable_space == 200, "VC elementBuffer element_usable_space is not 200, it is: %d", vc->elementBuffer->element_usable_space);
    ASSERT(vc->arq_session_identifier == 0, "VC arq_session_identifier is not 0, it is: %d", vc->arq_session_identifier);
    ASSERT(vc->arq_state_flag == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state_flag);
    ASSERT(vc->elementBuffer != NULL, "VC elementBuffer is NULL");
    ASSERT(vc->handshake_send == 0, "VC handshake_send is not 0, it is: %d", vc->handshake_send);
    ASSERT(vc->last_ctrl_send_tick == 0, "VC last_ctrl_send_tick is not 0, it is: %d", vc->last_ctrl_send_tick);
    ASSERT(vc->last_rx_tick == 0, "VC last_rx_tick is not 0, it is: %d", vc->last_rx_tick);
    ASSERT(vc->last_tx_tick == 0, "VC last_tx_tick is not 0, it is: %d", vc->last_tx_tick);
    ASSERT(vc->unconfirmed_payloads == 0, "VC unconfirmed_payloads is not 0, it is: %d", vc->unconfirmed_payloads);

    // Free VC.
    sky_vc_destroy(vc);

    // Invalid config, less than minimum values.
    vcConfig->send_ring_len = 0;
    vcConfig->rcv_ring_len = 0;
    vcConfig->horizon_width = 0;
    vcConfig->usable_element_size = 0;
    vc = sky_vc_create(vcConfig);
    ASSERT(vc != NULL, "VC is NULL");
    ASSERT(vc->sendRing != NULL, "VC sendRing is NULL");
    ASSERT(vc->sendRing->length == 32, "VC sendRing length is not 32, it is: %d", vc->sendRing->length);
    ASSERT(vc->rcvRing != NULL, "VC rcvRing is NULL");
    ASSERT(vc->rcvRing->length == 32, "VC rcvRing length is not 32, it is: %d", vc->rcvRing->length);
    ASSERT(vc->rcvRing->horizon_width == 0, "VC rcvRing horizon_width is not 29, it is: %d", vc->rcvRing->horizon_width);
    ASSERT(vc->elementBuffer->element_usable_space == 32, "VC elementBuffer element_usable_space is not 32, it is: %d", vc->elementBuffer->element_usable_space);
    ASSERT(vc->arq_session_identifier == 0, "VC arq_session_identifier is not 0, it is: %d", vc->arq_session_identifier);
    ASSERT(vc->arq_state_flag == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state_flag);
    ASSERT(vc->elementBuffer != NULL, "VC elementBuffer is NULL");
    ASSERT(vc->handshake_send == 0, "VC handshake_send is not 0, it is: %d", vc->handshake_send);
    ASSERT(vc->last_ctrl_send_tick == 0, "VC last_ctrl_send_tick is not 0, it is: %d", vc->last_ctrl_send_tick);
    ASSERT(vc->last_rx_tick == 0, "VC last_rx_tick is not 0, it is: %d", vc->last_rx_tick);
    ASSERT(vc->last_tx_tick == 0, "VC last_tx_tick is not 0, it is: %d", vc->last_tx_tick);
    ASSERT(vc->unconfirmed_payloads == 0, "VC unconfirmed_payloads is not 0, it is: %d", vc->unconfirmed_payloads);

    // Free VC.
    sky_vc_destroy(vc);

    // Invalid config, greater than maximum values.
    vcConfig->send_ring_len = 1000;
    vcConfig->rcv_ring_len = 1000;
    vcConfig->horizon_width = 1000;
    vcConfig->usable_element_size = 1000;
    vc = sky_vc_create(vcConfig);
    ASSERT(vc != NULL, "VC is NULL");
    ASSERT(vc->sendRing != NULL, "VC sendRing is NULL");
    ASSERT(vc->sendRing->length == 32, "VC sendRing length is not 32, it is: %d", vc->sendRing->length);
    ASSERT(vc->rcvRing != NULL, "VC rcvRing is NULL");
    ASSERT(vc->rcvRing->length == 32, "VC rcvRing length is not 32, it is: %d", vc->rcvRing->length);
    ASSERT(vc->rcvRing->horizon_width == 29, "VC rcvRing horizon_width is not 29, it is: %d", vc->rcvRing->horizon_width);
    ASSERT(vc->elementBuffer->element_usable_space == 32, "VC elementBuffer element_usable_space is not 32, it is: %d", vc->elementBuffer->element_usable_space);
    ASSERT(vc->arq_session_identifier == 0, "VC arq_session_identifier is not 0, it is: %d", vc->arq_session_identifier);
    ASSERT(vc->arq_state_flag == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state_flag);
    ASSERT(vc->elementBuffer != NULL, "VC elementBuffer is NULL");
    ASSERT(vc->handshake_send == 0, "VC handshake_send is not 0, it is: %d", vc->handshake_send);
    ASSERT(vc->last_ctrl_send_tick == 0, "VC last_ctrl_send_tick is not 0, it is: %d", vc->last_ctrl_send_tick);
    ASSERT(vc->last_rx_tick == 0, "VC last_rx_tick is not 0, it is: %d", vc->last_rx_tick);
    ASSERT(vc->last_tx_tick == 0, "VC last_tx_tick is not 0, it is: %d", vc->last_tx_tick);
    ASSERT(vc->unconfirmed_payloads == 0, "VC unconfirmed_payloads is not 0, it is: %d", vc->unconfirmed_payloads);

    // Free VC
    sky_vc_destroy(vc);
    // Free config
    free(vcConfig);
}

// Test changing arq states from off to init to on and back to off.
TEST(arq_state_change){
    // Create config
    SkyVCConfig *vcConfig = malloc(sizeof(SkyVCConfig));
    vcConfig->send_ring_len = 10;
    vcConfig->rcv_ring_len = 10;
    vcConfig->horizon_width = 4;
    vcConfig->require_authentication = 1;
    vcConfig->usable_element_size = 200;

    // Create VC.
    SkyVirtualChannel *vc = sky_vc_create(vcConfig);
    
    // Check arq state is off.
    ASSERT(vc->arq_state_flag == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state_flag);

    // Change arq state to init.
    sky_vc_wipe_to_arq_init_state(vc);
    ASSERT(vc->arq_state_flag == ARQ_STATE_IN_INIT, "VC arq_state is not ARQ_STATE_IN_INIT, it is: %d", vc->arq_state_flag);
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
    ASSERT(vc->arq_state_flag == ARQ_STATE_ON, "VC arq_state is not ARQ_STATE_ON, it is: %d", vc->arq_state_flag);
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
    ASSERT(vc->arq_state_flag == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state_flag);
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
    // Free config
    free(vcConfig);
}
/*
Test checking for timeouts. Should wipe to off state if there is a timeout.
A timeout means that the current time subtracted by the last tx or rx tick is greater than the timeout value.
*/
TEST(check_timeouts){
    // Create config
    SkyVCConfig *vcConfig = malloc(sizeof(SkyVCConfig));
    vcConfig->send_ring_len = 10;
    vcConfig->rcv_ring_len = 10;
    vcConfig->horizon_width = 4;
    vcConfig->require_authentication = 1;
    vcConfig->usable_element_size = 200;

    // Create VC.
    SkyVirtualChannel *vc = sky_vc_create(vcConfig);
    // Check arq state is off.
    ASSERT(vc->arq_state_flag == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state_flag);
    // Should do nothing as arq state is off.
    // Can't really be tested since it is a void function where the only thing that happens is a return or a state change to arq off.
    sky_vc_check_timeouts(vc, 300, 120);

    // Change arq state to init.
    sky_vc_wipe_to_arq_init_state(vc);
    ASSERT(vc->arq_state_flag == ARQ_STATE_IN_INIT, "VC arq_state is not ARQ_STATE_IN_INIT, it is: %d", vc->arq_state_flag);

    // Test a situation where there is no timeout.
    sky_vc_check_timeouts(vc, 120, 300);
    // State should still be in init.
    ASSERT(vc->arq_state_flag == ARQ_STATE_IN_INIT, "VC arq_state is not ARQ_STATE_IN_INIT, it is: %d", vc->arq_state_flag);

    // Test a situation where there is a timeout.
    sky_vc_check_timeouts(vc, 300, 120);
    // State should be off.
    ASSERT(vc->arq_state_flag == ARQ_STATE_OFF, "VC arq_state is not ARQ_STATE_OFF, it is: %d", vc->arq_state_flag);

    // Free VC
    sky_vc_destroy(vc);
    // Free config
    free(vcConfig);
}
// Test checking if there is content to send in a virtual channel. Depends on arq state so test all states.
TEST(vc_content_to_send){
    // Create config
    SkyConfig *config = malloc(sizeof(SkyConfig));
    default_config(config);
    config->arq.idle_frames_per_window = 4;
    SkyHandle handle = sky_create(config);
    uint8_t tgt[1000];
    // ARQ OFF, simply tests if there is content to send.:
    // Check that there is no content to send.
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 0) == 0, "VC sendable content is not 0, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 0));
    // Add content to send.
    uint8_t *pl = create_payload(100);
    const uint8_t *const_pl = pl;
    int sRing = sendRing_push_packet_to_send(handle->virtual_channels[0]->sendRing, handle->virtual_channels[0]->elementBuffer, const_pl, 100);
    ASSERT(sRing >= 0, "VC sendRing_push_packet_to_send error: %d", sRing);
    // Check that there is content to send.
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 0) == 1, "VC sendable content is not 1, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 0));
    // ARQ IN INIT, tests if frames_sent_in_this_vc_window < config->arq.idle_frames_per_window:
    // Change arq state to init.
    sky_vc_wipe_to_arq_init_state(handle->virtual_channels[0]);
    ASSERT(handle->virtual_channels[0]->arq_state_flag == ARQ_STATE_IN_INIT, "VC arq_state is not ARQ_STATE_IN_INIT, it is: %d", handle->virtual_channels[0]->arq_state_flag);
    // Check that there is no content to send.
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5) == 0, "VC sendable content is not 0, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5));
    // Frame sent in this vc window is given as a parameter, check when this is smaller than the idle frames per window.
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 3) == 1, "VC sendable content is not 1, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 3));
    // Check when this is equal to the idle frames per window.
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 4) == 0, "VC sendable content is not 0, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 4));

    // ARQ ON:
    // Returns 1 if there is content to send or something needs to be retransmitted or we need to send a handshake or if idle frames per window is not reached.
    // 0 otherwise.
    // Change arq state to on.
    sky_vc_wipe_to_arq_on_state(handle->virtual_channels[0], 10);
    // Turn off handshake send:
    handle->virtual_channels[0]->handshake_send = 0;
    ASSERT(handle->virtual_channels[0]->arq_state_flag == ARQ_STATE_ON, "VC arq_state is not ARQ_STATE_ON, it is: %d", handle->virtual_channels[0]->arq_state_flag);
    // Add something to send:
    sRing = sendRing_push_packet_to_send(handle->virtual_channels[0]->sendRing, handle->virtual_channels[0]->elementBuffer, const_pl, 100);
    sky_arq_sequence_t seq = sRing;
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5) == 1, "VC sendable content is not 1, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5));
    // Remove content to send.
    sendRing_read_to_tx(handle->virtual_channels[0]->sendRing, handle->virtual_channels[0]->elementBuffer, tgt, &seq, 0);
    // Send ring head and tx head.
    ASSERT(handle->virtual_channels[0]->sendRing->head == 1, "Head was not 1 it was %d", handle->virtual_channels[0]->sendRing->head);
    ASSERT(handle->virtual_channels[0]->sendRing->tx_head == 1, "TX Head was not 1 it was %d", handle->virtual_channels[0]->sendRing->tx_head);
    // There should be no content to send.
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5) == 0, "VC sendable content is not 0, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5));
    // Handshake should be sent.
    handle->virtual_channels[0]->handshake_send = 1;
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5) == 1, "VC sendable content is not 1, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5));
    // Handshake should not be sent.
    handle->virtual_channels[0]->handshake_send = 0;
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5) == 0, "VC sendable content is not 0, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5));
    // Idle frames per window not reached:
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 10000, 3) == 1, "VC sendable content is not 1, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 3));
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 4) == 0, "VC sendable content is not 0, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 4));
    // VC needs to recall:
    handle->virtual_channels[0]->need_recall = 1;
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 3) == 1, "VC sendable content is not 1, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5));
    // VC does not need to recall:
    handle->virtual_channels[0]->need_recall = 0;
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5) == 0, "VC sendable content is not 0, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5));
    // RCV ring horizon bitmap is not 0: (PL to be read in rcv ring)
    // Add a payload to the rcv ring.
    int rRing = rcvRing_push_rx_packet(handle->virtual_channels[0]->rcvRing, handle->virtual_channels[0]->elementBuffer, const_pl, 100, 2);
    ASSERT(rRing >= 0, "VC rcvRing_push_rx_packet error: %d", rRing);
    // Check that there is content to send.
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 3) == 1, "VC sendable content is not 1, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5));
    // Read the payload to not have content to send.
    rcvRing_read_next_received(handle->virtual_channels[0]->rcvRing, handle->virtual_channels[0]->elementBuffer, tgt, 200);
    // Check that there is no content to send.
    ASSERT(sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5) == 0, "VC sendable content is not 0, it is: %d", sky_vc_content_to_send(handle->virtual_channels[0], config, 0, 5));

    // Free VC
    sky_vc_destroy(handle->virtual_channels[0]);
    // Free config
    free(config);
    // Free payload.
    free(pl);
}
// Test Filling a frame with a packet if there is something to send. Execution depends on ARQ state.
TEST(fill_frame){
    SkyConfig *config = malloc(sizeof(SkyConfig));
    default_config(config);
    config->arq.idle_frames_per_window = 4;
    SkyHandle handle = sky_create(config);
    SkyTransmitFrame TXframe;
    SkyRadioFrame frame;
    init_tx(&frame, &TXframe);
    unsigned int init_len = TXframe.frame->length;
    // ARQ OFF:
    // Nothing in send ring:
    int ret = sky_vc_fill_frame(handle->virtual_channels[0], config, &TXframe, 0, 0);
    ASSERT(ret == 0, "sky_vc_fill_frame() should return 0 when arq is off and nothing to send, %d", ret);
    // Add payload to send ring.
    uint8_t *pl = create_payload(100);
    const uint8_t *const_pl = pl;
    int sRing = sendRing_push_packet_to_send(handle->virtual_channels[0]->sendRing, handle->virtual_channels[0]->elementBuffer, const_pl, 100);
    ASSERT(sRing >= 0, "VC sendRing_push_packet_to_send error: %d", sRing);
    // sky_vc_fill_frame() should be able to fill a frame.
    ret = sky_vc_fill_frame(handle->virtual_channels[0], config, &TXframe, 0, 0);
    ASSERT(ret == 1, "sky_vc_fill_frame() should return 1 when arq is off and there is something to send, %d", ret);
    // Check that frame raw is the same as the payload in for loop. Init_tx sets identity etc. so there is already some data before payload.
    for(unsigned int i = init_len; i < TXframe.frame->length ; i++){
        ASSERT(TXframe.frame->raw[i] == i - init_len, "%d is not equal to %d", TXframe.frame->raw[i], i - init_len);
    }
    // Make frame reusable.
    TXframe.ptr -= 100;
    TXframe.frame->length -= 100;
    ASSERT(TXframe.frame->length == init_len, "Frame length should be %d, it was %d", init_len, TXframe.frame->length);
    // IN INIT:
    sky_vc_wipe_to_arq_init_state(handle->virtual_channels[0]);
    TXframe.hdr->flag_has_payload = 0;
    // No idle frames to be sent, should return 0 and length should not be changed.
    ret = sky_vc_fill_frame(handle->virtual_channels[0], config, &TXframe, 0, 4);
    ASSERT(ret == 0, "There was an idle frame to be sent when there shouldn't be one.");
    // Idle frame should be sent. ARQ handshake extension
    ret = sky_vc_fill_frame(handle->virtual_channels[0], config, &TXframe, 0, 3);
    ASSERT(ret == 1, "There was no idle frame to be sent when there should be one.");
    // Check that extension was added properly to the frame by testing that length is increased by sizeof(ExtARQHandshake) + 1.
    ASSERT(TXframe.frame->length == init_len + sizeof(ExtARQHandshake) + 1, "Frame length should be %d, it was %d", init_len + sizeof(ExtARQHandshake) + 1, TXframe.frame->length);
    // Make frame reusable.
    TXframe.ptr -= sizeof(ExtARQHandshake) + 1;
    TXframe.frame->length -= sizeof(ExtARQHandshake) + 1;
    // ON:
    sky_vc_wipe_to_arq_on_state(handle->virtual_channels[0], 10);
    
}

// Test handling a handshake. Execution depends on ARQ state.
TEST(handle_handshake){
    ASSERT(1==1);
    // ARQ OFF:

    // IN INIT:

    // ON:

}

// Test processing parsed frames. Execution depends on ARQ state.
TEST(process_frame){
    ASSERT(1==1);
    // ARQ OFF:

    // IN INIT:

    // ON:

}