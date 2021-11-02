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
	for (int i = 0; i < 5500; ++i) {
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

	int corrupt_key = (randint_i32(0,2) == 0);
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


	SendFrame* sframe = new_send_frame();
	RCVFrame* rframe = new_receive_frame();
	RadioFrame2* frame = &sframe->radioFrame;
	fillrand(frame->raw, 255);
	int length = randint_i32(I_PK_EXTENSIONS, SKY_FRAME_MAX_LEN - (SKY_HMAC_LENGTH+1));
	frame->length = length;
	frame->vc = vc;


	int hmac_seq = sky_hmac_get_next_hmac_tx_sequence_and_advance(self1, vc);
	assert(hmac_seq == wrap_hmac_sequence(self1->hmac->sequence_tx[vc]-1));
	frame->auth_sequence = hmac_seq;
	if(randint_i32(0, 100) == 0){
		frame->auth_sequence = config1->hmac.magic_sequence;
	}


	sky_hmac_extend_with_authentication(self1, sframe);
	assert(frame->length == length + SKY_HMAC_LENGTH);


	int corrupt_pl = (randint_i32(0, 2) == 0);
	if(corrupt_pl){
		int i = randint_i32(0, length+SKY_HMAC_LENGTH-1);
		uint8_t old = frame->raw[i];
		while (frame->raw[i] == old){
			frame->raw[i] = (uint8_t) randint_i32(0,255);
		}
	}


	memcpy(&rframe->radioFrame, &sframe->radioFrame, sizeof(RadioFrame2));
	int r1 = sky_hmac_check_authentication(self2, rframe);
	RadioFrame2* frame2 = &rframe->radioFrame;

	if(frame2->auth_sequence == config1->hmac.magic_sequence){
		assert(r1 == 0);
		assert(frame2->length == length);
	}


	if (frame2->auth_sequence != config1->hmac.magic_sequence){
		if((shift_tx <= config1->hmac.maximum_jump) && (!corrupt_pl) && (!corrupt_key)){
			assert(r1 == 0);
			assert(frame2->length == length);
			assert(rframe->auth_verified == 1);
		} else {
			assert(r1 < 0);
			assert(frame2->length == length + SKY_HMAC_LENGTH);
			assert(rframe->auth_verified == 0);
		}
	}
	destroy_handle(self1);
	destroy_handle(self2);
	destroy_config(config1);
	destroy_config(config2);
	destroy_send_frame(sframe);
	destroy_receive_frame(rframe);
}









