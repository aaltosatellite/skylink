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
	for (int i = 0; i < 4500; ++i) {
		test1_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}


void test1_round(){
	SkyConfig* config = new_vanilla_config();
	SendFrame* sframe = new_send_frame();
	RCVFrame* rframe = new_receive_frame();
	fillrand((uint8_t*)sframe, sizeof(SendFrame));
	fillrand((uint8_t*)rframe, sizeof(RCVFrame));
	RadioFrame2* radioFrame_s = &sframe->radioFrame;
	RadioFrame2* radioFrame_r = &rframe->radioFrame;

	uint8_t identity[SKY_IDENTITY_LEN];
	fillrand(identity, SKY_IDENTITY_LEN);
	int vc = randint_i32(0, SKY_NUM_VIRTUAL_CHANNELS-1);
	int mac_length = randint_i32(0, config->mac.maximum_window_size);
	int mac_left = randint_i32(0, radioFrame_s->mac_window);
	int arq_on = randint_i32(0,1);
	int arq_sequence = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
	int hmac_on = randint_i32(0,1);
	int hmac_sequence = randint_i32(0, HMAC_CYCLE_LENGTH-1);

	radioFrame_s->length = I_PK_EXTENSIONS;
	radioFrame_s->start_byte = SKYLINK_START_BYTE;
	memcpy(radioFrame_s->identity, identity, SKY_IDENTITY_LEN);
	radioFrame_s->vc = vc;
	radioFrame_s->auth_sequence = hmac_sequence;
	radioFrame_s->mac_window = mac_length;
	radioFrame_s->mac_remaining = mac_left;
	radioFrame_s->arq_sequence = arq_sequence;
	radioFrame_s->ext_length = 0;
	radioFrame_s->flags = 0;
	if(hmac_on){
		radioFrame_s->flags |= SKY_FLAG_AUTHENTICATED;
	}
	if(arq_on){
		radioFrame_s->flags |= SKY_FLAG_ARQ_ON;
	}

	int n_extensions = 0;
	int extension_cursors[10];
	extension_cursors[0] = sframe->radioFrame.ext_length;

	int extension_mac_params = 0;
	int new_window = randint_i32(config->mac.minimum_window_size, config->mac.maximum_window_size);
	int new_gap = randint_i32(config->mac.minimum_gap_size, config->mac.maximum_gap_size);
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_mac_params = 1;
		sky_packet_add_extension_mac_params(sframe, new_gap, new_window);
		extension_cursors[n_extensions] = sframe->radioFrame.ext_length;
	}


	int extension_arq_settings = 0;
	int new_sequence0 = randint_i32(0, 240);
	int toggle = randint_i32(0, 1);
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_arq_settings = 1;
		sky_packet_add_extension_arq_enforce(sframe, toggle, new_sequence0);
		extension_cursors[n_extensions] = sframe->radioFrame.ext_length;
	}


	int extension_arq_rrequest = 0;
	int rr_sequence = randint_i32(0,190);
	uint8_t mask1 = randint_i32(0,255);
	uint8_t mask2 = randint_i32(0,255);
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_arq_rrequest = 1;
		sky_packet_add_extension_arq_rr(sframe, rr_sequence, mask1, mask2);
		extension_cursors[n_extensions] = sframe->radioFrame.ext_length;
	}



	int extension_hmac_enforcement = 0;
	uint16_t hmac_enforcement = randint_i32(0,65000);
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_hmac_enforcement = 1;
		sky_packet_add_extension_hmac_enforce(sframe, hmac_enforcement);
		extension_cursors[n_extensions] = sframe->radioFrame.ext_length;
	}


	if(n_extensions == 4){
		assert(available_payload_space(&sframe->radioFrame) == 185);
	}
	if(n_extensions == 0){
		assert(available_payload_space(&sframe->radioFrame) == (RS_MSGLEN - (SKY_HMAC_LENGTH + I_PK_EXTENSIONS)) );
	}


	int payload_len = randint_i32(0,184); //184 seems to be the max with this setup...
	uint8_t* pl = x_alloc(payload_len);
	fillrand(pl, payload_len);
	int r = sky_packet_extend_with_payload(sframe, pl, payload_len);
	assert(r == 0);
	assert(sframe->radioFrame.length == (payload_len + I_PK_EXTENSIONS + sframe->radioFrame.ext_length));

	if(n_extensions == 4){
		assert(available_payload_space(&sframe->radioFrame) <= 185);
	}
	if(n_extensions == 0){
		assert(available_payload_space(&sframe->radioFrame) <= (RS_MSGLEN - (SKY_HMAC_LENGTH + I_PK_EXTENSIONS)) );
	}

	memcpy(rframe->radioFrame.raw ,sframe->radioFrame.raw, sframe->radioFrame.length);
	rframe->radioFrame.length = sframe->radioFrame.length;


	assert(radioFrame_r->vc == vc);
	assert(radioFrame_r->mac_window == mac_length);
	assert(radioFrame_r->mac_remaining == mac_left);
	assert(((radioFrame_r->flags & SKY_FLAG_AUTHENTICATED) > 0) == (hmac_on > 0));
	assert(((radioFrame_r->flags & SKY_FLAG_ARQ_ON) > 0) == (arq_on > 0));
	if(hmac_on){
		assert(radioFrame_r->auth_sequence == hmac_sequence);
	}
	if(arq_on){
		assert(radioFrame_r->arq_sequence == arq_sequence);
	}
	assert(radioFrame_r->ext_length == radioFrame_s->ext_length);

	int ext_remaining = radioFrame_r->ext_length;
	int ext_cursor = I_PK_EXTENSIONS;
	while (ext_remaining) {
		SkyPacketExtension ext;
		int r2 = interpret_extension(radioFrame_r->raw + ext_cursor, ext_remaining, &ext);
		assert(r2 > 0);
		ext_cursor += r2;
		ext_remaining -= r2;
		if(ext.type == EXTENSION_MAC_PARAMETERS){
			assert(extension_mac_params == 1);
			assert(ext.ext_union.MACSpec.gap_size == new_gap);
			assert(ext.ext_union.MACSpec.window_size == new_window);
			extension_mac_params--;
		}
		if(ext.type == EXTENSION_ARQ_SEQ_RESET){
			assert(extension_arq_settings == 1);
			assert(ext.ext_union.ArqSeqReset.toggle == toggle);
			assert(ext.ext_union.ArqSeqReset.enforced_sequence == new_sequence0);
			extension_arq_settings--;
		}
		if(ext.type == EXTENSION_ARQ_RESEND_REQ){
			assert(extension_arq_rrequest == 1);
			assert(ext.ext_union.ArqReq.sequence == rr_sequence);
			assert(ext.ext_union.ArqReq.mask1 == mask1);
			assert(ext.ext_union.ArqReq.mask2 == mask2);
			extension_arq_rrequest--;
		}
		if(ext.type == EXTENSION_HMAC_ENFORCEMENT){
			assert(extension_hmac_enforcement == 1);
			assert(ext.ext_union.HMACTxReset.correct_tx_sequence == hmac_enforcement);
			extension_hmac_enforcement--;
		}
	}
	assert(ext_remaining == 0);


	int rcvd_pl_len = rframe->radioFrame.length - (rframe->radioFrame.ext_length + I_PK_EXTENSIONS);
	assert(rcvd_pl_len == payload_len);
	assert(memcmp(rframe->radioFrame.raw + radioFrame_r->ext_length + I_PK_EXTENSIONS, pl, rcvd_pl_len) == 0);


	destroy_config(config);
	destroy_send_frame(sframe);
	destroy_receive_frame(rframe);
	free(pl);
}













