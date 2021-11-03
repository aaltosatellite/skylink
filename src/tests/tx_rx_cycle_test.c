//
// Created by elmore on 2.11.2021.
//

#include "tx_rx_cycle_test.h"



static void test1();

static void test1_round();


void txrx_tests(){
	test1();
}



void test1(){
	PRINTFF(0,"[TX-RX Test 1: basic case]\n");
	for (int i = 0; i < 5; ++i) {
		test1_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}


uint16_t spin_to_seq(SkyArqRing* ring1, SkyArqRing* ring2, int target_sequence, int first_ahead){
	uint8_t* tgt = malloc(1000);
	int i = 0;
	int first_tgt_sequence = sequence_wrap(target_sequence - first_ahead);
	while (1){
		i++;
		String* s = get_random_string(randint_i32(0,100));
		skyArray_push_packet_to_send(ring1, s->data, s->length);
		int seq;
		skyArray_read_packet_for_tx(ring1, tgt, &seq, 1);
		skyArray_push_rx_packet(ring2, tgt, s->length, seq);
		skyArray_read_next_received(ring2, tgt, &seq);
		//PRINTFF(0,"%d %d\n",ring1->primarySendRing->tx_sequence, ring2->primaryRcvRing->head_sequence);
		assert(ring1->primarySendRing->tx_sequence == ring2->primaryRcvRing->head_sequence);
		destroy_string(s);
		if((i > 100) && (ring1->primarySendRing->tx_sequence == first_tgt_sequence)){
			break;
		}
	}
	uint16_t mask = 0;
	if(first_ahead > 0){
		for (int j = 0; j < first_ahead; ++j) {
			String* s = get_random_string(randint_i32(0,100));
			skyArray_push_packet_to_send(ring1, s->data, s->length);
			int seq;
			skyArray_read_packet_for_tx(ring1, tgt, &seq, 1);
			if(j > 0){
				int through = randint_i32(0, 3) == 0;
				if(through){
					skyArray_push_rx_packet(ring2, tgt, s->length, seq);
					if((j < 16) && (j <= ring2->primaryRcvRing->horizon_width) ){
						mask |= (1<<(j-1));
					}
				}
			}
			assert(ring1->primarySendRing->tx_sequence != ring2->primaryRcvRing->head_sequence);
			destroy_string(s);
		}
	}
	assert(ring1->primarySendRing->tx_sequence == target_sequence);
	assert(mask == skyArray_get_horizon_bitmap(ring2));
	free(tgt);
	return mask;
}



void test1_round(){
	int vc = randint_i32(0,SKY_NUM_VIRTUAL_CHANNELS-1);
	int seq_auth_1to2 = randint_i32(0, HMAC_CYCLE_LENGTH-1);
	int seq_auth_2to1 = randint_i32(0, HMAC_CYCLE_LENGTH-1);
	int hmac_on = randint_i32(0,2) > 0;
	int max_jump = randint_i32(1,40);
	int auth1_ahead = randint_i32(1, 6) == 0;
	int auth2_ahead = randint_i32(1, 6) == 0;
	int auth1_behind = randint_i32(1, 6) == 0;
	int auth2_behind = randint_i32(1, 6) == 0;
	int arq_on = randint_i32(0,2) > 0;
	int horizon1 = randint_i32(0, 16);
	int horizon2 = randint_i32(0, 16);
	int recall1 = randint_i32(0, 16);
	int recall2 = randint_i32(0, 16);
	int seq_arq_1to2 = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
	int seq_arq_2to1 = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
	SkyConfig* config1 = new_vanilla_config();
	SkyConfig* config2 = new_vanilla_config();
	config1->hmac.maximum_jump = max_jump;
	config2->hmac.maximum_jump = max_jump;
	config1->vc[vc].require_authentication = hmac_on;
	config2->vc[vc].require_authentication = hmac_on;
	config1->vc[vc].arq_on = arq_on;
	config2->vc[vc].arq_on = arq_on;
	config1->array[vc].initial_send_sequence = seq_arq_1to2;
	config1->array[vc].initial_rcv_sequence = seq_arq_2to1;
	config2->array[vc].initial_send_sequence = seq_arq_2to1;
	config2->array[vc].initial_rcv_sequence = seq_arq_1to2;
	config1->array[vc].n_recall = recall1;
	config2->array[vc].n_recall = recall2;
	config1->array[vc].horizon_width = horizon1;
	config2->array[vc].horizon_width = horizon2;
	config1->array[vc].rcv_ring_len  = 28;
	config1->array[vc].send_ring_len = 28;
	config2->array[vc].rcv_ring_len  = 28;
	config2->array[vc].send_ring_len = 28;
	SkyHandle handle1 = new_handle(config1);
	SkyHandle handle2 = new_handle(config2);
	handle1->hmac->sequence_tx[vc] = wrap_hmac_sequence( seq_auth_1to2 + (auth1_ahead ? (max_jump+4) : 0) + (auth1_behind ? -2 : 0) );
	handle1->hmac->sequence_rx[vc] = seq_auth_2to1;
	handle2->hmac->sequence_tx[vc] = wrap_hmac_sequence( seq_auth_2to1 + (auth2_ahead ? (max_jump+4) : 0) + (auth2_behind ? -2 : 0) );
	handle2->hmac->sequence_rx[vc] = seq_auth_1to2;
	SkyArqRing* ring1 = handle1->arrayBuffers[vc];
	SkyArqRing* ring2 = handle2->arrayBuffers[vc];



	uint16_t mask = spin_to_seq(ring1, ring2, seq_arq_1to2, 0);
	assert(mask == 0);

	SendFrame *sendFrame = new_send_frame();
	RCVFrame *rcvFrame = new_receive_frame();
	uint8_t *tgt = malloc(1000);

	for (int i = 0; i < ARQ_SEQUENCE_MODULO*2; ++i) {
		int len_pl = randint_i32(1, 100);
		String *payload = get_random_string(len_pl);
		skyArray_push_packet_to_send(handle1->arrayBuffers[vc], payload->data, payload->length);

		int content = sky_tx(handle1, sendFrame, vc, 1);
		memcpy(&rcvFrame->radioFrame, &sendFrame->radioFrame, sizeof(RadioFrame));
		//rcvFrame->radioFrame.length = sendFrame->radioFrame.length;
		sky_rx_0(handle2, rcvFrame, 1);

		int sequence = -1;
		int read = skyArray_read_next_received(handle2->arrayBuffers[vc], tgt, &sequence);

		int c0 = (content == 1);
		int c1 = (read == len_pl);
		int c2 = (sequence == sequence_wrap(seq_arq_1to2+i) );
		int c3 = (memcmp(tgt, payload->data, payload->length) == 0);
		destroy_string(payload);
		assert(c0);
		assert(c1);
		assert(c2);
		assert(c3);
	}

	free(tgt);

	destroy_receive_frame(rcvFrame);
	destroy_send_frame(sendFrame);
	destroy_handle(handle1);
	destroy_handle(handle2);
	destroy_config(config1);
	destroy_config(config2);

}