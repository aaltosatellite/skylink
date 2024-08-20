#include "units.h"

// Skylink RX/TX Tests
static int _sky_tx_pick_vc(SkyHandle self, sky_tick_t now)
{
	// Loop through all virtual channels.
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i)
	{
		// Get the VC. Check VC's in order starting from round robin start, looping to 0 at SKY_NUM_VIRTUAL_CHANNELS.
		int vc = (self->mac->vc_round_robin_start + i) % SKY_NUM_VIRTUAL_CHANNELS;

		// Pending HMAC sequence reset?
		if (self->hmac->vc[vc].send_sequence_reset != 0)
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

/*
 */
TEST(tx_rx_with_golay_and_fec)
{
	// Create default config and handle.
	SkyConfig config1 = default_config;
	config1.vc[0].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_REQUIRE_SEQUENCE;
	config1.mac.idle_frames_per_window = 3; // Enable MAC idle frames.
	SkyHandle handle1 = sky_create(&config1);
	sky_hmac_set_keys(handle1, keys_a, 1);

	// Create default config and handle.
	SkyConfig config2 = default_config;
	config2.vc[0].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_REQUIRE_SEQUENCE;
	// Create another handle for the receiver.
	memcpy(config2.identity, "AAAA", 4);
	SkyHandle handle2 = sky_create(&config2);
	sky_hmac_set_keys(handle2, keys_a, 1);


	SkyTransmitFrame TXframe;
	SkyRadioFrame frame;
	init_tx(&frame, &TXframe);

	handle1->mac->last_belief_update = 0;
	sky_vc_wipe_to_arq_on_state(handle1->virtual_channels[0], 0);
	sky_vc_wipe_to_arq_on_state(handle2->virtual_channels[0], 0);

	//
	int ret = _sky_tx_pick_vc(handle1, sky_get_tick_time());
	ASSERT(ret == SKY_RET_OK, "VC picked should be 0, was %d", ret);

	//
	ret = mac_can_send(handle1->mac, sky_get_tick_time());
	ASSERT(ret == 1, "mac_can_send should return 1, was %d", ret);

	// Add a payload to the VC send ring buffer.
	uint8_t *pl = create_payload(60);
	sendRing_push_packet_to_send(handle1->virtual_channels[0]->sendRing, handle1->virtual_channels[0]->elementBuffer, (const uint8_t *)pl, 60);

	ret = sky_tx_with_golay(handle1, TXframe.frame);
	ASSERT(ret == 1, "sky_tx_with_golay failed: %d", ret);

	ret = sky_rx_with_golay(handle2, TXframe.frame);
	ASSERT(ret == 0, "sky_rx_with_golay failed with error code: %d", ret);

	// Free memory.
	free(pl);
	sky_destroy(handle1);
	sky_destroy(handle2);
}

/*
 * No golay also ARQ off
 */
TEST(tx_rx_with_fec)
{
	// Create default config and handle.
	SkyConfig config1 = default_config;
	// Make sure that sending idle frames is enabled.
	config1.mac.idle_frames_per_window = 3;
	// Create SkyHandle struct.
	config1.vc[0].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_REQUIRE_SEQUENCE;
	SkyHandle handle1 = sky_create(&config1);
	sky_hmac_set_keys(handle1, keys_a, 1);
	handle1->mac->last_belief_update = 0;


	SkyConfig config2 = default_config;
	// Create another handle for the receiver.
	memcpy(config2.identity, "AAAA", 4);

	config2.vc[0].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_REQUIRE_SEQUENCE;
	SkyHandle handle2 = sky_create(&config2);
	sky_hmac_set_keys(handle2, keys_a, 1);


	SkyTransmitFrame TXframe;
	SkyRadioFrame frame;
	init_tx(&frame, &TXframe);


	sky_vc_wipe_to_arq_off_state(handle1->virtual_channels[0]);
	sky_vc_wipe_to_arq_off_state(handle2->virtual_channels[0]);

	int ret = _sky_tx_pick_vc(handle1, sky_get_tick_time());
	ASSERT(ret == 0, "VC picked should be 0, was %d", ret);

	ret = mac_can_send(handle1->mac, sky_get_tick_time());
	ASSERT(ret == 1, "mac_can_send should return 1, was %d", ret);

	// Add a payload to the VC send ring buffer.
	uint8_t *pl = create_payload(60);
	sendRing_push_packet_to_send(handle1->virtual_channels[0]->sendRing, handle1->virtual_channels[0]->elementBuffer, (const uint8_t *)pl, 60);

	ret = sky_tx_with_fec(handle1, TXframe.frame);
	ASSERT(ret == 1, "sky_tx_with_fec failed: %d", ret);

	ret = sky_rx_with_fec(handle2, TXframe.frame);
	ASSERT(ret == 0, "sky_rx_with_fec failed with error code: %d", ret);

	free(pl);
	// Free memory.
	sky_destroy(handle1);
	sky_destroy(handle2);
}

// No fec or golay
TEST(tx_rx)
{

	// Create default config and handle.
	SkyConfig config1 = default_config;
	config1.mac.idle_frames_per_window = 3;
	config1.vc[0].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_REQUIRE_SEQUENCE;

	SkyHandle handle1 = sky_create(&config1);
	sky_hmac_set_keys(handle1, keys_a, 1);

	handle1->mac->last_belief_update = 0;

	// Create SkyHandle struct.
	SkyConfig config2 = default_config;
	memcpy(config2.identity, "AAAA", 4);
	config2.vc[0].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_REQUIRE_SEQUENCE;
	// Make sure that sending idle frames is enabled.
	// Create another handle for the receiver.
	SkyHandle handle2 = sky_create(&config2);
	sky_hmac_set_keys(handle2, keys_a, 1);


	SkyTransmitFrame TXframe;
	SkyRadioFrame frame;
	init_tx(&frame, &TXframe);



	sky_vc_wipe_to_arq_on_state(handle1->virtual_channels[0],0);
	sky_vc_wipe_to_arq_on_state(handle2->virtual_channels[0],0);

	int ret = _sky_tx_pick_vc(handle1, sky_get_tick_time());
	ASSERT(ret == 0, "VC picked should be 0, was %d", ret);

	ret = mac_can_send(handle1->mac, sky_get_tick_time());
	ASSERT(ret == 1, "mac_can_send should return 1, was %d", ret);
	// Add a payload to the VC send ring buffer.
	uint8_t *pl = create_payload(60);
	sendRing_push_packet_to_send(handle1->virtual_channels[0]->sendRing, handle1->virtual_channels[0]->elementBuffer, (const uint8_t *)pl, 60);

	ret = sky_tx_with_fec(handle1, TXframe.frame);
	ASSERT(ret == 1, "sky_tx_with_fec failed: %d", ret);

	ret = sky_rx_with_fec(handle2, TXframe.frame);
	ASSERT(ret == 0, "sky_rx_with_fec failed with error code: %d", ret);

	// Free memory.
	free(pl);
	sky_destroy(handle1);
	sky_destroy(handle2);
}

TEST(continuous_tx_rx)
{
	// N times NOTE: pushes same package, could be changed to have different packages (Ran succesfully with n = 300000).:
	int n = 100;

	SkyTransmitFrame TXframe;
	SkyRadioFrame frame;
	init_tx(&frame, &TXframe);

	// Create default config and handle.
	SkyConfig config1 = default_config;
	config1.mac.idle_frames_per_window = 3;
	config1.vc[0].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_REQUIRE_SEQUENCE;
	SkyHandle handle1 = sky_create(&config1);
	sky_hmac_set_keys(handle1, keys_a, 1);
	handle1->mac->last_belief_update = 0;


	SkyConfig config2 = default_config;
	memcpy(config2.identity, "AAAA", 4);
	SkyHandle handle2 = sky_create(&config2);
	config2.vc[0].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_REQUIRE_SEQUENCE;
	sky_hmac_set_keys(handle2, keys_a, 1);


	sky_vc_wipe_to_arq_on_state(handle1->virtual_channels[0],0);
	sky_vc_wipe_to_arq_on_state(handle2->virtual_channels[0],0);

	int ret = _sky_tx_pick_vc(handle1, sky_get_tick_time());
	ASSERT(ret == 0, "VC picked should be 0, was %d", ret);

	ret = mac_can_send(handle1->mac, sky_get_tick_time());
	ASSERT(ret == 1, "mac_can_send should return 1, was %d", ret);

	// Add a payload to the VC send ring buffer.
	// Loop n times.
	int i = 0;
	uint8_t *pl = create_payload(60);
	while (i<n)
	{
		ret = sendRing_push_packet_to_send(handle1->virtual_channels[0]->sendRing, handle1->virtual_channels[0]->elementBuffer, (const uint8_t *)pl, 60);
		ASSERT(ret >= 0, "sendRing_push_packet_to_send failed: %d, I: %d", ret, i);

		ret = sky_tx_with_golay(handle1, TXframe.frame);
		ASSERT(ret == 1, "sky_tx_with_golay failed: %d, I: %d", ret, i);

		ret = sky_rx_with_golay(handle2, TXframe.frame);
		ASSERT(ret == 0, "sky_rx_with_golay failed with error code: %d, I: %d", ret, i);

		sendRing_clean_tail_up_to(handle1->virtual_channels[0]->sendRing, handle1->virtual_channels[0]->elementBuffer, handle1->virtual_channels[0]->sendRing->tx_sequence);
		++i;
	}


	// Free memory.
	free(pl);
	sky_destroy(handle1);
	sky_destroy(handle2);
}


// Possible bug note: If Arq state is in init / idle frame without payload is sent payload length will be set to 0 in sky_rx. (No flag set.)
// If vc requires authentication, then authentication will fail because payload length < HMAC length.
// See line 146 in skylink_rx.c in conjunction with line 151 in hmac.c.
// Also see function sky_vc_fill_frame in reliable_vc.c.