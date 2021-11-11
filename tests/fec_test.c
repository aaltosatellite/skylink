//
// Created by elmore on 29.10.2021.
//

#include "fec_test.h"


static void test1();
static int test1_round();


void fec_test(){
	test1(100);
}



int count_differing_bits(uint8_t a, uint8_t b){
	int ndiff = 0;
	for (int i = 0; i < 8; ++i) {
		uint8_t mask = (1<<i);
		int same = (a&mask) == (b&mask);
		if(!same){
			ndiff++;
		}
	}
	return ndiff;
}



void test1(){
	PRINTFF(0,"[FEC TEST 1]\n");
	int r_sum = 0;
	for (int i = 0; i < 3000; ++i) {
		r_sum +=test1_round();
	}
	assert(r_sum < 10);
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}


static int test1_round(){
	SkyRadioFrame* frame = new_send_frame();
	SkyDiagnostics* diag = new_diagnostics();

	int length = randint_i32(16+8, RS_MSGLEN);
	int n_corrupt_bytes = randint_i32(0, 16); //16 seems like the highest with 100% success rate

	//make a reference payload.
	uint8_t* ref = x_alloc(RS_MSGLEN*3);
	fillrand(ref, RS_MSGLEN*3);
	memcpy(frame->raw, ref, length);
	frame->length = length;


	//fec encode it.
	sky_fec_encode(frame);


	//take a reference of the encoded product.
	uint8_t* encoded_ref = x_alloc(frame->length);
	memcpy(encoded_ref, frame->raw, frame->length);


	//Corrupt the shit.
	int n_corrupt_bits = 0;
	int* corrupt_indexes = get_n_unique_random_integers(n_corrupt_bytes, 0, frame->length-1);
	for (int i = 0; i < n_corrupt_bytes; ++i) {
		int idx = corrupt_indexes[i];
		uint8_t old = frame->raw[idx];
		while (frame->raw[idx] == old){
			frame->raw[idx] = (uint8_t) randint_i32(0, 255);
		}
		n_corrupt_bits = n_corrupt_bits + count_differing_bits(frame->raw[idx], old);
	}


	//Assert that we in fact did what we meant.
	if(n_corrupt_bytes != 0){
		assert(memcmp(encoded_ref, frame->raw, frame->length) != 0);
	}
	if(n_corrupt_bytes == 0){
		assert(memcmp(encoded_ref, frame->raw, frame->length) == 0);
	}


	//attempt fec deocde.
	int r = sky_fec_decode(frame, diag);
	if(r == 0){
		//PRINTFF(0, "SUCCESS: %d bits,  %d bytes.\n", n_corrupt_bits, n_corrupt_bytes);
	}
	if(r != 0){
		PRINTFF(0, "Failure to correct:  %d bits,  %d bytes. (not necassarily a fail in test)\n", n_corrupt_bits, n_corrupt_bytes);
	}


	//Assert that the fec response was understood correctly...
	if(n_corrupt_bytes == 0){
		assert(r == 0);
		assert(memcmp(ref, frame->raw, length) == 0);
	}
	if(r == 0){
		assert(memcmp(ref, frame->raw, length) == 0);
		assert(frame->length == length);
	}
	if(r != 0){
		assert(memcmp(ref, frame->raw, length) != 0);
	}


	destroy_send_frame(frame);
	destroy_diagnostics(diag);
	free(corrupt_indexes);
	free(encoded_ref);
	free(ref);
	return r;
}
