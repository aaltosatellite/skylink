#include "units.h"

const uint8_t key_a[32] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
};

const uint8_t key_b[32] = {
	0xF0, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f
};

const SkyHMACKey keys_a[1] = { {.key = key_a, .len = sizeof(key_a)} };
const SkyHMACKey keys_b[1] = { {.key = key_b, .len = sizeof(key_b)} };
const SkyHMACKey keys_ab[2] = { {.key = key_a, .len = sizeof(key_a)}, {.key = key_b, .len = sizeof(key_b)} };
const SkyHMACKey keys_ba[2] = { {.key = key_b, .len = sizeof(key_b)}, {.key = key_a, .len = sizeof(key_a)} };


// Valid tick
TEST_PARAM(ticks, sky_tick_t, { 0, 100, 0x7FFFFFFF, 0xFFFFFFFF });

// Valid ARQ sequence numbers
TEST_PARAM(arq_sequence, sky_arq_sequence_t, { 0, 100, 0x7F, 0xFF });

// Valid ARQ masks numbers
TEST_PARAM(arq_mask, sky_arq_mask_t, { 0x01, 0x82, 0x8421 });

// Valid payload lengths
TEST_PARAM(payload_length, unsigned int, { 0, 1, 4, 65, 127, SKY_PAYLOAD_MAX_LEN });

// Valid radio frame lengths
TEST_PARAM(frame_length, unsigned int, { 12, 14, 66, 196, 223 - 4 });


const SkyConfig default_config = {
	.mac = {
		.gap_constant_ticks              = 600,
		.tail_constant_ticks             = 80,
		.minimum_window_length_ticks     = 250,
		.maximum_window_length_ticks     = 1000,
		.window_adjust_increment_ticks   = 250,
		.window_adjustment_threshold     = 2,
		.unauthenticated_mac_updates     = 0,
		.idle_frames_per_window          = 0,
		.idle_timeout_ticks              = 30000,
		.carrier_sense_ticks             = 200,
	},
	.hmac = {
		.maximum_jump = 24,
	},
	.vc = {
		{
			.require_authentication      = 0,
			.rcv_ring_len                = 22,
			.horizon_width               = 16,
			.send_ring_len               = 24,
			.usable_element_size         = 175,
			.tx_key                      = 0,
			.rx_key                      = 0,
		},
		{
			.require_authentication      = 0,
			.rcv_ring_len                = 22,
			.horizon_width               = 16,
			.send_ring_len               = 24,
			.usable_element_size         = 175,
			.tx_key                      = 0,
			.rx_key                      = 0,
		},
		{
			.require_authentication      = 0,
			.rcv_ring_len                = 22,
			.horizon_width               = 16,
			.send_ring_len               = 24,
			.usable_element_size         = 175,
			.tx_key                      = 0,
			.rx_key                      = 0,
		},
		{
			.require_authentication      = 0,
			.rcv_ring_len                = 22,
			.horizon_width               = 16,
			.send_ring_len               = 24,
			.usable_element_size         = 175,
			.tx_key                      = 0,
			.rx_key                      = 0,
		},
	},
	.arq = {
		.timeout_ticks                   = 26000,
		.idle_frame_threshold            = 6500,
		.idle_frames_per_window          = 1,
	},
	.identity = { 't', 'e', 's', 't', 'X', 'X', 'X' },
	.identity_len = 4,

};


u_int8_t *create_payload(int length)
{
    u_int8_t *pl = malloc(length);

    // Fill payload.
    for (int i = 0; i < length; i++)
    {
        pl[i] = i;
    }

    // Data needs to be const to push the packet.
    return pl;
}


void fill_random_payload(SkyTransmitFrame *tx_frame, unsigned int payload_len)
{
	for (unsigned int i = 0; i < payload_len; i++)
		*(tx_frame->ptr++) = rand() % 255;
	tx_frame->frame->length += payload_len;
}

void corrupt_frame(SkyRadioFrame *frame, unsigned int byte_errors)
{
	corrupt(frame->raw, frame->length, byte_errors);
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

void units_init_tx_frame(SkyRadioFrame *frame, SkyTransmitFrame *tx_frame)
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

void units_set_tx_frame_length(SkyTransmitFrame* tx_frame, unsigned int frame_length)
{
	SkyRadioFrame* frame = tx_frame->frame;
	frame->length = frame_length;
	tx_frame->ptr = &frame->raw[frame_length];
}

int units_start_parsing(SkyRadioFrame *frame, SkyParsedFrame *parsed)
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
	parsed->payload_len = frame->length - payload_start;

	return SKY_RET_OK;
}

int roll_chance(double const chance){
	int r = rand(); // NOLINT(cert-msc50-cpp)
	double rd = (double)r;
	double rM = (double)RAND_MAX;
	double rr = rd/rM;
	return rr < chance;
}
int get_cycle(SkyMAC* mac){
	return mac->my_window_length + mac->config->gap_constant_ticks + mac->peer_window_length + mac->config->tail_constant_ticks;
}

extern sky_tick_t _global_ticks_now;

sky_tick_t units_advance_ticks(sky_tick_t ticks)
{
	_global_ticks_now += ticks;
	return _global_ticks_now;
}
