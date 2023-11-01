#include "units.h"
// Skylink RX/TX Tests
static int _sky_tx_pick_vc(SkyHandle self, sky_tick_t now)
{
	// Loop through all virtual channels.
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i)
	{
		// Get the VC. Check VC's in order starting from round robin start, looping to 0 at SKY_NUM_VIRTUAL_CHANNELS.
		int vc = (self->mac->vc_round_robin_start + i) % SKY_NUM_VIRTUAL_CHANNELS;

		// Pending HMAC sequence enforcement?
		if (self->hmac->vc_enforcement_need[vc] != 0)
			return vc;

		// Something in the buffer?
		if (sky_vc_content_to_send(self->virtual_channels[vc], self->conf, now, self->mac->frames_sent_in_current_window_per_vc[vc]) > 0)
			return vc;
	}

	// This is here to ensure that the peer advances its window through TDD gap even if there are no messages to send
	if (mac_idle_frame_needed(self->mac, now))
		return 0;

	// No need to transmit.
	return -1;
}
// SkyRadioFrame *create_frame()
// {
//     SkyRadioFrame *frame = SKY_MALLOC(sizeof(SkyRadioFrame));
//     frame->length = 0;
//     // Add valid identity.
//     // VERSION_BYTE
//     frame->raw[0] = SKYLINK_FRAME_VERSION_BYTE;
//     frame->length += 1;
//     // Header

//     return frame;
// }
TEST(tx_rx_with_golay_and_fec){
    SkyTransmitFrame TXframe;
    SkyRadioFrame frame;
    init_tx(&frame, &TXframe);
    // Test that everything is initialized correctly.
    ASSERT((TXframe.frame->raw[0] & SKYLINK_FRAME_VERSION_BYTE) == SKYLINK_FRAME_VERSION_BYTE, "Version byte should be %d, was %d", SKYLINK_FRAME_VERSION_BYTE, (TXframe.frame->raw[0] & SKYLINK_FRAME_VERSION_BYTE));
    // Create default config and handle.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    SkyConfig* config2 = malloc(sizeof(SkyConfig));
    default_config(config);
    default_config(config2);
    // Make sure that sending idle frames is enabled.
    config->mac.idle_frames_per_window = 3;
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create another handle for the receiver.
    memcpy(config2->identity, "AAAA", 4);
    SkyHandle handle2 = sky_create(config2);
    handle->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;
    handle2->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;
    handle->mac->last_belief_update = 0;
    sky_vc_wipe_to_arq_on_state(handle->virtual_channels[0],0);
    ASSERT(_sky_tx_pick_vc(handle, sky_get_tick_time()) == 0, "VC picked should be 0, was %d", _sky_tx_pick_vc(handle, sky_get_tick_time()));
    ASSERT(mac_can_send(handle->mac, sky_get_tick_time()) == 1, "mac_can_send should return 1, was %d", mac_can_send(handle->mac, sky_get_tick_time()));
    // Add a payload to the VC send ring buffer.
    u_int8_t *pl = create_payload(60);
    const u_int8_t *pl_const = pl;
    sendRing_push_packet_to_send(handle->virtual_channels[0]->sendRing, handle->virtual_channels[0]->elementBuffer, pl_const, 60);
    int ret = sky_tx_with_golay(handle, TXframe.frame);
    ASSERT(ret == 1, "sky_tx_with_golay failed: %d", ret);
    ret = sky_rx_with_golay(handle2, TXframe.frame);
    ASSERT(ret == 0, "sky_rx_with_golay failed with error code: %d", ret);

    // Free memory.
    free(pl);
    sky_destroy(handle);
    sky_destroy(handle2);
    free(config2);
    free(config);
}
// No golay also ARQ off
TEST(tx_rx_with_fec){
    SkyTransmitFrame TXframe;
    SkyRadioFrame frame;
    init_tx(&frame, &TXframe);
    // Test that everything is initialized correctly.
    ASSERT((TXframe.frame->raw[0] & SKYLINK_FRAME_VERSION_BYTE) == SKYLINK_FRAME_VERSION_BYTE, "Version byte should be %d, was %d", SKYLINK_FRAME_VERSION_BYTE, (TXframe.frame->raw[0] & SKYLINK_FRAME_VERSION_BYTE));
    // Create default config and handle.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    SkyConfig* config2 = malloc(sizeof(SkyConfig));
    default_config(config);
    default_config(config2);
    // Make sure that sending idle frames is enabled.
    config->mac.idle_frames_per_window = 3;
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create another handle for the receiver.
    memcpy(config2->identity, "AAAA", 4);
    SkyHandle handle2 = sky_create(config2);
    handle->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;
    handle2->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;
    handle->mac->last_belief_update = 0;
    sky_vc_wipe_to_arq_off_state(handle->virtual_channels[0]);
    ASSERT(_sky_tx_pick_vc(handle, sky_get_tick_time()) == 0, "VC picked should be 0, was %d", _sky_tx_pick_vc(handle, sky_get_tick_time()));
    ASSERT(mac_can_send(handle->mac, sky_get_tick_time()) == 1, "mac_can_send should return 1, was %d", mac_can_send(handle->mac, sky_get_tick_time()));
    // Add a payload to the VC send ring buffer.
    u_int8_t *pl = create_payload(60);
    const u_int8_t *pl_const = pl;
    sendRing_push_packet_to_send(handle->virtual_channels[0]->sendRing, handle->virtual_channels[0]->elementBuffer, pl_const, 60);
    int ret = sky_tx_with_fec(handle, TXframe.frame);
    ASSERT(ret == 1, "sky_tx_with_fec failed: %d", ret);
    ret = sky_rx_with_fec(handle2, TXframe.frame);
    ASSERT(ret == 0, "sky_rx_with_fec failed with error code: %d", ret);

    // Free memory.
    free(pl);
    sky_destroy(handle);
    sky_destroy(handle2);
    free(config2);
    free(config);
}

