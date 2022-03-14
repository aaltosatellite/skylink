//
// Created by elmore on 1.11.2021.
//

#include "hmac_tests.h"



static void test1();
static void test1_round();


void hmac_tests(){
	test1();
}





static void test1(){
	PRINTFF(0,"[HMAC TEST 1]\n");
	for (int i = 0; i < 220000; ++i) {
		test1_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}





static void test1_round(){
	SkyConfig* config1 = new_vanilla_config();
	SkyConfig* config2 = new_vanilla_config();

	fillrand(config1->hmac.key, 16);
	memcpy(config2->hmac.key, config1->hmac.key, 16);

	SkyHandle self1 = new_handle(config1);
	SkyHandle self2 = new_handle(config2);

	int corrupt_key = (randint_i32(0,10) == 0);
	if(corrupt_key){
		fillrand(self2->hmac->key, self2->hmac->key_len);
	}

	int vc = randint_i32(0, SKY_NUM_VIRTUAL_CHANNELS-1);

	self2->hmac->sequence_rx[vc] = randint_i32(0, HMAC_CYCLE_LENGTH-1);
	self2->hmac->sequence_tx[vc] = randint_i32(0, HMAC_CYCLE_LENGTH-1);

	int shift_tx = randint_i32(0, config1->hmac.maximum_jump*2);
	int shift_rx = randint_i32(0, config1->hmac.maximum_jump*2);
	self1->hmac->sequence_rx[vc] = wrap_hmac_sequence(self2->hmac->sequence_tx[vc] + shift_rx);
	self1->hmac->sequence_tx[vc] = wrap_hmac_sequence(self2->hmac->sequence_rx[vc] + shift_tx);

	assert((SKY_VC_FLAG_AUTHENTICATE_TX & SKY_VC_FLAG_REQUIRE_SEQUENCE) == 0);
	assert((SKY_VC_FLAG_AUTHENTICATE_TX & SKY_VC_FLAG_REQUIRE_AUTHENTICATION) == 0);
	assert((SKY_VC_FLAG_REQUIRE_SEQUENCE & SKY_VC_FLAG_REQUIRE_AUTHENTICATION) == 0);

	self1->conf->vc[vc].require_authentication = 0;
	self1->conf->vc[vc].require_authentication |= (SKY_VC_FLAG_AUTHENTICATE_TX * randint_i32(0,1));
	int auth_tx = self1->conf->vc[vc].require_authentication & SKY_VC_FLAG_AUTHENTICATE_TX;

	self2->conf->vc[vc].require_authentication = 0;
	self2->conf->vc[vc].require_authentication |= (SKY_VC_FLAG_REQUIRE_AUTHENTICATION * randint_i32(0,1));
	self2->conf->vc[vc].require_authentication |= (SKY_VC_FLAG_REQUIRE_SEQUENCE * randint_i32(0,1));
	int req_auth = self2->conf->vc[vc].require_authentication & SKY_VC_FLAG_REQUIRE_AUTHENTICATION;
	int req_seq = self2->conf->vc[vc].require_authentication & SKY_VC_FLAG_REQUIRE_SEQUENCE;

	SkyRadioFrame* sframe = new_frame();
	SkyRadioFrame* rframe = new_frame();
	//RadioFrame* frame = &sframe->radioFrame;
	fillrand(sframe->raw, 255);
	int length = randint_i32(EXTENSION_START_IDX, SKY_FRAME_MAX_LEN - (SKY_HMAC_LENGTH + 1));
	sframe->length = length;
	sframe->vc = vc;


	int hmac_seq = sky_hmac_get_next_hmac_tx_sequence_and_advance(self1, vc);
	assert(hmac_seq == wrap_hmac_sequence(self1->hmac->sequence_tx[vc]-1));
	sframe->auth_sequence = hmac_seq;
	sframe->auth_sequence = sky_hton16(sframe->auth_sequence);
	sframe->flags = 0;
	if(auth_tx){
		sky_hmac_extend_with_authentication(self1, sframe);
		assert((int)sframe->length == (length + SKY_HMAC_LENGTH));
		assert(sframe->flags & SKY_FLAG_AUTHENTICATED);
		if(randint_i32(0,20) == 0){
			sframe->flags = sframe->flags & (~SKY_FLAG_AUTHENTICATED);
		}
	}

	int corrupt_pl = (randint_i32(0, 4) == 0);
	if(corrupt_pl){
		int i = randint_i32(0, sframe->length-1);
		while (i == 7){
			i = randint_i32(0, sframe->length-1);
		}
		uint8_t old = sframe->raw[i];
		while (sframe->raw[i] == old){
			sframe->raw[i] = (uint8_t) randint_i32(0,255);
		}
	}


	int flag_in_place = (sframe->flags & SKY_FLAG_AUTHENTICATED) > 0;

	memcpy(rframe, sframe, sizeof(SkyRadioFrame));
	int rlength0 = rframe->length;
	int too_short = (int)rframe->length < (SKY_PLAIN_FRAME_MIN_LENGTH + SKY_HMAC_LENGTH*flag_in_place);
	int r1 = sky_hmac_check_authentication(self2, rframe);


	if((!req_auth)){
		if(flag_in_place && too_short){
			assert(r1 < 0);
			assert((int)rframe->length == rlength0);
		}
		if(flag_in_place && (!too_short)){
			assert(r1 == 0);
			assert((int)rframe->length == (rlength0 - SKY_HMAC_LENGTH));
		}
		if(!flag_in_place){
			assert(r1 == 0);
			assert((int)rframe->length == rlength0);
		}
	}

	if(req_auth){
		if(flag_in_place && (!corrupt_pl) && (!corrupt_key) && auth_tx && (!too_short)){
			if(!req_seq){
				assert(r1 == 0);
				assert((int)rframe->length == (rlength0 - SKY_HMAC_LENGTH));
			}
			if(req_seq && (shift_tx <= config1->hmac.maximum_jump)){
				assert(r1 == 0);
				assert((int)rframe->length == (rlength0 - SKY_HMAC_LENGTH));
			}
			if(req_seq && (shift_tx > config1->hmac.maximum_jump)){
				assert(r1 < 0);
			}
		}

		if((!flag_in_place) || corrupt_key || corrupt_pl || (!auth_tx) || too_short){
			assert(r1 < 0);
		}
	}

	destroy_handle(self1);
	destroy_handle(self2);
	destroy_config(config1);
	destroy_config(config2);
	destroy_frame(sframe);
	destroy_frame(rframe);
}
