#include "units.h"
#include "ext/blake3/blake3.h"


// Make sure config flags don't overlap
static_assert((SKY_CONFIG_FLAG_AUTHENTICATE_TX & SKY_CONFIG_FLAG_REQUIRE_SEQUENCE) == 0);
static_assert((SKY_CONFIG_FLAG_AUTHENTICATE_TX & SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION) == 0);
static_assert((SKY_CONFIG_FLAG_REQUIRE_SEQUENCE & SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION) == 0);
static_assert((SKY_CONFIG_FLAG_REQUIRE_SEQUENCE & SKY_CONFIG_FLAG_USE_CRC32) == 0);
static_assert((SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION & SKY_CONFIG_FLAG_USE_CRC32) == 0);



// Test creating an HMAC context.
TEST(create_HMAC)
{
	// Malloc SkyConfig struct and set default config.
	SkyConfig config = default_config;
	// Create SkyHandle struct.
	SkyHandle handle = sky_create(&config);
	// Create SkyHMAC struct.
	SkyHMAC* hmac = sky_hmac_create(&handle->conf->hmac);
	// Check that the struct is not NULL.
	ASSERT(hmac != NULL, "Create HMAC failed.");

	sky_hmac_set_keys(handle, keys_a, 1);
	//ASSERT(handle->hmac);


	// Destroy HMAC struct.
	sky_hmac_destroy(hmac);
	// Destroy SkyHandle struct.
	sky_destroy(handle);

}


/*
 * Test that frames with different payload lengths gets extended correcly.
 */
TEST(hmac_successful, frame_length)
{
	GET_PARAM(frame_length);

	// Create new protocol instance
	SkyConfig config = default_config;
	SkyHandle handle = sky_create(&config);

	// Set require authentication for VC0 to 1.
	sky_hmac_set_keys(handle, keys_a, 1);
	handle->conf->vc[0].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;
	handle->hmac->vc[0].sequence_tx = handle->hmac->vc[0].sequence_rx = 1234;

	// Generate random frame.
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	units_init_tx_frame(&frame, &tx_frame);
	units_set_tx_frame_length(&tx_frame, frame_length);

	// Extend the frame with authentication code
	int ret = sky_hmac_extend_with_authentication(handle, &tx_frame);
	ASSERT(ret == SKY_RET_OK, "sky_hmac_extend_with_authentication() returned %d", ret);

	// Debug hash print.
	const uint8_t *frame_hash = &frame.raw[frame.length - SKY_HMAC_LENGTH];
	printf("HASH: %02x %02x %02x %02x\n", frame_hash[0], frame_hash[1], frame_hash[2], frame_hash[3]);

	// Make sure the frame length increased correctly.
	ASSERT(frame.length == frame_length + 4, "frame length = %d", frame.length);

	// Make sure the header got flagged correctly.
	ASSERT(tx_frame.hdr->flag_authenticated == 1, "Flags %d", tx_frame.hdr->flags);

	// Start parsing the generated frame
	SkyParsedFrame parsed;
	ret = units_start_parsing(&frame, &parsed);

	// Check authentication.
	ret = sky_hmac_check_authentication(handle, tx_frame.frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "sky_hmac_check_authentication() returned %d", ret);

	// Check that the payload length is correct and the length OK?, if HMAC passed, payload length should be original-4.
	//ASSERT(parsed.payload_len == payload_length, "Returned %d", parsed.payload_len);

	//ASSERT(parsed.hdr.flag_authenticated == 0, "Flags %d", parsed.hdr.flags);

	sky_destroy(handle);
}


/*
 * Test that two instances with different keys fails to authentication check.
 */
