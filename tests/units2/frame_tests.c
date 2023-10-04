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
			ASSERT(parsed.hdr.flag_has_payload == 1);
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
	ASSERT(parsed.arq_sequence->ARQSeq.sequence == sky_ntoh16(sequence), "Parsed sequence: %d, expected: %d", parsed.arq_sequence->ARQSeq.sequence, sky_ntoh16(sequence));
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
	ASSERT(parsed.arq_request->ARQReq.sequence == sky_ntoh16(sequence), "Parsed sequence: %d, expected: %d", parsed.arq_request->ARQReq.sequence, sky_ntoh16(sequence));
	ASSERT(parsed.arq_request->ARQReq.mask == sky_ntoh16(mask), "Parsed mask: %d, expected: %d", parsed.arq_request->ARQReq.mask, sky_ntoh16(mask));
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
	ASSERT(parsed.arq_ctrl->ARQCtrl.tx_sequence == sky_ntoh16(tx_sequence), "Parsed tx_sequence: %d, expected: %d", parsed.arq_ctrl->ARQCtrl.tx_sequence, sky_ntoh16(tx_sequence));
	ASSERT(parsed.arq_ctrl->ARQCtrl.rx_sequence == sky_ntoh16(rx_sequence), "Parsed rx_sequence: %d, expected: %d", parsed.arq_ctrl->ARQCtrl.rx_sequence, sky_ntoh16(rx_sequence));
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
	ASSERT(parsed.mac_tdd->TDDControl.window == sky_ntoh16(window));
	ASSERT(parsed.mac_tdd->TDDControl.remaining == sky_ntoh16(remaining));
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
		((has_payload) << 4) |
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