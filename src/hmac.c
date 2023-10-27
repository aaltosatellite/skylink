
/*======================================	Hash-based message authentication code (HMAC)	==========================================*/

#include <stdlib.h>
#include <string.h>


#include "skylink/hmac.h"
#include "skylink/diag.h"
#include "skylink/conf.h"
#include "skylink/frame.h"
#include "skylink/utilities.h"
#include "skylink/fec.h"

#include "sky_platform.h"

#include "ext/blake3/blake3.h"

const unsigned int SKY_HMAC_CTX_SIZE = sizeof(blake3_hasher);



// Allocate and initialize HMAC state instance
SkyHMAC* sky_hmac_create(SkyHMACConfig* config)
{
	// Allocate memory for HMAC struct and clear
	SkyHMAC* hmac = SKY_MALLOC(sizeof(SkyHMAC));
	SKY_ASSERT(hmac != NULL);
	memset(hmac, 0, sizeof(SkyHMAC));

	// Allocate context memory for HMAC hash function
	hmac->ctx = SKY_MALLOC(SKY_HMAC_CTX_SIZE);
	SKY_ASSERT(hmac->ctx != NULL);

	// Allocate memory for HMAC key and copy it.
	SKY_ASSERT(config->key_length == BLAKE3_KEY_LEN);
	hmac->key = SKY_MALLOC(config->key_length);
	SKY_ASSERT(hmac->key != NULL);
	memcpy(hmac->key, config->key, config->key_length);
	hmac->key_len = config->key_length;

	return hmac;
}

// Free HMAC context, key and the struct itself.
void sky_hmac_destroy(SkyHMAC* hmac)
{
	SKY_FREE(hmac->ctx);
	SKY_FREE(hmac->key);
	SKY_FREE(hmac);
}

// Get next sequence number from transmit counter and advance it by one. Sequence number naturally wraps around due to uint16 overflow.
int32_t sky_hmac_get_next_tx_sequence(SkyHandle self, unsigned int vc)
{
	if (vc > SKY_NUM_VIRTUAL_CHANNELS)
		return 0;
	int32_t seq = self->hmac->sequence_tx[vc];
	self->hmac->sequence_tx[vc] = seq + 1; // uint16 naturally overflows
	return seq;
}

/*
Load HMAC sequence numbers from given array.
Size of the array is 2 * SKY_NUM_VIRTUAL_CHANNELS
*/
void sky_hmac_load_sequences(SkyHandle self, const uint16_t* sequences)
{
	// Loop through all virtual channels and load the sequence numbers
	for (int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		self->hmac->sequence_tx[vc] = *sequences++;
		self->hmac->sequence_rx[vc] = *sequences++;
	}
}

/*
Dump HMAC sequence numbers to given array.
Size of the array is 2 * SKY_NUM_VIRTUAL_CHANNELS.
*/
void sky_hmac_dump_sequences(SkyHandle self, uint16_t* sequences)
{
	// Loop through all virtual channels and dump the sequence numbers
	for (int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		*sequences++ = self->hmac->sequence_tx[vc];
		*sequences++ = self->hmac->sequence_rx[vc];
	}
}


// Add authentication to a frame.
int sky_hmac_extend_with_authentication(SkyHandle self, SkyTransmitFrame* tx_frame)
{
	// Get the pointer for hmac struct from the handle.
	SkyHMAC* hmac = self->hmac;
	SkyRadioFrame *frame = tx_frame->frame;

	// Check that the frame has enough free space for the hmac.
	if(frame->length > (SKY_FRAME_MAX_LEN - SKY_HMAC_LENGTH))
		return SKY_RET_FRAME_TOO_LONG_FOR_HMAC;

	// Add authenticaton flag to static header
	tx_frame->hdr->flags |= SKY_FLAG_AUTHENTICATED;
	tx_frame->hdr->flag_authenticated = 1;

	// Calculate blake3 hash
	blake3_hasher* hasher = (blake3_hasher*)hmac->ctx;
	blake3_hasher_init_keyed(hasher, hmac->key);
	blake3_hasher_update(hasher, frame->raw, frame->length);

	// Copy truncated hash to the end of the frame.
	blake3_hasher_finalize(hasher, tx_frame->ptr, SKY_HMAC_LENGTH);

	// Update length of frame.
	tx_frame->ptr += SKY_HMAC_LENGTH;
	frame->length += SKY_HMAC_LENGTH;

	return SKY_RET_OK;
}

