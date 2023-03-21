
#include <stdlib.h>
#include <string.h>

#define SKY_INCLUDE_EXTERNAL_DEPENDENCIES
#include "skylink/hmac.h"
#include "skylink/diag.h"
#include "skylink/conf.h"
#include "skylink/frame.h"
#include "skylink/utilities.h"
#include "skylink/fec.h"

#include "sky_platform.h"


const unsigned int SKY_HMAC_CTX_SIZE = sizeof(blake3_hasher);


int32_t wrap_hmac_sequence(int32_t sequence) {
	return positive_modulo(sequence, HMAC_CYCLE_LENGTH);
}



SkyHMAC* sky_hmac_create(HMACConfig* config)
{
	// Allocate memory for HMAC struct and clear 
	SkyHMAC* hmac = SKY_MALLOC(sizeof(SkyHMAC));
	SKY_ASSERT(hmac != NULL);
	memset(hmac, 0, sizeof(SkyHMAC));

	// Allocate context memory for HMAC hash function
	hmac->ctx = SKY_MALLOC(SKY_HMAC_CTX_SIZE);
	SKY_ASSERT(hmac->ctx != NULL);

	// Allocate memory for HMAC key and copy it.
	hmac->key = SKY_MALLOC(config->key_length);
	SKY_ASSERT(hmac->key != NULL);
	memcpy(hmac->key, config->key, config->key_length);
	hmac->key_len = config->key_length;

	return hmac;
}


void sky_hmac_destroy(SkyHMAC* hmac) {
	SKY_FREE(hmac->ctx);
	SKY_FREE(hmac->key);
	SKY_FREE(hmac);
}


int32_t sky_hmac_get_next_tx_sequence(SkyHandle self, unsigned int vc) {
	if (vc > SKY_NUM_VIRTUAL_CHANNELS)
		return 0;
	int32_t seq = self->hmac->sequence_tx[vc];
	self->hmac->sequence_tx[vc] = wrap_hmac_sequence(self->hmac->sequence_tx[vc] + 1);
	return seq;
}


int sky_hmac_load_sequences(SkyHandle self, const int32_t* sequences) {
	for (int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		self->hmac->sequence_tx[vc] = *sequences++;
		self->hmac->sequence_rx[vc] = *sequences++;
	}
	return 0;
}

int sky_hmac_dump_sequences(SkyHandle self, int32_t* sequences) {
	for (int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		*sequences++ = self->hmac->sequence_tx[vc];
		*sequences++ = self->hmac->sequence_rx[vc];
	}
	return 0;
}



int sky_hmac_extend_with_authentication(SkyHandle self, SkyRadioFrame* frame) {
	SkyHMAC* hmac = self->hmac;

	if(frame->length > (SKY_FRAME_MAX_LEN - SKY_HMAC_LENGTH))
		return SKY_RET_FRAME_TOO_LONG_FOR_HMAC;
	
	// Add authenticaton flag to 
	frame->flags |= SKY_FLAG_AUTHENTICATED;

	// Calculate blake3 hash
	blake3_hasher* hasher = (blake3_hasher*)hmac->ctx;
	blake3_hasher_init_keyed(hasher, hmac->key);
	blake3_hasher_update(hasher, frame->raw, frame->length);

	// Copy truncated hash to the end of the frame.
	blake3_hasher_finalize(hasher, &frame->raw[frame->length], SKY_HMAC_LENGTH);
	frame->length += SKY_HMAC_LENGTH;

	return SKY_RET_OK;
}

/* Process HMAC Sequence Reset extension header */
static void sky_rx_process_ext_hmac_sequence_reset(SkyHMAC *hmac, const SkyPacketExtension *ext, int vc)
{
	if (ext->length != sizeof(ExtHMACSequenceReset) + 1)
		return;

	// Parse new sequence number and set it 
	uint16_t new_sequence = sky_ntoh16(ext->HMACSequenceReset.sequence);
	hmac->sequence_tx[vc] = wrap_hmac_sequence(new_sequence);

	SKY_PRINTF(SKY_DIAG_INFO, "VC #%d sequence numbering reset to %d\n", vc, new_sequence);
}


