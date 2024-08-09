#include "units.h"

const int valid_extension_lengths[6] = {
	sizeof(ExtARQSeq),
	sizeof(ExtARQReq),
	sizeof(ExtARQCtrl),
	sizeof(ExtARQHandshake),
	sizeof(ExtTDDControl),
	sizeof(ExtHMACSequenceReset)
};


/*
 * Test successful parsing of each extension type
 */
TEST(successful_extension_parsing)
{
	for (int combination = 0; combination < (1 << 7) - 1; combination++)
	{
		SkyRadioFrame frame;
		SkyTransmitFrame tx_frame;
		init_tx(&frame, &tx_frame);

		if (combination & 0x01)
			sky_frame_add_extension_arq_sequence(&tx_frame, 1);
		if (combination & 0x02)
			sky_frame_add_extension_arq_request(&tx_frame, 1, 2);
		if (combination & 0x04)
			sky_frame_add_extension_arq_ctrl(&tx_frame, 1, 2);
		if (combination & 0x08)
			sky_frame_add_extension_arq_handshake(&tx_frame, 1, 2);
		if (combination & 0x10)
			sky_frame_add_extension_mac_tdd_control(&tx_frame, 100, 200);
		if (combination & 0x20)
			sky_frame_add_extension_hmac_sequence_reset(&tx_frame, 123);
		if (combination & 0x40)
			ASSERT(sky_frame_extend_with_payload(&tx_frame, (uint8_t*)"Hello world", 11) == SKY_RET_OK);

		// Start parsing the generated frame
		SkyParsedFrame parsed;
		int ret = start_parsing(&frame, &parsed);
		ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

		// Make sure extension parsing is successful
		ret = sky_frame_parse_extension_headers(&frame, &parsed);
		ASSERT(ret == SKY_RET_OK, "ret: %d, combination: %02x", ret, combination);

		if (combination & 0x01)
			ASSERT(parsed.arq_sequence != NULL);
		if (combination & 0x02)
			ASSERT(parsed.arq_request != NULL);
		if (combination & 0x04)
			ASSERT(parsed.arq_ctrl != NULL);
		if (combination & 0x08)
			ASSERT(parsed.arq_handshake != NULL);
		if (combination & 0x10)
			ASSERT(parsed.mac_tdd != NULL);
		if (combination & 0x20)
			ASSERT(parsed.hmac_reset != NULL);
		if (combination & 0x40) {
			ASSERT(parsed.payload_len == 11, "payload_len: %d combination: %02x", parsed.payload_len, combination);
			ASSERT(parsed.payload != NULL);
			ASSERT_MEMORY(parsed.payload, "Hello world", 11);
		}
	}
}

/*
 * Test adding and parsing of ARQ Sequence extension
 */
