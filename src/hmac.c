#include "skylink/skylink.h"
#include "skylink/hmac.h"
#include "skylink/diag.h"
#include "skylink/fec.h"

#include <stdlib.h>
#include <string.h>

#include "skylink_platform.h"

int sky_hmac_init(SkyHandle_t self, const uint8_t* key, unsigned int key_len) {

	SkyHMAC_t* hmac = self->hmac;
	if (hmac == NULL) {
		self->hmac = hmac = SKY_MALLOC(sizeof(SkyHMAC_t));
		memset(hmac, 0, sizeof(SkyHMAC_t));
	}

	if (hmac == NULL)
		return SKY_RET_MALLOC_FAILED;

	if (hmac->ctx == NULL)
		hmac->ctx = SKY_MALLOC(SKY_HMAC_CTX_SIZE);

	if (hmac->ctx == NULL)
		return SKY_RET_MALLOC_FAILED;

	hmac->key = key;
	hmac->key_len = key_len;

	return SKY_RET_OK;
}


int sky_hmac_authenticate(SkyHandle_t self, SkyRadioFrame_t* frame) {

	if (self->conf->phy.authenticate_tx == 0)
		return SKY_RET_OK;

	SkyHMAC_t* hmac = self->hmac;
	if (hmac == NULL)
		return SKY_RET_AUTH_UNINITIALIZED;

	SKY_ASSERT(frame->length + SKY_HMAC_LENGTH <= SKY_FRAME_MAX_LEN);

	// Indicate in the phy header that the frame is authenticated
	frame->hdr.flags |= SKY_FRAME_AUTHENTICATED;

	/*
	 * Calculate SHA256 hash
	 */
	uint8_t full_hash[32];
	cf_hmac_init(hmac->ctx, &cf_sha256, hmac->key, hmac->key_len);
	cf_hmac_update(hmac->ctx, frame->raw, frame->length);
	cf_hmac_finish(hmac->ctx, full_hash);

	/*
	 * Copy truncated hash to the end of the frame.
	 */
	memcpy(&frame->raw[frame->length], full_hash, SKY_HMAC_LENGTH);
	frame->length += SKY_HMAC_LENGTH;

	return SKY_RET_OK;
}



int sky_hmac_check_authentication(SkyHandle_t self, SkyRadioFrame_t* frame) {

	if ((frame->hdr.flags & SKY_FRAME_AUTHENTICATED) == 0)
		return SKY_RET_OK;

	/*
	 * If the frame is too short don't even try to calculate anything
	 */
	if (frame->length < SKY_HMAC_LENGTH)
		return SKY_RET_INVALID_LENGTH;

	SkyHMAC_t* hmac = self->hmac;
	if (hmac == NULL)
		return SKY_RET_AUTH_UNINITIALIZED;

	/*
	 * Calculate the hash for the frame
	 */
	uint8_t calculated_hash[32];
	cf_hmac_init(hmac->ctx, &cf_sha256, hmac->key, hmac->key_len);
	cf_hmac_update(hmac->ctx, frame->raw, frame->length - SKY_HMAC_LENGTH);
	cf_hmac_finish(hmac->ctx, calculated_hash);

	uint8_t *frame_hash = &frame->raw[frame->length - SKY_HMAC_LENGTH];

	/*
	 * Compare the received hash to received one
	 */
	if (memcmp(frame_hash, calculated_hash, SKY_HMAC_LENGTH) != 0) {
		SKY_PRINTF(SKY_DIAG_FRAMES, "%10u: HMAC failed\n", get_timestamp());
		return SKY_RET_AUTH_FAILED;
	}

	frame->length -= SKY_HMAC_LENGTH;

	/*
	 * Check if the sequence number is something we are expecting
	 */
	if (0) {
		// TODO: Set a flag to indicate need to transmit sequence number on the next

		return SKY_RET_AUTH_FAILED;
	}

	return SKY_RET_OK;
}
