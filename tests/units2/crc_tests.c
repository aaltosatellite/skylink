// Skylink CRC implementation tests

#include "units.h"

DECLARE_GROUP(crc);

/*
 * Extend a frame with a CRC and check it.
 */
TEST(crc_extension, frame_length)
{
	GET_PARAM(frame_length);

	// Create new protocol instance
	SkyConfig config = default_config;
	SkyHandle handle = sky_create(&config);

	// Generate a random frame
	SkyTransmitFrame tx_frame;
	SkyRadioFrame frame;
	units_init_tx_frame(&frame, &tx_frame);
	units_set_tx_frame_length(&tx_frame, frame_length);

	// Extend the frame with CRC32.
	int ret = sky_extend_with_crc32(&tx_frame);
	ASSERT(ret == SKY_RET_OK, "sky_extend_with_crc32 failed. ret=%d", ret);
	ASSERT(tx_frame.hdr->flag_crced == 1);

	// Check that the frame length is correct.
	ASSERT(tx_frame.frame->length == frame_length + sizeof(uint32_t), "CRC extension failed. Length should be %d, was %d", 200 + sizeof(uint32_t), tx_frame.frame->length);

	SkyParsedFrame parsed;
	ret = start_parsing(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Check that the CRC is correct.
	ret = sky_check_crc32(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "sky_check_crc32 failed ret=%d", ret);
	//ASSERT(parsed.hdr.flag_crced == 0);

	// Check that the frame length is correct.
	//ASSERT(frame.length == frame_length, "frame.length = %d", frame.length);

	// Check that the CRC is correct.
	//ASSERT(sky_check_crc32(&frame, &parsed) == SKY_RET_CRC_INVALID_CHECKSUM, "CRC calculation somehow passed even though it should be removed.");

	// Destroy instance
	sky_destroy(handle);
}

/*
 * Invalid CRC check.
 */
TEST(invalid_crc)
{
	// Create new protocol instance
	SkyConfig config = default_config;
	SkyHandle handle = sky_create(&config);

	// Set require authentication for VC0 to 1.
	handle->conf->vc[0].require_authentication |= SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;

	unsigned int frame_length = 200;

	// Create initialize frames.
	SkyTransmitFrame tx_frame;
	SkyRadioFrame frame;
	units_init_tx_frame(&frame, &tx_frame);
	units_set_tx_frame_length(&tx_frame, frame_length);

	sky_extend_with_crc32(&tx_frame);

	// Check that the frame length is correct.
	ASSERT(frame.length == frame_length + sizeof(uint32_t), "frame.length = %d", frame.length);

	// Corrupt the frame causing the CRC check to fail!
	corrupt_frame(&frame, 1);

	// Start parsing
	SkyParsedFrame parsed;
	int ret = start_parsing(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Check that the CRC is invalid.
	ret = sky_check_crc32(&frame, &parsed);
	ASSERT(ret == SKY_RET_CRC_INVALID_CHECKSUM, "CRC check succeeded even though it should be invalid. %d", ret);

	// Frame length should be the same.
	ASSERT(frame.length == frame_length + sizeof(uint32_t), "CRC extension failed. Frame length = %d", frame.length);

	// Test changing first byte.
	tx_frame.frame->length = 200;

	// Extend with CRC.
	ret = sky_extend_with_crc32(&tx_frame);

	// Corrupt the frame
	tx_frame.frame->raw[0] ^= 0x55;
	// Check that the CRC check fails.
	ret = sky_check_crc32(tx_frame.frame, &parsed);
	ASSERT(ret == SKY_RET_CRC_INVALID_CHECKSUM, "CRC check succeeded even though it should be invalid. %d", ret);
	// Frame length should be the same.
	ASSERT(tx_frame.frame->length == 200 + sizeof(uint32_t), "CRC extension failed. Frame length = %d", tx_frame.frame->length);

	// Destroy SkyHandle struct.
	sky_destroy(handle);
}

/*
 * Test
 */
TEST(crc_framing)
{
	// Create new protocol instance
	SkyConfig config1 = default_config;
	SkyHandle handle1 = sky_create(&config1);

	SkyConfig config2 = default_config;
	config2.identity[0] = 'b';
	SkyHandle handle2 = sky_create(&config2);

	ASSERT(1);
	SkyRadioFrame frame;

	u_int8_t *pl = create_payload(60);
    const u_int8_t *pl_const = pl;


	sendRing_push_packet_to_send(handle1->virtual_channels[0]->sendRing, handle1->virtual_channels[0]->elementBuffer, pl_const, 60);

	// Transmit the frame
	int ret = sky_tx(handle1, &frame);
	ASSERT(ret == 1, "ret: %d", ret);

	// Receive the frame
	ret = sky_rx(handle2, &frame);
	ASSERT(ret == SKY_RET_OK, "ret: %d", ret);

	// Destroy SkyHandle struct.
	sky_destroy(handle1);
	sky_destroy(handle2);
}

TEST_GROUP(crc, { crc_extension, invalid_crc, crc_framing });