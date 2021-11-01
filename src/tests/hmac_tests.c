//
// Created by elmore on 1.11.2021.
//

#include "hmac_tests.h"



static void test1(int rounds);
static void test1_round();


void hmac_tests(){
	test1(3000);
}





static void test1(int rounds){
	PRINTFF(0,"[HMAC TEST 1]\n");
	for (int i = 0; i < rounds; ++i) {
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

	int corrupt_key = randint_i32(0,1);
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


	SkyRadioFrame* frame = new_frame();
	fillrand(frame->raw, 255);
	int length = randint_i32(0, SKY_FRAME_MAX_LEN - (SKY_HMAC_LENGTH+1));
	frame->length = length;
	frame->vc = vc;

	int hmac_seq = sky_hmac_get_next_hmac_tx_sequence_and_advance(self1, vc);
	assert(hmac_seq == wrap_hmac_sequence(self1->hmac->sequence_tx[vc]-1));
	frame->hmac_sequence = hmac_seq;
	if(randint_i32(0, 100) == 0){
		frame->hmac_sequence = config1->hmac.magic_sequence;
	}


	sky_hmac_extend_with_authentication(self1, frame);
	assert(frame->length == length + SKY_HMAC_LENGTH);


	int corrupt_pl = randint_i32(0, 1);
	if(corrupt_pl){
		int i = randint_i32(0, length+SKY_HMAC_LENGTH-1);
		uint8_t old = frame->raw[i];
		while (frame->raw[i] == old){
			frame->raw[i] = (uint8_t) randint_i32(0,255);
		}
	}


	int r1 = sky_hmac_check_authentication(self2, frame);


	if(frame->hmac_sequence == config1->hmac.magic_sequence){
		assert(r1 == 0);
		assert(frame->length == length);
	}

	if (frame->hmac_sequence != config1->hmac.magic_sequence){
		if((shift_tx <= config1->hmac.maximum_jump) && (!corrupt_pl) && (!corrupt_key)){
			assert(r1 == 0);
			assert(frame->length == length);
		} else {
			assert(r1 < 0);
			assert(frame->length == length + SKY_HMAC_LENGTH);
		}
	}

	destroy_handle(self1);
	destroy_handle(self2);
	destroy_config(config1);
	destroy_config(config2);
	destroy_frame(frame);

}