TEST(HMAC_invalid_key)
{
	// Create a new handle for transmitting
	SkyConfig config1 = default_config;
	config1.vc[0].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX;
	config1.vc[0].tx_key = config1.vc[0].rx_key = 0;
	SkyHandle handle1 = sky_create(&config1);
	sky_hmac_set_keys(handle1, keys_a, 1);

	// Create a new handle for receiving
	SkyConfig config2 = default_config;
	SkyHandle handle2 = sky_create(&config2);
	config2.vc[0].require_authentication = SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION;
	config2.vc[0].tx_key = config2.vc[0].rx_key = 0;
	sky_hmac_set_keys(handle2, keys_b, 1); // Note: Different HMAC key!

	// Make sure the sequence numbers matches
	handle1->hmac->vc[0].sequence_tx = handle2->hmac->vc[0].sequence_rx = 1234;

	// Create a new transmit frame
	SkyTransmitFrame tx_frame;
	SkyRadioFrame frame;
	units_init_tx_frame(&frame, &tx_frame);

	// Calculate hash for randomly filled data up to 200 bytes. Pointer is at byte after final written index.
	units_set_tx_frame_length(&tx_frame, 200);
	int ret = sky_hmac_extend_with_authentication(handle1, &tx_frame);
	ASSERT(ret == SKY_RET_OK, "sky_hmac_extend_with_authentication() returned %d", ret);

	// Start parsing the generated frame
	SkyParsedFrame parsed;
	ret = units_start_parsing(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "units_start_parsing() returned %d", ret);


	ASSERT(frame.length == 204, "frame.length = %d", frame.length); // Frame length OK?
	ASSERT(parsed.hdr.vc == 0, "vc = %d", parsed.hdr.vc); // VC OK?
	ASSERT(parsed.hdr.flag_authenticated == 1);

	// Check authentication.
	ret = sky_hmac_check_authentication(handle2, tx_frame.frame, &parsed);
	ASSERT(ret == SKY_RET_AUTH_FAILED, "sky_hmac_check_authentication() returned %d", ret);
	//ASSERT(parsed.payload_len == 50, "Returned %d", parsed.payload_len); // Payload length OK?, if HMAC passed, payload length should be original-4.

	// Destroy SkyHandle struct.
	sky_destroy(handle1);
	sky_destroy(handle2);
}

/*
 * Test that the HMAC is not calculated if authentication is not required.
 * Should allow differing HMAC keys to go through checking authentication.
 */
TEST(no_authentication)
{
	// Create new protocol instance for transmitting
	SkyConfig config1 = default_config;
	SkyHandle handle1 = sky_create(&config1);

	// Create new protocol instance for receiving
	SkyConfig config2 = default_config;
	SkyHandle handle2 = sky_create(&config2);

	// Set require authentication for VC0 to 0.
	config1.vc[0].require_authentication = 0;
	config2.vc[0].require_authentication = 0;
	config1.vc[0].tx_key = config1.vc[0].rx_key = 0;
	config2.vc[0].tx_key = config2.vc[0].rx_key = 0;
	// Set different HMAC keys
	sky_hmac_set_keys(handle1, keys_a, 1);
	sky_hmac_set_keys(handle2, keys_b, 1);

	// Create initialize frames.
	SkyTransmitFrame TXframe;
	SkyRadioFrame frame;
	SkyParsedFrame parsed;
	memset(&parsed, 0, sizeof(SkyParsedFrame));
	units_init_tx_frame(&frame, &TXframe);
	parsed.hdr.flag_authenticated = 1;
	parsed.hdr.vc = 0;
	parsed.payload_len = 50;
	// Calculate hash for randomly filled data up to 200 bytes. Pointer is at byte after final written index.
	TXframe.frame->length = 200;
	TXframe.ptr = &TXframe.frame->raw[200];
	sky_hmac_extend_with_authentication(handle1, &TXframe);

	// Debug hash print.
	//const uint8_t *frame_hash = &frame.raw[frame.length - SKY_HMAC_LENGTH];
	//ASSERT(0, "HASH: %02x %02x %02x %02x", frame_hash[0], frame_hash[1], frame_hash[2], frame_hash[3]);
	ASSERT(frame.length == 204, "frame.length = %d", frame.length); // Frame length OK?
	ASSERT((handle1->conf->vc[0].require_authentication & SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION) == 0);
	ASSERT((handle2->conf->vc[0].require_authentication & SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION) == 0);

	// Check authentication.
	int ret = sky_hmac_check_authentication(handle2, TXframe.frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "sky_hmac_check_authentication() returned %d", ret); // Auth should be ok even though keys are different. // Sequence check still needs to be done.

	//ASSERT(parsed.payload_len == 46, "payload_len = %d", parsed.payload_len); // Payload length OK?, if HMAC passed, payload length should be original-4.

	// Destroy SkyHandle struct.
	sky_destroy(handle1);
	sky_destroy(handle2);
}


