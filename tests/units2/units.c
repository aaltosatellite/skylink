#include "units.h"

const uint8_t dummy_key1[32] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f};
const uint8_t dummy_key2[32] = { 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b, 0x3c, 0x3d, 0x3e, 0x3f};
const uint8_t *key_list[] = {dummy_key1, dummy_key2};


void default_config(SkyConfig* config)
{
	config->vc[0].horizon_width                 = 16;
	config->vc[0].send_ring_len                 = 24;
	config->vc[0].rcv_ring_len 	                = 22;
	config->vc[0].usable_element_size           = 175;

	config->vc[1].horizon_width                 = 16;
	config->vc[1].send_ring_len                 = 24;
	config->vc[1].rcv_ring_len 	                = 22;
	config->vc[1].usable_element_size           = 175;

	config->vc[2].horizon_width                 = 6;
	config->vc[2].send_ring_len                 = 12;
	config->vc[2].rcv_ring_len 	                = 12;
	config->vc[2].usable_element_size           = 175;

	config->vc[3].horizon_width                 = 6;
	config->vc[3].send_ring_len                 = 12;
	config->vc[3].rcv_ring_len 	                = 12;
	config->vc[3].usable_element_size           = 175;

	config->vc[0].require_authentication        = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_REQUIRE_SEQUENCE;
	config->vc[1].require_authentication        = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_REQUIRE_SEQUENCE;
	config->vc[2].require_authentication        = 0;
	config->vc[3].require_authentication        = 0;

	config->arq.timeout_ticks                   = 26000;
	config->arq.idle_frame_threshold            = config->arq.timeout_ticks / 4;
	config->arq.idle_frames_per_window          = 1;


	config->hmac.key_length = sizeof(dummy_key1);
	memcpy(config->hmac.key, dummy_key1, sizeof(dummy_key1));
	config->hmac.maximum_jump                   = 32;

	config->mac.gap_constant_ticks              = 600;
	config->mac.tail_constant_ticks             = 80;
	config->mac.minimum_window_length_ticks     = 250;
	config->mac.maximum_window_length_ticks     = 1000;
	config->mac.window_adjust_increment_ticks   = 250;
	config->mac.window_adjustment_period        = 2;
	config->mac.unauthenticated_mac_updates     = 0;
	config->mac.shift_threshold_ticks           = 10000;
	config->mac.idle_frames_per_window          = 0;
	config->mac.idle_timeout_ticks              = 30000;
	config->mac.carrier_sense_ticks             = 200;

    // Set
    memcpy(config->identity, "testabc", 7);
	config->identity_len = 4;
}


void corrupt(uint8_t *data, unsigned int data_len, unsigned int byte_errors)
{
	assert(data_len >= byte_errors);
	unsigned int error_locations[byte_errors];
	for (unsigned int i = 0; i < byte_errors; i++)
	{
		unsigned int loc;
again:
		loc = randint_i32(0, data_len - 1);
		for (unsigned int j = 0; j < i; j++)
		{
			if (error_locations[j] == loc)
				goto again;
		}
		error_locations[i] = loc;
		data[loc] ^= randint_i32(1, 255);
	}
}

void init_tx(SkyRadioFrame *frame, SkyTransmitFrame *tx_frame)
{
	tx_frame->frame = frame;

	// Fill with random to catch possible uninitialized sections
	fillrand(frame, sizeof(SkyRadioFrame));

	// Start byte
	int identity_len = randint_i32(1, SKY_MAX_IDENTITY_LEN);
	frame->raw[0] = (SKYLINK_FRAME_VERSION_BYTE | identity_len);

	// Random indentity
	fillrand(&frame->raw[1], identity_len);
	frame->length = 1 + identity_len + sizeof(SkyStaticHeader);

	//
	tx_frame->hdr = (SkyStaticHeader *)&frame->raw[1 + identity_len];
	memset(tx_frame->hdr, 0, sizeof(SkyStaticHeader));
	tx_frame->ptr = &frame->raw[1 + identity_len + sizeof(SkyStaticHeader)];
}

int start_parsing(SkyRadioFrame *frame, SkyParsedFrame *parsed)
{
	// Some error checks
	if (frame->length < SKY_FRAME_MIN_LEN)
		return SKY_RET_INVALID_PLAIN_LENGTH;

	memset(parsed, 0, sizeof(SkyParsedFrame));

	// Validate protocol version
	const uint8_t version = frame->raw[0] & SKYLINK_FRAME_VERSION_BYTE;
	if (version != SKYLINK_FRAME_VERSION_BYTE)
		return SKY_RET_INVALID_VERSION;

	// Validate identity field
	parsed->identity = &frame->raw[1];
	parsed->identity_len = (frame->raw[0] & SKYLINK_FRAME_IDENTITY_MASK);
	if (parsed->identity_len == 0 || parsed->identity_len > SKY_MAX_IDENTITY_LEN)
		return SKY_RET_INVALID_VERSION;

	// Parse header
	const unsigned header_start = 1 + parsed->identity_len;
	memcpy(&parsed->hdr, &frame->raw[header_start], sizeof(SkyStaticHeader));
	const unsigned payload_start = header_start + sizeof(SkyStaticHeader) + parsed->hdr.extension_length;
	if (payload_start > frame->length)
		return SKY_RET_INVALID_EXT_LENGTH;

#if SKY_NUM_VIRTUAL_CHANNELS < 4
	const unsigned vc = parsed->hdr.vc;
	if (vc >= SKY_NUM_VIRTUAL_CHANNELS)
		return SKY_RET_INVALID_VC;
#endif

	// Extract the frame payload
	parsed->payload = &frame->raw[payload_start];
	if (parsed->hdr.flag_has_payload)
		parsed->payload_len = frame->length - payload_start;
	else
		parsed->payload_len = 0; // Ignore frame payload if payload flag is not set.

	return SKY_RET_OK;
}

int roll_chance(double const chance){
	int r = rand(); // NOLINT(cert-msc50-cpp)
	double rd = (double)r;
	double rM = (double)RAND_MAX;
	double rr = rd/rM;
	return rr < chance;
}