// No fec or golay
TEST(tx_rx){
    SkyTransmitFrame TXframe;
    SkyRadioFrame frame;
    init_tx(&frame, &TXframe);
    // Test that everything is initialized correctly.
    ASSERT((TXframe.frame->raw[0] & SKYLINK_FRAME_VERSION_BYTE) == SKYLINK_FRAME_VERSION_BYTE, "Version byte should be %d, was %d", SKYLINK_FRAME_VERSION_BYTE, (TXframe.frame->raw[0] & SKYLINK_FRAME_VERSION_BYTE));
    // Create default config and handle.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    SkyConfig* config2 = malloc(sizeof(SkyConfig));
    default_config(config);
    default_config(config2);
    // Make sure that sending idle frames is enabled.
    config->mac.idle_frames_per_window = 3;
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create another handle for the receiver.
    memcpy(config2->identity, "AAAA", 4);
    SkyHandle handle2 = sky_create(config2);
    handle->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;
    handle2->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;
    handle->mac->last_belief_update = 0;
    sky_vc_wipe_to_arq_on_state(handle->virtual_channels[0],0);
    ASSERT(_sky_tx_pick_vc(handle, sky_get_tick_time()) == 0, "VC picked should be 0, was %d", _sky_tx_pick_vc(handle, sky_get_tick_time()));
    ASSERT(mac_can_send(handle->mac, sky_get_tick_time()) == 1, "mac_can_send should return 1, was %d", mac_can_send(handle->mac, sky_get_tick_time()));
    // Add a payload to the VC send ring buffer.
    u_int8_t *pl = create_payload(60);
    const u_int8_t *pl_const = pl;
    sendRing_push_packet_to_send(handle->virtual_channels[0]->sendRing, handle->virtual_channels[0]->elementBuffer, pl_const, 60);
    int ret = sky_tx_with_fec(handle, TXframe.frame);
    ASSERT(ret == 1, "sky_tx_with_fec failed: %d", ret);
    ret = sky_rx_with_fec(handle2, TXframe.frame);
    ASSERT(ret == 0, "sky_rx_with_fec failed with error code: %d", ret);

    // Free memory.
    free(pl);
    sky_destroy(handle);
    sky_destroy(handle2);
    free(config2);
    free(config);  
}
TEST(continuous_tx_rx){
    // N times NOTE: pushes same package, could be changed to have different packages (Ran succesfully with n = 300000).:
    int n = 1000;
    SkyTransmitFrame TXframe;
    SkyRadioFrame frame;
    init_tx(&frame, &TXframe);
    // Test that everything is initialized correctly.
    ASSERT((TXframe.frame->raw[0] & SKYLINK_FRAME_VERSION_BYTE) == SKYLINK_FRAME_VERSION_BYTE, "Version byte should be %d, was %d", SKYLINK_FRAME_VERSION_BYTE, (TXframe.frame->raw[0] & SKYLINK_FRAME_VERSION_BYTE));
    // Create default config and handle.
    SkyConfig* config = malloc(sizeof(SkyConfig));
    SkyConfig* config2 = malloc(sizeof(SkyConfig));
    default_config(config);
    default_config(config2);
    // Make sure that sending idle frames is enabled.
    config->mac.idle_frames_per_window = 3;
    // Create SkyHandle struct.
    SkyHandle handle = sky_create(config);
    // Create another handle for the receiver.
    memcpy(config2->identity, "AAAA", 4);
    SkyHandle handle2 = sky_create(config2);
    handle->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;
    handle2->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;
    handle->mac->last_belief_update = 0;
    sky_vc_wipe_to_arq_on_state(handle->virtual_channels[0],0);
    ASSERT(_sky_tx_pick_vc(handle, sky_get_tick_time()) == 0, "VC picked should be 0, was %d", _sky_tx_pick_vc(handle, sky_get_tick_time()));
    ASSERT(mac_can_send(handle->mac, sky_get_tick_time()) == 1, "mac_can_send should return 1, was %d", mac_can_send(handle->mac, sky_get_tick_time()));
    // Add a payload to the VC send ring buffer.
    // Loop n times.
    int i = 0;
    u_int8_t *pl = create_payload(60);
    const u_int8_t *pl_const = pl;
    while (i<n)
    {
        int ret = sendRing_push_packet_to_send(handle->virtual_channels[0]->sendRing, handle->virtual_channels[0]->elementBuffer, pl_const, 60);
        ASSERT(ret >= 0, "sendRing_push_packet_to_send failed: %d, I: %d", ret, i);
        ret = sky_tx_with_golay(handle, TXframe.frame);
        ASSERT(ret == 1, "sky_tx_with_golay failed: %d, I: %d", ret, i);
        ret = sky_rx_with_golay(handle2, TXframe.frame);
        ASSERT(ret == 0, "sky_rx_with_golay failed with error code: %d, I: %d", ret, i);
        sendRing_clean_tail_up_to(handle->virtual_channels[0]->sendRing, handle->virtual_channels[0]->elementBuffer, handle->virtual_channels[0]->sendRing->tx_sequence);
        ++i;
    }


    // Free memory.
    free(pl);
    sky_destroy(handle);
    sky_destroy(handle2);
    free(config2);
    free(config);
}
 

// Possible bug note: If Arq state is in init / idle frame without payload is sent payload length will be set to 0 in sky_rx. (No flag set.)
// If vc requires authentication, then authentication will fail because payload length < HMAC length.
// See line 146 in skylink_rx.c in conjunction with line 151 in hmac.c.
// Also see function sky_vc_fill_frame in reliable_vc.c.