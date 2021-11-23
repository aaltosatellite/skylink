//
// Created by elmore on 28.10.2021.
//

#include "packet_encode_test.h"
#include "tst_utilities.h"
#include "skylink/fec.h"
#include "skylink/utilities.h"

static void test1();
static void test1_round();


void packet_tests(){
	test1();
}



static void test1(){
	PRINTFF(0,"[Packet Test 1: ENCODE-DECODE]\n");
	for (int i = 0; i < 9500; ++i) {
		test1_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}



static void test1_round(){
	// pull random params ----------------------------------------------------------------------------------------------
	SkyConfig* config = new_vanilla_config();
	SkyRadioFrame* sframe = new_frame();
	SkyRadioFrame* rframe = new_frame();
	fillrand((uint8_t*)sframe, sizeof(SkyRadioFrame));
	fillrand((uint8_t*)rframe, sizeof(SkyRadioFrame));
	uint8_t identity[SKY_IDENTITY_LEN];
	fillrand(identity, SKY_IDENTITY_LEN);
	int vc = randint_i32(0, SKY_NUM_VIRTUAL_CHANNELS-1);

	int arq_on = randint_i32(0,1);
	int arq_sequence = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
	int hmac_on = randint_i32(0,1);
	int hmac_sequence = randint_i32(0, HMAC_CYCLE_LENGTH-1);


	// set satic members ----------------------------------------------------------------------------------------------
	sframe->length = EXTENSION_START_IDX;
	sframe->start_byte = SKYLINK_START_BYTE;
	memcpy(sframe->identity, identity, SKY_IDENTITY_LEN);
	sframe->vc = vc;
	sframe->auth_sequence = hmac_sequence;
	sframe->ext_length = 0;
	sframe->flags = 0;
	if(hmac_on){
		sframe->flags |= SKY_FLAG_AUTHENTICATED;
	}
	if(arq_on){
		sframe->flags |= SKY_FLAG_ARQ_ON;
	}



	int n_extensions = 0;

	int extension_mac_tdd = 0;
	int mac_length = randint_i32(0, config->mac.maximum_window_length);
	int mac_left = randint_i32(0, config->mac.maximum_window_length); // TODO sframe->mac_window);
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_mac_tdd = 1;
		sky_packet_add_extension_mac_tdd_control(sframe, mac_length, mac_left);
	}

	int extension_arq_ctrl = 0;
	int ctrl_tx = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
	int ctrl_rx = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_arq_ctrl = 1;
		sky_packet_add_extension_arq_ctrl(sframe, ctrl_tx, ctrl_rx);
	}

	int extension_arq_setup = 0;
	uint32_t setup_identifier = randint_u32(0, 0xFFFFFFFF);
	int setup_flag = randint_i32(0, 4);
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_arq_setup = 1;
		sky_packet_add_extension_arq_handshake(sframe, setup_flag, setup_identifier);
	}


	int extension_arq_sequence = 0;
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_arq_sequence = 1;
		sky_packet_add_extension_arq_sequence(sframe, arq_sequence);
	}


	int extension_arq_rrequest = 0;
	int rr_sequence = randint_i32(0,190);
	uint8_t mask = randint_i32(0,0xFFFF);
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_arq_rrequest = 1;
		sky_packet_add_extension_arq_request(sframe, rr_sequence, mask);
	}



	int extension_hmac_reset = 0;
	uint16_t hmac_enforcement = randint_i32(0, HMAC_CYCLE_LENGTH-1);
	if(randint_i32(0,1) == 1){
		n_extensions++;
		extension_hmac_reset = 1;
		sky_packet_add_extension_hmac_sequence_reset(sframe, hmac_enforcement);
	}

	assert(n_extensions <= 7);
	if(n_extensions == 7){
		assert(available_payload_space(sframe) == 173);
	}
	if(n_extensions == 0){
		assert(available_payload_space(sframe) == (RS_MSGLEN - (SKY_HMAC_LENGTH + EXTENSION_START_IDX)) );
	}

	int payload_len = randint_i32(0,173); //175 seems to be the max with this setup...
	uint8_t* pl = x_alloc(payload_len);
	fillrand(pl, payload_len);
	int r = sky_packet_extend_with_payload(sframe, pl, payload_len);
	assert(r == 0);
	assert((int)sframe->length == (payload_len + EXTENSION_START_IDX + sframe->ext_length));



	memcpy(rframe->raw ,sframe->raw, sframe->length);
	rframe->length = sframe->length;

	assert(rframe->start_byte == SKYLINK_START_BYTE);
	assert(memcmp(rframe->identity, identity, SKY_IDENTITY_LEN) ==0);
	assert(rframe->vc == vc);
	assert(((rframe->flags & SKY_FLAG_AUTHENTICATED) > 0) == (hmac_on > 0));
	assert(((rframe->flags & SKY_FLAG_ARQ_ON) > 0) == (arq_on > 0));
	if(hmac_on){
		assert(rframe->auth_sequence == hmac_sequence);
	}
	assert(rframe->ext_length == sframe->ext_length);



	int ext_remaining = rframe->ext_length;
	int ext_cursor = EXTENSION_START_IDX;
	while (ext_remaining) {
		SkyPacketExtension* ext = (SkyPacketExtension*)(rframe->raw + ext_cursor);
		int r2 = ext->length; // TODO: interpret_extension(rframe->raw + ext_cursor, ext_remaining, &ext);
		assert(r2 > 0);
		ext_cursor += r2;
		ext_remaining -= r2;
		if (ext->type == EXTENSION_MAC_TDD_CONTROL) {
			assert(ext != NULL);
			assert(ext->length == sizeof(ExtTDDControl)+1);
			assert(ext->TDDControl.window == mac_length);
			assert(ext->TDDControl.remaining == mac_left);
			extension_mac_tdd--;
			assert(extension_mac_tdd == 0);
		}
		else if(ext->type == EXTENSION_ARQ_HANDSHAKE){
			assert(extension_arq_setup == 1);
			assert(ext->length == sizeof(ExtARQHandshake) + 1);
			assert(ext->ARQHandshake.peer_state == setup_flag);
			assert(ext->ARQHandshake.identifier == setup_identifier);
			extension_arq_setup--;
			assert(extension_arq_setup == 0);
		}
		else if(ext->type == EXTENSION_ARQ_CTRL){
			assert(extension_arq_ctrl == 1);
			assert(ext->length == sizeof(ExtARQCtrl )+1);
			assert(ext->ARQCtrl.tx_sequence == ctrl_tx);
			assert(ext->ARQCtrl.rx_sequence == ctrl_rx);
			extension_arq_ctrl--;
			assert(extension_arq_ctrl == 0);
		}
		else if(ext->type == EXTENSION_ARQ_SEQUENCE){
			assert(extension_arq_sequence == 1);
			assert(ext->length == sizeof(ExtARQSeq )+1);
			assert(ext->ARQSeq.sequence == arq_sequence);
			extension_arq_sequence--;
			assert(extension_arq_sequence == 0);
		}
		else if(ext->type == EXTENSION_ARQ_REQUEST){
			assert(extension_arq_rrequest == 1);
			assert(ext->length == sizeof(ExtARQReq)+1);
			assert(ext->ARQReq.sequence == rr_sequence);
			assert(ext->ARQReq.mask == sky_hton16(mask));
			extension_arq_rrequest--;
			assert(extension_arq_rrequest == 0);
		}
		else if(ext->type == EXTENSION_HMAC_SEQUENCE_RESET){
			assert(ext->length == sizeof(ExtHMACSequenceReset)+1);
			assert(extension_hmac_reset == 1);
			assert(ext->HMACSequenceReset.sequence == sky_hton16(hmac_enforcement));
			extension_hmac_reset--;
			assert(extension_hmac_reset == 0);
		}
	}
	assert(ext_remaining == 0);


	int rcvd_pl_len = (int)rframe->length - (rframe->ext_length + EXTENSION_START_IDX);
	assert(rcvd_pl_len == payload_len);
	assert(memcmp(rframe->raw + rframe->ext_length + EXTENSION_START_IDX, pl, rcvd_pl_len) == 0);


	destroy_config(config);
	destroy_frame(sframe);
	destroy_frame(rframe);
	free(pl);
}
