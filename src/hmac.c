
#include <stdlib.h>
#include <string.h>
#include "skylink/hmac.h"
#include "skylink/diag.h"
#include "skylink/platform.h"
#include "skylink/conf.h"
#include "skylink/frame.h"
#include "skylink/utilities.h"



int32_t wrap_hmac_sequence(int32_t sequence){
	return positive_modulo(sequence, HMAC_CYCLE_LENGTH);
}



SkyHMAC* new_hmac_instance(HMACConfig* config) {
	SkyHMAC* hmac = SKY_MALLOC(sizeof(SkyHMAC));
	if (hmac == NULL) {
		return NULL;
	}
	memset(hmac, 0, sizeof(SkyHMAC));

	hmac->ctx = SKY_MALLOC(SKY_HMAC_CTX_SIZE);
	if (hmac->ctx == NULL){
		return NULL;
	}

	hmac->key = SKY_MALLOC(config->key_length);
	if (hmac->key == NULL){
		return NULL;
	}
	memcpy(hmac->key, config->key, config->key_length);
	hmac->key_len = config->key_length;

	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		hmac->sequence_tx[i] = 0;
		hmac->sequence_rx[i] = 0;
	}
	return hmac;
}



void destroy_hmac(SkyHMAC* hmac){
	SKY_FREE(hmac->ctx);
	SKY_FREE(hmac->key);
	SKY_FREE(hmac);
}



int32_t sky_hmac_get_next_hmac_tx_sequence_and_advance(SkyHandle self, uint8_t vc){
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
	if(frame->length > (SKY_FRAME_MAX_LEN - SKY_HMAC_LENGTH)){
		return SKY_RET_FRAME_TOO_LONG_FOR_HMAC;
	}
	frame->flags |= SKY_FLAG_AUTHENTICATED;

	// Calculate SHA256 hash
	uint8_t full_hash[32];
	cf_hmac_init(hmac->ctx, &cf_sha256, hmac->key, hmac->key_len);
	cf_hmac_update(hmac->ctx, frame->raw, frame->length);
	cf_hmac_finish(hmac->ctx, full_hash);

	//Copy truncated hash to the end of the frame.
	memcpy(&frame->raw[frame->length], full_hash, SKY_HMAC_LENGTH);
	frame->length += SKY_HMAC_LENGTH;

	return SKY_RET_OK;
}



int sky_hmac_check_authentication(SkyHandle self, SkyRadioFrame* frame) {
	//If the frame is too short don't even try to calculate anything

	if ((frame->flags & SKY_FLAG_AUTHENTICATED) && (frame->length < (SKY_PLAIN_FRAME_MIN_LENGTH + SKY_HMAC_LENGTH))){
		return SKY_RET_FRAME_TOO_SHORT_FOR_HMAC;
	}

	SkyHMAC* hmac = self->hmac;
	const SkyVCConfig* vc_conf = &self->conf->vc[frame->vc];

	// Is authentication required.
	if ((vc_conf->require_authentication & SKY_VC_FLAG_REQUIRE_AUTHENTICATION) == 0) {

		// Remove the HMAC field if exists
		if ((frame->flags & SKY_FLAG_AUTHENTICATED) != 0)
			frame->length -= SKY_HMAC_LENGTH;

		frame->auth_sequence = sky_ntoh16(frame->auth_sequence);

		return SKY_RET_OK;
	}

	// No authentication field provided.
	if ((frame->flags & SKY_FLAG_AUTHENTICATED) == 0) {
		SKY_PRINTF(SKY_DIAG_HMAC, "HMAC: Authentication missing!\n")
		hmac->vc_enforcement_need[frame->vc] = 1;
		return SKY_RET_AUTH_MISSING;
	}

	// Calculate the hash for the frame
	uint8_t calculated_hash[32];
	cf_hmac_init(hmac->ctx, &cf_sha256, hmac->key, hmac->key_len);
	cf_hmac_update(hmac->ctx, frame->raw, frame->length - SKY_HMAC_LENGTH);
	cf_hmac_finish(hmac->ctx, calculated_hash);

	// Compare the calculated hash to received one
	uint8_t *frame_hash = &frame->raw[frame->length - SKY_HMAC_LENGTH];
	if (memcmp(frame_hash, calculated_hash, SKY_HMAC_LENGTH) != 0) {
		SKY_PRINTF(SKY_DIAG_HMAC, "HMAC: Invalid authentication code!\n")
		hmac->vc_enforcement_need[frame->vc] = 1;
		return SKY_RET_AUTH_FAILED;
	}

	// Correct sequence field endianess after hash calculation for later use
	frame->auth_sequence = sky_ntoh16(frame->auth_sequence);
	SKY_PRINTF(SKY_DIAG_DEBUG | SKY_DIAG_HMAC, "HMAC: Received sequence %d\n", frame->auth_sequence)

	if (vc_conf->require_authentication & SKY_VC_FLAG_REQUIRE_SEQUENCE) {
		// Check if the hmac sequence number is something we are expecting
		int32_t jump = wrap_hmac_sequence( (int32_t)(frame->auth_sequence - hmac->sequence_rx[frame->vc]));
		if (jump > self->conf->hmac.maximum_jump) {
			SKY_PRINTF(SKY_DIAG_HMAC, "HMAC: Larger than allowed sequence jump\n")
			hmac->vc_enforcement_need[frame->vc] = 1;
			return SKY_RET_EXCESSIVE_HMAC_JUMP;
		}
	}

	// The hmac sequence on our side jumps to the immediate next sequence number.
	hmac->sequence_rx[frame->vc] = wrap_hmac_sequence((int32_t)(frame->auth_sequence + 1));

	// Remove the HMAC field from the end
	frame->length -= SKY_HMAC_LENGTH;

	return SKY_RET_OK;
}