/*
 * Send unauthenticated frame even the receiver requires authentication,
 * and check that receiver rejects the frame correctly.
 */
TEST(missing_authentication)
{

	// Check authentication.
	//int ret = sky_hmac_check_authentication(handle2, TXframe.frame, &parsed);
	//ASSERT(ret == SKY_RET_AUTH_MISSING, "sky_hmac_check_authentication() returned %d", ret); // Auth should be ok even though keys are different. // Sequence check still needs to be done.
	//ASSERT(parsed.payload_len == 46, "Returned %d", parsed.payload_len); // Payload length OK?, if HMAC passed, payload length should be original-4.

	ASSERT(1);
}


/*
 * Send authenticated frame even the receiver doesn't require authentication,
 * and check that reception was still succesfull.
 */
TEST(unrequired_authentication)
{
	ASSERT(1); // TODO:
}


/*
 * Test that frames with different payload lengths gets extended correcly.
 */
TEST(hmac_sequence_numbers)
{
	SkyConfig config = default_config;
	SkyHandle handle = sky_create(&config);

	config.hmac.maximum_jump = 0;

	ASSERT(1); // TODO:

	// for each sequence start
	// - Not increased
	// - Increased equal to {1, 4, max_jump}

	sky_destroy(handle);
}

/*
 * Test flag placement in bitfield for SkyStaticHeader of parsed frame.
 */
TEST(header_bits)
{
	SkyParsedFrame parsed;
	memset(&parsed, 0, sizeof(SkyParsedFrame));
	ASSERT(parsed.hdr.flags == 0, "Returned %d", parsed.hdr.flags);
	parsed.hdr.flag_authenticated = 1;
	ASSERT(parsed.hdr.flags == 0b00001000, "Returned %d", parsed.hdr.flags);
	parsed.hdr.flag_authenticated = 0;
	ASSERT(parsed.hdr.flags == 0, "Returned %d", parsed.hdr.flags);
	parsed.hdr.flag_arq_on = 1;
	ASSERT(parsed.hdr.flags == 0b00000100, "Returned %d", parsed.hdr.flags);
	parsed.hdr.flag_arq_on = 0;
	ASSERT(parsed.hdr.flags == 0, "Returned %d", parsed.hdr.flags);
	parsed.hdr.vc = 3;
	ASSERT(parsed.hdr.flags & 0b11, "Returned %d", parsed.hdr.flags);



/*
FIX NOTES:
Order of bit field "Flags" in SkyStaticHeader:
VC0 =                       0b00000000
VC1 =                       0b00000001
VC2 =                       0b00000010
VC3 =                       0b00000011
SKY_FLAG_ARQ_ON =           0b00000100
SKY_FLAG_AUTHENTICATED =    0b00001000
Sequence control bits =     0b01100000
Reserved =                  0b10000000
Fixed so that SKY_FLAG_ARQ_ON is now 0b00000100, SKY_FLAG_AUTHENTICATED is 0b00001000.
Previously bits were offset.
*/


}


/*
 * Test framing when both HMAC and CRC32 are enabled
 */