/* Process HMAC Sequence Reset extension header */
static void sky_rx_process_ext_hmac_sequence_reset(SkyHMAC *hmac, const SkyHeaderExtension* ext, int vc)
{
	// Make sure the extension has the correct length.
	if (ext->length != sizeof(ExtHMACSequenceReset) + 1)
		return;

	// Parse new sequence number and set it
	uint16_t new_sequence = sky_ntoh16(ext->HMACSequenceReset.sequence);
	hmac->sequence_tx[vc] = new_sequence;

	SKY_PRINTF(SKY_DIAG_INFO | SKY_DIAG_HMAC, "VC #%d sequence numbering reset to %u\n", vc, new_sequence);
}

/*
Check the frame authentication and sequence number if required for the virtual channel.
Also, corrects sequence number field endianess and removes the HMAC extension from the frame if provided.
HMAC trailer is removed from the end of the frame.
*/
int sky_hmac_check_authentication(SkyHandle self, const SkyRadioFrame *frame, SkyParsedFrame* parsed)
{
	SkyHMAC *hmac = self->hmac;
	const unsigned vc = parsed->hdr.vc;
	const SkyVCConfig *vc_conf = &self->conf->vc[vc];
	SkyStaticHeader *hdr = &parsed->hdr;

	// Swap the endianness of sequence number for later use.
	const uint16_t frame_sequence = sky_ntoh16(parsed->hdr.frame_sequence);
	parsed->hdr.frame_sequence = frame_sequence;

	// If the frame claims to be authenticated, make sure is not too short.
	if ((hdr->flags & SKY_FLAG_AUTHENTICATED) != 0 && parsed->payload_len < SKY_HMAC_LENGTH) {
		self->diag->rx_hmac_fail++;
		return SKY_RET_FRAME_TOO_SHORT_FOR_HMAC;
	}

	// Is authentication not required?
	if ((vc_conf->require_authentication & SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION) == 0)
	{
		// Remove the HMAC field if it exists.
		if ((hdr->flags & SKY_FLAG_AUTHENTICATED) != 0)
			parsed->payload_len -= SKY_HMAC_LENGTH;

		return SKY_RET_OK;
	}

	// Authentication is required but no authentication field provided?
	if ((hdr->flags & SKY_FLAG_AUTHENTICATED) == 0) {
		SKY_PRINTF(SKY_DIAG_INFO | SKY_DIAG_HMAC, "HMAC: Authentication missing!\n")
		self->diag->rx_hmac_fail++;
		hmac->vc_enforcement_need[vc] = 1;
		return SKY_RET_AUTH_MISSING;
	}

	// Calculate the hash for the frame
	uint8_t calculated_hash[SKY_HMAC_LENGTH];
	blake3_hasher* hasher = (blake3_hasher*)hmac->ctx;
	blake3_hasher_init_keyed(hasher, hmac->key);
	blake3_hasher_update(hasher, frame->raw, frame->length - SKY_HMAC_LENGTH);
	blake3_hasher_finalize(hasher, calculated_hash, SKY_HMAC_LENGTH);

	// Compare the calculated hash to received one
	const uint8_t *frame_hash = &frame->raw[frame->length - SKY_HMAC_LENGTH];
	if (memcmp(frame_hash, calculated_hash, SKY_HMAC_LENGTH) != 0) {
		SKY_PRINTF(SKY_DIAG_INFO | SKY_DIAG_HMAC, "HMAC: Invalid authentication code!\n")
		self->diag->rx_hmac_fail++;
		hmac->vc_enforcement_need[vc] = 1;
		return SKY_RET_AUTH_FAILED;
	}

	// Process possible HMAC reset extension before validating the sequence number.
	// Otherwise, the logic authentication can get locked if both peers use incorrect sequence number
	// and both peer's check the sequence number.
	if (parsed->hmac_reset != NULL)
		sky_rx_process_ext_hmac_sequence_reset(hmac, parsed->hmac_reset, vc);

	// Authentication hash check was successfull.
	SKY_PRINTF(SKY_DIAG_DEBUG | SKY_DIAG_HMAC, "HMAC: Received sequence %u\n", frame_sequence)

	// If sequence number check is required for authentication check it.
	if (vc_conf->require_authentication & SKY_CONFIG_FLAG_REQUIRE_SEQUENCE)
	{
		// Get distance between received sequence number and the expected next sequence number.
		uint16_t jump = frame_sequence - hmac->sequence_rx[vc];

		// Check if jump is too large
		if (jump > self->conf->hmac.maximum_jump) {
			SKY_PRINTF(SKY_DIAG_INFO | SKY_DIAG_HMAC, "HMAC: Larger than allowed sequence jump\n")
			self->diag->rx_hmac_fail++;
			hmac->vc_enforcement_need[vc] = 1;
			return SKY_RET_EXCESSIVE_HMAC_JUMP;
		}
	}

	// The HMAC sequence on our side jumps to the immediate next sequence number.
	hmac->sequence_rx[vc] = frame_sequence + 1; // uint16 naturally overflows

	// Remove the HMAC field from the end of the frame
	parsed->payload_len -= SKY_HMAC_LENGTH;

	return SKY_RET_OK;
}


