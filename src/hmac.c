
#include <stdlib.h>
#include <string.h>
#include "skylink/skylink.h"
#include "skylink/hmac.h"
#include "skylink/platform.h"



int32_t wrap_hmac_sequence(int32_t sequence){
	return ((sequence % HMAC_CYCLE_LENGTH) + HMAC_CYCLE_LENGTH) % HMAC_CYCLE_LENGTH;
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
	free(hmac->ctx);
	free(hmac->key);
	free(hmac);
}



int32_t sky_hmac_get_next_hmac_tx_sequence_and_advance(SkyHandle self, uint8_t vc){
	int32_t seq = self->hmac->sequence_tx[vc];
	self->hmac->sequence_tx[vc] = wrap_hmac_sequence(self->hmac->sequence_tx[vc] + 1);
	return seq;
}


int sky_hmac_extend_with_authentication(SkyHandle self, SkyRadioFrame* frame) {
	SkyHMAC* hmac = self->hmac;
	if(frame->length > (SKY_FRAME_MAX_LEN - SKY_HMAC_LENGTH)){
		return SKY_RET_INVALID_LENGTH;
	}


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
	if (frame->length < SKY_HMAC_LENGTH)
		return SKY_RET_INVALID_LENGTH;

	SkyHMAC* hmac = self->hmac;

	//For functional safety, there is a hmac number that shorts the auth process.
	if(frame->hmac_sequence == self->conf->hmac.magic_sequence){
		frame->metadata.auth_verified = 1;
		return SKY_RET_OK;
	}

	//Calculate the hash for the frame
	uint8_t calculated_hash[32];
	cf_hmac_init(hmac->ctx, &cf_sha256, hmac->key, hmac->key_len);
	cf_hmac_update(hmac->ctx, frame->raw, frame->length - SKY_HMAC_LENGTH);
	cf_hmac_finish(hmac->ctx, calculated_hash);


	//Compare the received hash to received one
	uint8_t *frame_hash = &frame->raw[frame->length - SKY_HMAC_LENGTH];
	if (memcmp(frame_hash, calculated_hash, SKY_HMAC_LENGTH) != 0) {
		return SKY_RET_AUTH_FAILED;
	}


	//Check if the hmac sequence number is something we are expecting
	int32_t jump = wrap_hmac_sequence( (int32_t)(frame->hmac_sequence - self->hmac->sequence_rx[frame->vc]));
	if (jump > self->conf->hmac.maximum_jump) {
		return SKY_RET_EXCESSIVE_HMAC_JUMP;
	}


	//The hmac sequence on our side jumps to the immediate next sequence number.
	self->hmac->sequence_rx[frame->vc] = wrap_hmac_sequence((int32_t)(frame->hmac_sequence + 1));
	frame->metadata.auth_verified = 1;
	frame->length -= SKY_HMAC_LENGTH;
	return SKY_RET_OK;
}


int sky_hmac_vc_demands_auth(SkyHandle self, uint8_t vc){
	return self->conf->vc[vc].require_authentication > 0;
}