TEST(both_hmac_and_crc)
{
	// Create new protocol instance
	SkyConfig config = default_config;
	SkyHandle handle = sky_create(&config);
	sky_hmac_set_keys(handle, keys_a, 1);

	// Set require authentication for VC0 to 1.
	config.vc[0].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_USE_CRC32;

	unsigned int frame_length = 200;

	// Generate random tx frame.
	SkyRadioFrame frame;
	SkyTransmitFrame tx_frame;
	units_init_tx_frame(&frame, &tx_frame);
	units_set_tx_frame_length(&tx_frame, frame_length);

	ASSERT(tx_frame.hdr->flag_authenticated == 0);
	ASSERT(tx_frame.hdr->flag_crced == 0);

	// Extend the frame with authentication code
	int ret = sky_hmac_extend_with_authentication(handle, &tx_frame);
	ASSERT(ret == SKY_RET_OK, "sky_hmac_extend_with_authentication() returned %d", ret);
	ASSERT(frame.length == frame_length + 4, "frame length = %d", frame.length);
	ASSERT(tx_frame.hdr->flag_authenticated == 1);
	ASSERT(tx_frame.hdr->flag_crced == 0);

	// Extend the frame with CRC32
	ret = sky_extend_with_crc32(&tx_frame);
	ASSERT(ret == SKY_RET_OK, "sky_extend_with_crc32() returned %d", ret);
	ASSERT(frame.length == frame_length + 4 + 4, "frame length = %d", frame.length);
	ASSERT(tx_frame.hdr->flag_authenticated == 1);
	ASSERT(tx_frame.hdr->flag_crced == 1);

	// Start parsing the frame
	SkyParsedFrame parsed;
	ret = units_start_parsing(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "units_start_parsing() returned %d", ret);
	ASSERT(parsed.hdr.flag_authenticated == 1);
	ASSERT(parsed.hdr.flag_crced == 1);

	// Check CRC
	ret = sky_check_crc32(&frame, &parsed);
	ASSERT(ret == SKY_RET_OK, "sky_extend_with_crc32() returned %d", ret);

	// Check authentication.
	ret = sky_hmac_check_authentication(handle, &frame, &parsed);
	ASSERT(ret == 0, "sky_hmac_check_authentication() returned %d", ret);

	//ASSERT(parsed.payload_len == 46, "Returned %d", parsed.payload_len); // Payload length OK?, if HMAC passed, payload length should be original-4.

	// Destroy SkyHandle struct.
	sky_destroy(handle);
}

/*
 * Test compatibility of the Blake3 implementation agains precalculated hashes.
 */