TEST(add_extension_arq_sequence)
{
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	init_tx(&frame, &tx_frame);

	sky_arq_sequence_t sequence = 123; // TODO
	int ret = sky_frame_add_extension_arq_sequence(&tx_frame, sequence);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Start parsing the generated frame
	SkyParsedFrame parsed;
	ret = start_parsing(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Make sure ARQ Sequence is parsed correctly
	ret = sky_frame_parse_extension_headers(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(parsed.arq_sequence != NULL, "Parsed frame does not contain ARQ Sequence extension");
	ASSERT(parsed.arq_sequence->ARQSeq.sequence == sky_arq_seq_hton(sequence), "Parsed sequence: %d, expected: %d", parsed.arq_sequence->ARQSeq.sequence, sky_arq_seq_hton(sequence));
}

/*
 * Test adding and parsing of ARQ Request extension
 */
TEST(add_extension_arq_request)
{
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	init_tx(&frame, &tx_frame);

	sky_arq_sequence_t sequence = 123;
	uint16_t mask = 0xF00D;

	int ret = sky_frame_add_extension_arq_request(&tx_frame, sequence, mask);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Start parsing the generated frame
	SkyParsedFrame parsed;
	ret = start_parsing(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Make sure ARQ Request is parsed correctly
	ret = sky_frame_parse_extension_headers(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d extension_length: %d", ret, tx_frame.hdr->extension_length);
	ASSERT(parsed.arq_request != NULL, "Parsed frame does not contain ARQ Request extension");
	ASSERT(parsed.arq_request->ARQReq.sequence == sky_arq_seq_hton(sequence), "Parsed sequence: %d, expected: %d", parsed.arq_request->ARQReq.sequence, sky_arq_seq_hton(sequence));
	ASSERT(parsed.arq_request->ARQReq.mask == sky_arq_mask_hton(mask), "Parsed mask: %d, expected: %d", parsed.arq_request->ARQReq.mask, sky_arq_mask_hton(mask));
}

/*
 * Test adding and parsing of ARQ Control extension
 */
TEST(add_extension_arq_ctrl)
{
	int ret;
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	init_tx(&frame, &tx_frame);

	sky_arq_sequence_t tx_sequence = 1234;
	sky_arq_sequence_t rx_sequence = 4321;

	ret = sky_frame_add_extension_arq_ctrl(&tx_frame, tx_sequence, rx_sequence);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Start parsing the generated frame
	SkyParsedFrame parsed;
	ret = start_parsing(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Make sure HMAC Control is parsed correctly
	ret = sky_frame_parse_extension_headers(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(parsed.arq_ctrl != NULL, "Parsed frame does not contain ARQ Control extension");
	ASSERT(parsed.arq_ctrl->ARQCtrl.tx_sequence == sky_arq_seq_hton(tx_sequence), "Parsed tx_sequence: %d, expected: %d", parsed.arq_ctrl->ARQCtrl.tx_sequence, sky_arq_seq_hton(tx_sequence));
	ASSERT(parsed.arq_ctrl->ARQCtrl.rx_sequence == sky_arq_seq_hton(rx_sequence), "Parsed rx_sequence: %d, expected: %d", parsed.arq_ctrl->ARQCtrl.rx_sequence, sky_arq_seq_hton(rx_sequence));
}

/*
 * Test adding and parsing of ARQ Handshake extension
 */
TEST(add_extension_arq_handshake)
{
	int ret;
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	init_tx(&frame, &tx_frame);

	uint8_t state_flag = 69;
	uint32_t identifier = 0xDEADBEEF;
	ret = sky_frame_add_extension_arq_handshake(&tx_frame, state_flag, identifier);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Start parsing the generated frame
	SkyParsedFrame parsed;
	ret = start_parsing(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Make sure ARQ Handshake extension is parsed correctly
	ret = sky_frame_parse_extension_headers(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(parsed.arq_handshake != NULL);
	ASSERT(parsed.arq_handshake->ARQHandshake.peer_state == state_flag);
	ASSERT(parsed.arq_handshake->ARQHandshake.identifier == sky_ntoh32(identifier));
}

/*
 * Test adding and parsing of MAC TDD control extension
 */
TEST(add_extension_mac_tdd_control)
{
	int ret;
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	init_tx(&frame, &tx_frame);

	uint16_t window = 1234;
	uint16_t remaining = 4321;
	ret = sky_frame_add_extension_mac_tdd_control(&tx_frame, window, remaining);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Start parsing the generated frame
	SkyParsedFrame parsed;
	ret = start_parsing(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Make sure RFF Control extension is parsed correctly
	ret = sky_frame_parse_extension_headers(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(parsed.mac_tdd != NULL);
	ASSERT(parsed.mac_tdd->TDDControl.window == sky_arq_window_ntoh(window), "Parsed window: %d, expected: %d", parsed.mac_tdd->TDDControl.window, sky_arq_seq_ntoh(window));
	ASSERT(parsed.mac_tdd->TDDControl.remaining == sky_arq_window_ntoh(remaining), "Parsed remaining: %d, expected: %d", parsed.mac_tdd->TDDControl.remaining, sky_arq_seq_ntoh(remaining));
}

/*
 * Test adding and parsing of HMAC Sequence Reset extension
 */
TEST(add_extension_hmac_sequence_reset)
{
	int ret;
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	init_tx(&frame, &tx_frame);

	uint16_t sequence = 1234;
	ret = sky_frame_add_extension_hmac_sequence_reset(&tx_frame, sequence);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Start parsing the generated frame
	SkyParsedFrame parsed;
	ret = start_parsing(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Make sure HMAC extension is parsed correctly
	ret = sky_frame_parse_extension_headers(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(parsed.hmac_reset != NULL);
	ASSERT(parsed.hmac_reset->HMACSequenceReset.sequence == sky_ntoh16(sequence));
}

/*
 * Test parsing of all invalid extension types
 */
TEST(unknown_extension_type)
{
	for (int ext_type = 6; ext_type < 16; ext_type++)
	{
		// Empty frame
		int ret;
		SkyRadioFrame frame;
		SkyTransmitFrame tx_frame;
		init_tx(&frame, &tx_frame);

		// Add invalid extension type
		unsigned int ext_len = randint_i32(0, 15);
		SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame.ptr;
		extension->type = ext_type;
		extension->length = ext_len;
		fillrand(&extension->ARQSeq, ext_len);
		tx_frame.hdr->extension_length += 1 + ext_len;
		tx_frame.ptr += 1 + ext_len;
		frame.length += 1 + ext_len;

		// Start parsing the generated frame
		SkyParsedFrame parsed;
		ret = start_parsing(&frame, &parsed);
		ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

		// Make sure extension parsing fails
		ret = sky_frame_parse_extension_headers(&frame, &parsed);
		ASSERT(ret == SKY_RET_INVALID_EXT_TYPE, "ret: %d, ext_type: %d, ext_len: %d", ret, ext_type, ext_len);
	}
}

/*
 * Try to parse frame with to extensions of same type.
 */
TEST(extension_present_twice)
{
	for (int ext_type = 0; ext_type < 6; ext_type++)
	{
		int ret;
		SkyRadioFrame frame;
		SkyTransmitFrame tx_frame;
		init_tx(&frame, &tx_frame);

		int ext_len = valid_extension_lengths[ext_type];

		// Add extension
		SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame.ptr;
		extension->type = ext_type;
		extension->length = ext_len;
		fillrand(&extension->ARQSeq, ext_len);
		tx_frame.hdr->extension_length += 1 + ext_len;
		tx_frame.ptr += 1 + ext_len;
		frame.length += 1 + ext_len;

		// Add same extension second time
		extension = (SkyHeaderExtension *)tx_frame.ptr;
		extension->type = ext_type;
		extension->length = ext_len;
		fillrand(&extension->ARQSeq, ext_len);
		tx_frame.hdr->extension_length += 1 + ext_len;
		frame.length += 1 + ext_len;
		tx_frame.ptr += 1 + ext_len;

		// Start parsing the generated frame
		SkyParsedFrame parsed;
		ret = start_parsing(&frame, &parsed);
		ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

		// Make sure extension parsing fails
		ret = sky_frame_parse_extension_headers(&frame, &parsed);
		ASSERT(ret == SKY_RET_REDUNDANT_EXTENSIONS, "ret: %d, ext_type: %d, ext_len: %d", ret, ext_type, ext_len);
	}
}

/*
 * Test extension parsing by feeding the `sky_frame_parse_extension_headers` function with
 * frames having all possible extension types and lengths and make sure it success or fails correctly.
 */
TEST(invalid_extension_length)
{
	for (int ext_type = 0; ext_type < 6; ext_type++)
	for (int ext_len = 0; ext_len < 16; ext_len++) {

		int ret;
		// Empty frame
		SkyRadioFrame frame;
		SkyTransmitFrame tx_frame;
		init_tx(&frame, &tx_frame);

		// Add extension with invalid length
		SkyHeaderExtension *extension = (SkyHeaderExtension *)(&frame.raw[frame.length]);
		extension->type = ext_type;
		extension->length = ext_len;
		fillrand(&extension->ARQSeq, ext_len);
		tx_frame.hdr->extension_length += 1 + ext_len;
		frame.length += 1 + ext_len;

		// Start parsing the generated frame
		SkyParsedFrame parsed;
		ret = start_parsing(&frame, &parsed);
		ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

		// Test extension parsing
		ret = sky_frame_parse_extension_headers(&frame, &parsed);
		if (ext_len == valid_extension_lengths[ext_type])
			ASSERT(ret == SKY_RET_OK, "ret: %d, ext_type: %d, ext_len: %d", ret, ext_type, ext_len);
		else
			ASSERT(ret == SKY_RET_INVALID_EXT_LENGTH, "ret: %d, ext_type: %d, ext_len: %d", ret, ext_type, ext_len);
	}
}


/*
 * Try to parse frame which is not long enough and to have
 */
TEST(too_short_frame_during_extension_parsing)
{
	const unsigned int truncations[] = { 1 };
	for (int ext_type = 0; ext_type < 6; ext_type++)
	for (int ti = 1; ti < ARRAY_SZ(truncations); ti++)
	{
		int ret;
		SkyRadioFrame frame;
		SkyTransmitFrame tx_frame;
		init_tx(&frame, &tx_frame);

		int ext_len = valid_extension_lengths[ext_type];

		// Add extension
		SkyHeaderExtension *extension = (SkyHeaderExtension *)(&frame.raw[frame.length]);
		extension->type = ext_type;
		extension->length = ext_len;
		fillrand(&extension->ARQSeq, ext_len);
		tx_frame.hdr->extension_length += 1 + ext_len;
		frame.length += 1 + ext_len;

		// Not enough bytes!
		frame.length -= truncations[ti];

		// Start parsing the generated frame
		SkyParsedFrame parsed;
		ret = start_parsing(&frame, &parsed);
		ASSERT(ret == SKY_RET_OK, "ret: %d truncation: %d", ret, truncations[ti]);

		// Make sure extension parsing fails
		ret = sky_frame_parse_extension_headers(&frame, &parsed);
		ASSERT(ret == SKY_RET_INVALID_EXT_LENGTH, "ret: %d, ext_type: %d, ext_len: %d", ret, ext_type, ext_len);
	}
}

TEST(extension_effects){
	// Test that when different extensions are received, skylink acts accordingly
	/*
	Extension types:
		ARQ Sequence
		ARQ Request
		ARQ Control
		ARQ Handshake
		MAC TDD Control
		HMAC Sequence Reset: 
	*/
	SkyRadioFrame frame;
	SkyConfig config;
    default_config(&config);
	SkyConfig config2;
	default_config(&config2);
	// Handle of tx side
    SkyHandle handle = sky_create(&config);
	memcpy(&config2.identity, "abcdefg", 7);
	config2.identity_len = 7;
	// Payload to add to have content to send.
	u_int8_t *pl = create_payload(60);
    const u_int8_t *pl_const = pl;
	// Handle of rx side
	SkyHandle handle2 = sky_create(&config2);

	// Add extension types through sky_tx and receive them through sky_rx, check that extensions have the desired effect in the receiving side.

	// Extension 1: HMAC Sequence Reset
	// Should be naturally added when this is set and sky_tx is called
	handle->hmac->vc_enforcement_need[0] = 1;
	handle->hmac->sequence_rx[0] = 1234; // Handle2 shoulld set its tx sequence to 1234 + 3 = 1237
	sendRing_push_packet_to_send(handle->virtual_channels[0]->sendRing, handle->virtual_channels[0]->elementBuffer, pl_const, 60);
	int ret = sky_tx(handle, &frame);
	ASSERT(ret == 1, "ret: %d", ret);
	// Receive the frame
	ret = sky_rx(handle2, &frame);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	// Check that the sequence is set correctly
	ASSERT(handle2->hmac->sequence_tx[0] == 1237, "HMAC Sequence Reset failed, expected: 1237, got: %d", handle2->hmac->sequence_tx[0]);

	// Extension 2: ARQ Handshake
	// Should be naturally added when this is set and sky_tx is called
	sky_vc_wipe_to_arq_init_state(handle->virtual_channels[0]);
	handle->virtual_channels[0]->handshake_send = 1;


	// Send the frame
	handle->mac->frames_sent_in_current_window_per_vc[0] = 0;
	ret = sky_tx(handle, &frame);
	ASSERT(ret == 1, "ret: %d", ret);
	// Receive the frame
	ret = sky_rx(handle2, &frame);

	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	// Check that ARQ State has changed
	ASSERT(handle2->virtual_channels[0]->arq_state_flag == ARQ_STATE_ON, "ARQ Handshake failed, expected: 2, got: %d", handle2->virtual_channels[0]->arq_state_flag);

	// Also send handshake back
	sendRing_push_packet_to_send(handle2->virtual_channels[0]->sendRing, handle2->virtual_channels[0]->elementBuffer, pl_const, 60);
	// Tick by half a cycle to have handle2's window open
	sky_tick_t now = 1000;
	now += mac_time_to_own_window(handle2->mac, now);
	sky_tick(now);
	printf("Now: %d\n", now);
	ret = sky_tx(handle2, &frame);
	ASSERT(ret == 1, "ret: %d", ret);
	// Receive the frame
	ret = sky_rx(handle, &frame);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	// Check that ARQ State has changed
	ASSERT(handle->virtual_channels[0]->arq_state_flag == ARQ_STATE_ON, "ARQ Handshake failed, expected: 2, got: %d", handle->virtual_channels[0]->arq_state_flag);
	// Read packet from handshake
	rcvRing_read_next_received(handle->virtual_channels[0]->rcvRing, handle->virtual_channels[0]->elementBuffer, pl, 60);

	// Extension 3: ARQ Control, extension 4: ARQ Request and extension 5: ARQ Sequence

	// ARQ sequence gets tested automatically, since it handles all of the sequence numbers within the rcvRing.

	// ARQ control sends sendRing->tx_sequence and rcvRing->head_sequence
	

	for(int i = 0; i < 6; i++){
		sendRing_push_packet_to_send(handle2->virtual_channels[0]->sendRing, handle2->virtual_channels[0]->elementBuffer, pl_const, 60);
		ret = sky_tx(handle2, &frame);
		ASSERT(ret == 1, "ret: %d", ret);
		// To allow for ARQ Request to be sent leave out the 3rd frame (Sequence 4)
		if(i != 3){
			// Receive the frame
			ret = sky_rx(handle, &frame);
			ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
			rcvRing_read_next_received(handle->virtual_channels[0]->rcvRing, handle->virtual_channels[0]->elementBuffer, pl, 60);
		}
	}
	// Back to handle 1 window
	now += mac_time_to_own_window(handle->mac, now);
	printf("Now: %d\n", now);
	sky_tick(now);
	printf("SendRing tx_sequence: %d\n", handle->virtual_channels[0]->sendRing->tx_sequence);
	printf("rcvRing head_sequence: %d\n", handle->virtual_channels[0]->rcvRing->head_sequence);
	sendRing_push_packet_to_send(handle->virtual_channels[0]->sendRing, handle->virtual_channels[0]->elementBuffer, pl_const, 60);
	ret = sky_tx(handle, &frame);
	printf("ret: %d\n", ret);
	ASSERT(ret == 1, "ret: %d", ret);
	// Receive the frame
	ret = sky_rx(handle2, &frame);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	// Should be 6 since 5 frames were read in loop and 1 before it.
	ASSERT(handle2->virtual_channels[0]->sendRing->tx_sequence == 7, "tx_sequence: %d", handle2->virtual_channels[0]->sendRing->tail_sequence);
	// Head sequence of handle rcvRing should be 4 since packet 4 was not received
	ASSERT(handle->virtual_channels[0]->rcvRing->head_sequence == 4, "head_sequence: %d", handle2->virtual_channels[0]->rcvRing->tail_sequence);
	now += mac_time_to_own_window(handle2->mac, now);
	sky_tick(now);
	ret = sky_tx(handle2, &frame);
	ASSERT(ret == 1, "ret: %d", ret);
	ret = sky_rx(handle, &frame);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	// rx sync should be done in handle 1 and its need recall should be set. (Handle 2 has not read any of its received packets so its rcvRing head sequence hasnt moved)
	// Recall itself is tested elsewhere to not overcomplicate this test
	ASSERT(handle->virtual_channels[0]->need_recall == 1, "need_recall: %d", handle->virtual_channels[0]->need_recall);
	rcvRing_read_next_received(handle->virtual_channels[0]->rcvRing, handle->virtual_channels[0]->elementBuffer, pl, 60);
	// Should now have sent package 4 and head sequence should be moved to 7
	ASSERT(handle->virtual_channels[0]->rcvRing->head_sequence == 7, "head_sequence: %d", handle->virtual_channels[0]->rcvRing->tail_sequence);
	now += mac_time_to_own_window(handle->mac, now);
	sky_tick(now);
	printf("Now: %d\n", now);

	// Extension 6: MAC TDD Control

	mac_expand_window(handle->mac, now);

	// Send the frame
	ret = sky_tx(handle, &frame);
	printf("Handle 2 T0: %d\n", handle2->mac->T0);
	ASSERT(ret == 1, "ret: %d", ret);
	frame.rx_time_ticks = now;
	// Receive the frame
	ret = sky_rx(handle2, &frame);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	// Check that MAC TDD Control has changed the window
	ASSERT(handle2->mac->peer_window_length == handle2->conf->mac.minimum_window_length_ticks + handle2->conf->mac.window_adjust_increment_ticks, "MAC TDD Control failed, expected: %d, got: %d", handle2->conf->mac.minimum_window_length_ticks + handle2->conf->mac.window_adjust_increment_ticks, handle2->mac->peer_window_length);
	// T0 of handle 2 should be set to implied T0. (frame time (now) + handle 1 window remaining + tail constant ticks) - updated cycle
	// Start of window so window remaining is the window length
	ASSERT(handle2->mac->T0 == now + handle->mac->my_window_length + handle2->conf->mac.tail_constant_ticks - get_cycle(handle2->mac), "T0: %d, should be: %d", handle2->mac->T0, now + handle2->mac->peer_window_length + handle2->conf->mac.tail_constant_ticks - get_cycle(handle2->mac));
	free(pl);

}

#if 0
/*
 */
TEST(manual_decoding)
{

	SkyRadioFrame frame;
	const frame[] = {
		SKYLINK_FRAME_VERSION_BYTE | 3,
		'a', 'b', 'c', // Identity
		0 | // Flags MSB
		((sequence_control) << 2) |
		((authenticated) << 3) |
		((arq_on) << 2) |
		((vc) << 0), // Flags LSB
		0xFF & (sequence >> 8), // Frame Sequence MSB
		0xFF & (sequence >> 8), // Frame Sequence LSB
		0 // Extension header length
	};

	SkyParsedFrame parsed;
	int ret = start_parsing(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

}
#endif