#if 0

void sky_seed(uint32_t seed);
uint32_t sky_rand();


////////////////////////////////////////////////////////////////////////////////
// Pseudo random number generator
// Source: https://www.analog.com/en/design-notes/random-number-generation-using-lfsr.html
// NOTE: c stdlib rand() uses malloc and free...
////////////////////////////////////////////////////////////////////////////////

#define POLY_MASK_32 0xb4bcd35c
#define POLY_MASK_31 0x7a5bc2e3

static uint32_t lfsr32, lfsr31;

static uint32_t shift_lfsr(uint32_t lfsr, uint32_t mask) {
	if (lfsr & 1) {
		lfsr ^= mask;
	} else {
		lfsr >>= 1;
	}
	return lfsr;
}

void sky_seed(uint32_t seed) {
	lfsr32 = seed ^ (seed << 8);
	lfsr31 = seed ^ (seed << 16) ^ (seed >> 16);
}

uint32_t sky_rand() {
	lfsr32 = shift_lfsr(lfsr32, POLY_MASK_32);
	lfsr32 = shift_lfsr(lfsr32, POLY_MASK_32);
	lfsr31 = shift_lfsr(lfsr31, POLY_MASK_31);
	return (lfsr32 ^ lfsr31) & 0xffff;
}


// https://github.com/oconnor663/bessie/blob/main/design.md
// https://github.com/BLAKE3-team/BLAKE3/issues/138
// https://github.com/oconnor663/bessie
// https://github.com/oconnor663/blake3_aead
// https://github.com/k0001/baile

int sky_encrypt(SkyHandle self, SkyRadioFrame *frame, uint8_t *data, unsigned int data_len)
{
	SkyHMAC *hmac = self->hmac;

	uint32_t nonce = get_rand();

	uint32_t keys[2*32];
	uint8_t init[5] = {
		0xFF & (nonce << 24),
		0xFF & (nonce << 16),
		0xFF & (nonce << 8),
		0, // Chunk index
		1, // Final flag
	};

	blake3_hasher *hasher = (blake3_hasher *)hmac->ctx;
	blake3_hasher_init_keyed(hasher, hmac->key);
	blake3_hasher_update(hasher, init, sizeof(k));
	blake3_hasher_finalize(hasher, keys, sizeof(keys));

	// Authentication tag
	blake3_hasher_init_keyed(hasher, &keys[0]);
	blake3_hasher_update(hasher, data, data_len);
	blake3_hasher_finalize(hasher, keys, sizeof(keys));

	// Stream
	uint8_t stream[MAX_LEN];
	blake3_hasher_init_keyed(hasher, &keys[32]);
	blake3_hasher_update(hasher, data, data_len);
	blake3_hasher_finalize(hasher, stream, data_len);

	for (unsigned int i = 0; i < data_len; i++)
		frame->data[i] ^= stream[i];

	return 0;
}

int sky_decrypt(uint32_t nonce, uint8_t *data, unsigned int data_len)
{
	SkyHMAC *hmac = self->hmac;
	uint32_t keys[2 * 32];

	uint8_t init[5] = {
		data[0],
		data[1],
		data[2],
		0, // Chunk index
		1, // Final flag
	};

	blake3_hasher *hasher = (blake3_hasher *)hmac->ctx;
	blake3_hasher_init_keyed(hasher, hmac->key);
	blake3_hasher_update(hasher, init, sizeof(k));
	blake3_hasher_finalize(hasher, keys, sizeof(keys));

	// Authentication tag
	blake3_hasher_init_keyed(hasher, &keys[0]);
	blake3_hasher_update(hasher, data, data_len);
	blake3_hasher_finalize(hasher, keys, sizeof(keys));

	// Stream
	uint8_t stream[MAX_LEN];
	blake3_hasher_init_keyed(hasher, &keys[32]);
	blake3_hasher_update(hasher, data, data_len);
	blake3_hasher_finalize(hasher, stream, data_len);

	for (unsigned int i = 0; i < data_len; i++)
		frame->data[i] ^= stream[i];

		return SKY_RET_AUTH_FAILED;

	return 0;
}

#endif