int sky_hmac_check_authentication(SkyHandle self, SkyRadioFrame *frame, const SkyPacketExtension *ext_hmac_reset)
{
	SkyHMAC *hmac = self->hmac;
	const SkyVCConfig *vc_conf = &self->conf->vc[frame->vc];

	// If the frame claims to be authenticated, make sure is not too short.
	if ((frame->flags & SKY_FLAG_AUTHENTICATED) != 0 && frame->length < (SKY_PLAIN_FRAME_MIN_LENGTH + SKY_HMAC_LENGTH))
		return SKY_RET_FRAME_TOO_SHORT_FOR_HMAC;

	// Is not authentication required?
	if ((vc_conf->require_authentication & SKY_VC_FLAG_REQUIRE_AUTHENTICATION) == 0) {

		// Remove the HMAC field if it exists
		if ((frame->flags & SKY_FLAG_AUTHENTICATED) != 0)
			frame->length -= SKY_HMAC_LENGTH;

		// Swap the endianness of sequence number for later use
		frame->auth_sequence = sky_ntoh16(frame->auth_sequence);

		return SKY_RET_OK;
	}

	// Authentication is required but no authentication field provided?
	if ((frame->flags & SKY_FLAG_AUTHENTICATED) == 0) {
		SKY_PRINTF(SKY_DIAG_HMAC, "HMAC: Authentication missing!\n")
		hmac->vc_enforcement_need[frame->vc] = 1;
		return SKY_RET_AUTH_MISSING;
	}

	// Calculate the hash for the frame
	uint8_t calculated_hash[SKY_HMAC_LENGTH];
	blake3_hasher* hasher = (blake3_hasher*)hmac->ctx;
	blake3_hasher_init_keyed(hasher, hmac->key);
	blake3_hasher_update(hasher, frame->raw, frame->length - SKY_HMAC_LENGTH);
	blake3_hasher_finalize(hasher, calculated_hash, SKY_HMAC_LENGTH);

	// Compare the calculated hash to received one
	uint8_t *frame_hash = &frame->raw[frame->length - SKY_HMAC_LENGTH];
	if (memcmp(frame_hash, calculated_hash, SKY_HMAC_LENGTH) != 0) {
		SKY_PRINTF(SKY_DIAG_HMAC, "HMAC: Invalid authentication code!\n")
		hmac->vc_enforcement_need[frame->vc] = 1;
		return SKY_RET_AUTH_FAILED;
	}

	// Process possible HMAC reset extension before validating the sequence number.
	// Otherwise, the logic authentication can get locked if both peers use incorrect sequence number
	// and both peer's check the sequence number.
	if (ext_hmac_reset != NULL)
		sky_rx_process_ext_hmac_sequence_reset(hmac, ext_hmac_reset, frame->vc);

	// Authentication hash check was successfull.
	// Swap the endianness of sequence number for later use.
	frame->auth_sequence = sky_ntoh16(frame->auth_sequence);
	SKY_PRINTF(SKY_DIAG_DEBUG | SKY_DIAG_HMAC, "HMAC: Received sequence %d\n", frame->auth_sequence)

	// If sequence number check is required for authentication check it.
	if (vc_conf->require_authentication & SKY_VC_FLAG_REQUIRE_SEQUENCE) {
		int32_t jump = wrap_hmac_sequence( (int32_t)(frame->auth_sequence - hmac->sequence_rx[frame->vc]));
		if (jump > self->conf->hmac.maximum_jump) {
			SKY_PRINTF(SKY_DIAG_HMAC, "HMAC: Larger than allowed sequence jump\n")
			hmac->vc_enforcement_need[frame->vc] = 1;
			return SKY_RET_EXCESSIVE_HMAC_JUMP;
		}
	}

	// The HMAC sequence on our side jumps to the immediate next sequence number.
	hmac->sequence_rx[frame->vc] = wrap_hmac_sequence((int32_t)(frame->auth_sequence + 1));

	// Remove the HMAC field from the end of the frame
	frame->length -= SKY_HMAC_LENGTH;

	return SKY_RET_OK;
}
