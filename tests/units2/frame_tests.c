#include "narwhal.h"
#include "skylink/skylink.h"
#include "skylink/frame.h"
#include "skylink/fec.h"
#include "skylink/utilities.h"
#include "skylink/reliable_vc.h"



const int valid_extension_lengths[6] = {
	sizeof(ExtARQSeq),
	sizeof(ExtARQReq),
	sizeof(ExtARQCtrl),
	sizeof(ExtARQHandshake),
	sizeof(ExtTDDControl),
	sizeof(ExtHMACSequenceReset)
};



void init_tx(SkyRadioFrame *frame, SkyTransmitFrame *tx_frame)
{
	tx_frame->frame = frame;
	memset(frame, 0, sizeof(SkyRadioFrame));

	// Start byte
	int identity_len = randint_i32(1, SKY_MAX_IDENTITY_LEN);
	frame->raw[0] = (SKYLINK_FRAME_VERSION_BYTE | identity_len);

	// Random indentity
	fillrand(&frame->raw[1], identity_len);
	frame->length = 1 + identity_len + sizeof(SkyStaticHeader);

	//
	tx_frame->hdr = (SkyStaticHeader*)&frame->raw[1 + identity_len];
	tx_frame->ptr = &frame->raw[1 + identity_len + sizeof(SkyStaticHeader)];
}


void start_parsing(SkyRadioFrame *frame, SkyParsedFrame *parsed)
{
	memset(parsed, 0, sizeof(SkyParsedFrame));
	parsed->identity_len = (SKYLINK_FRAME_IDENTITY_MASK & frame->raw[0]);
	parsed->identity = &frame->raw[1];
	memcpy(&parsed->hdr, &frame->raw[1 + parsed->identity_len], sizeof(SkyStaticHeader));
}


/*
 * Test succesfull parsing of each extension type
 */
