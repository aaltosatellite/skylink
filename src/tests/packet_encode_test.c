//
// Created by elmore on 28.10.2021.
//

#include "packet_encode_test.h"
#include "tst_utilities.h"

static void test1();

static void test1_round();


void packet_tests(){
	test1();
}



void test1(){
	PRINTFF(0,"[Packet Test 1: ENCODE-DECODE]\n");
	for (int i = 0; i < 4000; ++i) {
		test1_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}


void test1_round(){
	SkyConfig* config = new_vanilla_config();
	SkyRadioFrame* frame = new_frame();
	SkyRadioFrame* frame2 = new_frame();


	uint8_t identity[SKY_IDENTITY_LEN];
	fillrand(identity, SKY_IDENTITY_LEN);
	int vc = randint_i32(0, SKY_NUM_VIRTUAL_CHANNELS-1);
	int mac_length = randint_i32(0, config->mac.maximum_window_size);
	int mac_left = randint_i32(0, frame->mac_length);
	int arq_on = randint_i32(0,1);
	int arq_sequence = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
	int hmac_on = randint_i32(0,1);
	int hmac_sequence = randint_i32(0, config->hmac.cycle_length);


	memcpy(frame->identity, identity, SKY_IDENTITY_LEN);
	frame->vc = vc;
	frame->hmac_on = hmac_on;
	frame->hmac_sequence = hmac_sequence;
	frame->mac_length = mac_length;
	frame->mac_remaining = mac_left;
	frame->arq_on = arq_on;
	frame->arq_sequence = arq_sequence;
	frame->n_extensions = 0;


	int n_extensions = 0;

	int extension_mac_params = 0;
	int new_window = randint_i32(config->mac.minimum_window_size, config->mac.maximum_window_size);
	int new_gap = randint_i32(config->mac.minimum_gap_size, config->mac.maximum_gap_size);
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_mac_params = 1;
		sky_packet_add_extension_mac_params(frame, new_window, new_gap);
	}

	int extension_arq_settings = 0;
	int new_sequence0 = randint_i32(0, 240);
	int toggle = randint_i32(0, 1);
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_arq_settings = 1;
		sky_packet_add_extension_arq_setup(frame, new_sequence0, toggle);
	}


	int extension_arq_rrequest = 0;
	int rr_sequence = randint_i32(0,190);
	uint8_t mask1 = randint_i32(0,255);
	uint8_t mask2 = randint_i32(0,255);
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_arq_rrequest = 1;
		sky_packet_add_extension_arq_resend_request(frame, rr_sequence, mask1, mask2);
	}


	int payload_len = randint_i32(0,187); //187 seems to be the max with this setup...
	uint8_t* pl = x_alloc(payload_len);
	fillrand(pl, payload_len);


	encode_skylink_packet(frame);
	sky_packet_extend_with_payload(frame, pl, payload_len);

	memcpy(frame2->raw ,frame->raw, frame->length);
	frame2->length = frame->length;
	decode_skylink_packet(frame2);


	assert(frame2->vc == vc);
	assert(frame2->mac_length == mac_length);
	assert(frame2->mac_remaining == mac_left);
	assert(frame2->hmac_on == hmac_on);
	if(hmac_on){
		assert(frame2->hmac_sequence == hmac_sequence);
	}
	assert(frame2->arq_on == arq_on);
	if(arq_on){
		assert(frame2->arq_sequence == arq_sequence);
	}

	assert(frame2->n_extensions == n_extensions);
	for (int i = 0; i < n_extensions; ++i) {
		SkyPacketExtension ext = frame2->extensions[0];
		if(ext.type == EXTENSION_MAC_PARAMETERS){
			assert(extension_mac_params == 1);
			assert(ext.ext_union.MACSpec.gap_size == new_gap);
			assert(ext.ext_union.MACSpec.window_size == new_window);
		}
		if(ext.type == EXTENSION_ARQ_SEQ_RESET){
			assert(extension_arq_settings == 1);
			assert(ext.ext_union.ArqSeqReset.toggle == toggle);
			assert(ext.ext_union.ArqSeqReset.enforced_sequence == new_sequence0);
		}

		if(ext.type == EXTENSION_ARQ_RESEND_REQ){
			assert(extension_arq_rrequest == 1);
			assert(ext.ext_union.ArqReq.sequence == rr_sequence);
			assert(ext.ext_union.ArqReq.mask1 == mask1);
			assert(ext.ext_union.ArqReq.mask2 == mask2);
		}
	}

	assert(frame2->metadata.payload_read_length == payload_len);
	assert(memcmp(frame2->metadata.payload_read_start, pl, frame2->metadata.payload_read_length) == 0);


	destroy_config(config);
	destroy_frame(frame);
	destroy_frame(frame2);
	free(pl);
}