TEST(blake3_impl)
{
	/*
	 * from blake3 import blake3
	 * key = bytes([ 132, 254, 193, 191, 95, 13, 62, 164, 8, 140, 197, 175, 13, 191, 104, 34, 8, 192, 248, 95, 7, 43, 188, 223, 122, 252, 117, 164, 115, 130, 192, 166 ])
	 * data = bytes([ 117, 27, 82, 74, 33, 174, 183, 85, 22, 60, 135, 19, 166, 94, 158, 169, 99, 74, 112, 64, 100, 28, 80, 70, 11, 21, 136, 229, 83, 142, 1, 8, 227, 17, 230, 41, 195, 202, 153, 107, 144, 214, 129, 95, 143, 235, 103, 154, 241, 45, 20, 222, 205, 67, 127, 183, 20, 69, 197, 248, 207, 220, 59, 144, 134, 146, 98, 86, 155, 246, 94, 160, 15, 37, 46, 49, 217, 14, 190, 143, 78, 40, 100, 42, 105, 226, 99, 49, 235, 128, 227, 23, 202, 174, 218, 210, 50, 1, 36, 85, 128, 146, 203, 30, 151, 204, 51, 219, 53, 106, 32, 173, 203, 177, 237, 202, 97, 96, 183, 209, 203, 45, 120, 34, 104, 101, 239, 68, 174, 0, 125, 200, 118, 182, 213, 20, 154, 52, 72, 16, 188, 58, 231, 55, 238, 149, 49, 125, 38, 239, 35, 226, 186, 13, 89, 67, 234, 134, 116, 140, 132, 145, 247, 39, 184, 209, 220, 148, 140, 108, 87, 105, 93, 145, 141, 37, 91, 32, 70, 246, 202, 89, 213, 58, 143, 20, 202, 33, 193, 144, 61, 57, 217, 99, 248, 91, 47, 189, 136, 41, 30, 30, 12, 66, 42, 46, 115, 119, 218, 161, 187, 203, 68, 189, 212, 14, 245, 171, 127, 78, 236, 177, 186, 167, 108, 83, 10, 188, 39, 205, 197, 134, 30, 168, 207, 121, 239, 239, 22, 116, 201, 115, 54, 67, 194, 7, 151, 113, 228, 9, 134, 40, 120, 193, 60, 160 ])
	 *
	 * digest = blake3(data, key=key).digest()
	 * print(", ".join([ str(c) for c in digest ]))
	 */

	const uint8_t key[32] = {132, 254, 193, 191, 95, 13, 62, 164, 8, 140, 197, 175, 13, 191, 104, 34, 8, 192, 248, 95, 7, 43, 188, 223, 122, 252, 117, 164, 115, 130, 192, 166};
	const uint8_t data[256] = {117, 27, 82, 74, 33, 174, 183, 85, 22, 60, 135, 19, 166, 94, 158, 169, 99, 74, 112, 64, 100, 28, 80, 70, 11, 21, 136, 229, 83, 142, 1, 8, 227, 17, 230, 41, 195, 202, 153, 107, 144, 214, 129, 95, 143, 235, 103, 154, 241, 45, 20, 222, 205, 67, 127, 183, 20, 69, 197, 248, 207, 220, 59, 144, 134, 146, 98, 86, 155, 246, 94, 160, 15, 37, 46, 49, 217, 14, 190, 143, 78, 40, 100, 42, 105, 226, 99, 49, 235, 128, 227, 23, 202, 174, 218, 210, 50, 1, 36, 85, 128, 146, 203, 30, 151, 204, 51, 219, 53, 106, 32, 173, 203, 177, 237, 202, 97, 96, 183, 209, 203, 45, 120, 34, 104, 101, 239, 68, 174, 0, 125, 200, 118, 182, 213, 20, 154, 52, 72, 16, 188, 58, 231, 55, 238, 149, 49, 125, 38, 239, 35, 226, 186, 13, 89, 67, 234, 134, 116, 140, 132, 145, 247, 39, 184, 209, 220, 148, 140, 108, 87, 105, 93, 145, 141, 37, 91, 32, 70, 246, 202, 89, 213, 58, 143, 20, 202, 33, 193, 144, 61, 57, 217, 99, 248, 91, 47, 189, 136, 41, 30, 30, 12, 66, 42, 46, 115, 119, 218, 161, 187, 203, 68, 189, 212, 14, 245, 171, 127, 78, 236, 177, 186, 167, 108, 83, 10, 188, 39, 205, 197, 134, 30, 168, 207, 121, 239, 239, 22, 116, 201, 115, 54, 67, 194, 7, 151, 113, 228, 9, 134, 40, 120, 193, 60, 160};
	const uint8_t correct_hash[32] = {57, 241, 94, 49, 187, 30, 248, 132, 205, 143, 169, 11, 209, 92, 239, 26, 97, 0, 206, 59, 187, 210, 157, 160, 182, 141, 163, 207, 23, 134, 213, 218};

	blake3_hasher ctx;
	uint8_t hash[32];

	blake3_hasher_init_keyed(&ctx, key);
	blake3_hasher_update(&ctx, data, 256);
	blake3_hasher_finalize(&ctx, hash, 32);

	ASSERT_MEMORY(hash, correct_hash, 32);
}