TEST(successful_extension_parsing)
{
	for (int combination = 0; combination < (1 << 6) - 1; combination++)
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

		SkyParsedFrame parsed;
		start_parsing(&frame, &parsed);

		// Make sure extension parsing is succesfull
		int ret = sky_frame_parse_extension_headers(&frame, &parsed);
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

	sky_arq_sequence_t sequence = 0; // TODO
	int ret = sky_frame_add_extension_arq_sequence(&tx_frame, sequence);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	SkyParsedFrame parsed;
	start_parsing(&frame, &parsed);

	ret = sky_frame_parse_extension_headers(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(parsed.arq_sequence != NULL);
	ASSERT(parsed.arq_sequence->ARQSeq.sequence == sky_ntoh16(sequence));
}

/*
 * Test adding and parsing of ARQ Request extension
 */
TEST(add_extension_arq_request)
{
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	init_tx(&frame, &tx_frame);

	sky_arq_sequence_t sequence = 0; // TODO
	uint16_t mask = 0;  // TODO

	int ret = sky_frame_add_extension_arq_request(&tx_frame, sequence, mask);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	SkyParsedFrame parsed;
	start_parsing(&frame, &parsed);

	ret = sky_frame_parse_extension_headers(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d extension_length: %d", ret, tx_frame.hdr->extension_length);

	ASSERT(parsed.arq_request != NULL);
	ASSERT(parsed.arq_request->ARQReq.sequence == sequence);
	ASSERT(parsed.arq_request->ARQReq.mask == mask);
}

/*
 * Test adding and parsing of ARQ Control extension
 */
TEST(add_extension_arq_ctrl)
{
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	init_tx(&frame, &tx_frame);

	sky_arq_sequence_t tx_sequence = 0; // TODO
	sky_arq_sequence_t rx_sequence = 0; // TODO

	int ret = sky_frame_add_extension_arq_ctrl(&tx_frame, tx_sequence, rx_sequence);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	SkyParsedFrame parsed;
	start_parsing(&frame, &parsed);

	ret = sky_frame_parse_extension_headers(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(parsed.arq_ctrl != NULL);
	ASSERT(parsed.arq_ctrl->ARQCtrl.tx_sequence == tx_sequence);
	ASSERT(parsed.arq_ctrl->ARQCtrl.rx_sequence == rx_sequence);
}

/*
 * Test adding and parsing of ARQ Handshake extension
 */
TEST(add_extension_arq_handshake)
{
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	init_tx(&frame, &tx_frame);

	uint8_t state_flag = 0;	 // TODO
	uint32_t identifier = 0; // TODO
	int ret = sky_frame_add_extension_arq_handshake(&tx_frame, state_flag, identifier);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	SkyParsedFrame parsed;
	start_parsing(&frame, &parsed);

	ret = sky_frame_parse_extension_headers(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(parsed.arq_handshake != NULL);
	ASSERT(parsed.arq_handshake->ARQHandshake.peer_state == state_flag);
	ASSERT(parsed.arq_handshake->ARQHandshake.identifier == identifier);
}

/*
 * Test adding and parsing of MAC TDD control extension
 */
TEST(add_extension_mac_tdd_control)
{
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	init_tx(&frame, &tx_frame);

	uint16_t window = 0;
	uint16_t remaining = 0;
	int ret = sky_frame_add_extension_mac_tdd_control(&tx_frame, window, remaining);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	SkyParsedFrame parsed;
	start_parsing(&frame, &parsed);

	ret = sky_frame_parse_extension_headers(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(parsed.mac_tdd != NULL);
	ASSERT(parsed.mac_tdd->TDDControl.window == window);
	ASSERT(parsed.mac_tdd->TDDControl.remaining == remaining);
}

/*
 * Test adding and parsing of HMAC Sequence Reset extension
 */
TEST(add_extension_hmac_sequence_reset)
{
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	init_tx(&frame, &tx_frame);

	uint16_t sequence = 0;
	int ret = sky_frame_add_extension_hmac_sequence_reset(&tx_frame, sequence);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	SkyParsedFrame parsed;
	start_parsing(&frame, &parsed);

	ret = sky_frame_parse_extension_headers(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);
	ASSERT(parsed.hmac_reset != NULL);
	ASSERT(parsed.hmac_reset->HMACSequenceReset.sequence == sequence);
}

/*
 * Test parsing of all invalid extension types
 */
TEST(unknown_extension_type)
{
	for (int ext_type = 6; ext_type < 16; ext_type++)
	{
		// Empty frame
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

		SkyParsedFrame parsed;
		start_parsing(&frame, &parsed);

		// Make sure extension parsing fails
		int ret = sky_frame_parse_extension_headers(&frame, &parsed);
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

		SkyParsedFrame parsed;
		start_parsing(&frame, &parsed);

		// Make sure extension parsing fails
		int ret = sky_frame_parse_extension_headers(&frame, &parsed);
		ASSERT(ret != SKY_RET_OK, "ret: %d, ext_type: %d, ext_len: %d", ret, ext_type, ext_len);
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

		SkyParsedFrame parsed;
		start_parsing(&frame, &parsed);

		// Test extension parsing
		int ret = sky_frame_parse_extension_headers(&frame, &parsed);
		if (ext_len == valid_extension_lengths[ext_type])
			ASSERT(ret == SKY_RET_OK, "ret: %d, ext_type: %d, ext_len: %d", ret, ext_type, ext_len);
		else
			ASSERT(ret == SKY_RET_INVALID_EXT_LENGTH, "ret: %d, ext_type: %d, ext_len: %d", ret, ext_type, ext_len);
	}
}

/*
 * Try to parse
 */
TEST(too_short_frame_during_extension_parsing)
{
	for (int ext_type = 0; ext_type < 6; ext_type++)
	{
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
		//frame.length -= randint_i32(1, 5);

		SkyParsedFrame parsed;
		start_parsing(&frame, &parsed);

		// Make sure extension parsing fails
		int ret = sky_frame_parse_extension_headers(&frame, &parsed);
		ASSERT(ret == SKY_RET_INVALID_EXT_LENGTH, "ret: %d, ext_type: %d, ext_len: %d", ret, ext_type, ext_len);
	}
}
