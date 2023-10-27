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
    int identity_len = (TXframe.frame->raw[0] & SKYLINK_FRAME_IDENTITY_MASK);
    // Test that everything is initialized correctly.
    ASSERT((TXframe.frame->raw[0] & SKYLINK_FRAME_VERSION_BYTE) == SKYLINK_FRAME_VERSION_BYTE, "Version byte should be %d, was %d", SKYLINK_FRAME_VERSION_BYTE, (TXframe.frame->raw[0] & SKYLINK_FRAME_VERSION_BYTE));
    // Fill from identity length to frame length with random data.
    fillrand(&TXframe.frame->raw[1 + identity_len + sizeof(SkyStaticHeader)], TXframe.frame->length - (1 + identity_len + sizeof(SkyStaticHeader)));
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
    sky_vc_wipe_to_arq_init_state(handle->virtual_channels[0]);
    ASSERT(_sky_tx_pick_vc(handle, sky_get_tick_time()) == 0, "VC picked should be 0, was %d", _sky_tx_pick_vc(handle, sky_get_tick_time()));
    ASSERT(mac_can_send(handle->mac, sky_get_tick_time()) == 1, "mac_can_send should return 1, was %d", mac_can_send(handle->mac, sky_get_tick_time()));
    int ret = sky_tx_with_golay(handle, TXframe.frame);
    ASSERT(ret == 1, "sky_tx_with_golay failed", ret);
    ret = sky_rx_with_golay(handle2, TXframe.frame);
    ASSERT(ret == 0, "sky_rx_with_golay failed with error code: %d", ret);
    // TODO: Ask about sky_tx tx_frame scope, the tx frame is set in sky_tx and its header is not placed anywhere. Header is gone?

    // Free memory.
    sky_destroy(handle);
    free(config);
}