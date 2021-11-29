//
// Created by elmore on 16.10.2021.
//

#include "ring_test.h"
#include "tst_utilities.h"
#include <assert.h>
#include <math.h>

static void test1(int count);
static void test2(int count);
static void test3_rcv(int count);
static void test4_send(int count);

void ring_tests(int load){
	test1(1000*load +1);
	test2(500*load +1);
	test3_rcv(load);
	test4_send(load);
}

static int positive_mod(int x, int m){
	return ((x % m) + m) % m;
}

static int wrap_seq(int x){
	return ((x % ARQ_SEQUENCE_MODULO) + ARQ_SEQUENCE_MODULO) % ARQ_SEQUENCE_MODULO;
}


//a quick generic test of base functionality with a known case.
//======================================================================================================================
//======================================================================================================================
static void test1_round();
static void test1(int count){
	PRINTFF(0,"[RING TEST 1: a known case]\n");
	for (int i = 0; i < count; ++i) {
		if(i % 2000 == 0){
			PRINTFF(0,"\ti=%d\n", i);
		}
		test1_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}

static void test1_round(){
	//create array.
	SkyArrayConfig config;
	int sq0 = 0;
	config.send_ring_len = 10;
	config.rcv_ring_len = 10;
	config.horizon_width = 4;
	config.element_size = 64;
	config.element_count = 1500;
	SkyArqRing* array = new_arq_ring(&config);
	uint8_t* tgt = x_alloc(60000);
	String* s0 = get_random_string(randint_i32(0,SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE));
	String* s1 = get_random_string(randint_i32(0,SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE));
	String* s2 = get_random_string(randint_i32(0,SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE));
	String* s3 = get_random_string(randint_i32(0,SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE));
	String* s4 = get_random_string(randint_i32(0,SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE));
	String* s5 = get_random_string(randint_i32(0,SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE));
	String* s6 = get_random_string(randint_i32(0,SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE));

	skyArray_wipe_to_arq_init_state(array, 10);
	ExtARQHandshake handshake;
	handshake.identifier = array->arq_session_identifier;
	handshake.peer_state = ARQ_STATE_ON;
	skyArray_handle_handshake(array, handshake.peer_state, handshake.identifier, 12);


	//check the msg counts are 0.
	assert(skyArray_count_packets_to_tx(array, 1) == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 0);

	//generate random string and push it to send.
	int r = skyArray_push_packet_to_send(array, s0->data, s0->length);
	assert(r == 0);
	assert(skyArray_count_packets_to_tx(array, 1) == 1);

	//attempt recall. Should fail.
	assert(skyArray_can_recall(array, 0) == 0);
	r = skyArray_schedule_resend(array, 0);
	assert(r == RING_RET_CANNOT_RECALL);

	//read for tx. assert data matches.
	int seq_read_for_tx = -1;
	r = skyArray_read_packet_for_tx(array, tgt, &seq_read_for_tx, 1);
	assert(r == s0->length);

	assert(seq_read_for_tx == 0);
	assert(memcmp(tgt, s0->data, s0->length) == 0);
	assert(skyArray_count_packets_to_tx(array, 1) == 0);

	//recall. This should succeed.
	fillrand(tgt, s0->length+10);
	assert(skyArray_can_recall(array, 0) == 1);
	assert(skyArray_can_recall(array, 0+1) == 0);
	r = skyArray_schedule_resend(array, 0);
	assert(r == 0);

	//get pl to send. This should fail, as recall=0
	r = skyArray_read_packet_for_tx(array, tgt, &seq_read_for_tx, 0);
	assert(r == RING_RET_EMPTY);

	//get pl to send. This should succeed, as recall=1
	r = skyArray_read_packet_for_tx(array, tgt, &seq_read_for_tx, 1);
	assert(r == s0->length);
	assert(seq_read_for_tx == 0);
	assert(memcmp(tgt, s0->data, s0->length) == 0);


	//push data as rx, as sequence = sq0.
	uint16_t window0 = rcvRing_get_horizon_bitmap(array->rcvRing);
	assert(window0 == 0b00);
	assert(skyArray_count_readable_rcv_packets(array) == 0);
	r = skyArray_push_rx_packet(array, s0->data, s0->length, sq0, 12);
	assert(r == 1);
	assert(skyArray_count_readable_rcv_packets(array) == 1);
	uint16_t window1 = rcvRing_get_horizon_bitmap(array->rcvRing);
	assert(window1 == 0b00);

	//push data as rx, as sequence = sq0+2.
	r = skyArray_push_rx_packet(array, s2->data, s2->length, sq0+2, 12);
	assert(r == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 1);
	uint16_t window2 = rcvRing_get_horizon_bitmap(array->rcvRing);
	assert(window2 == 0b01);

	//push data as rx, as sequence = sq0+4.
	r = skyArray_push_rx_packet(array, s4->data, s4->length, sq0+4, 12);
	assert(r == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 1);
	uint16_t window3 = rcvRing_get_horizon_bitmap(array->rcvRing);
	assert(window3 == 0b101);

	//push data as rx, as sequence = sq0+6. should fail.
	r = skyArray_push_rx_packet(array, s6->data, s6->length, sq0+6, 12);
	assert(r == RING_RET_INVALID_SEQUENCE);
	assert(skyArray_count_readable_rcv_packets(array) == 1);

	//push data as rx, as sequence = sq0+1.
	r = skyArray_push_rx_packet(array, s1->data, s1->length, sq0+1, 12);
	assert(r == 2);
	assert(skyArray_count_readable_rcv_packets(array) == 3);
	uint16_t window4 = rcvRing_get_horizon_bitmap(array->rcvRing);
	assert(window4 == 0b01);

	//receive packets 0, 1, and 2
	int seq = 100;
	r = skyArray_read_next_received(array, tgt, &seq);
	assert(r == s0->length);
	assert(seq == sq0);
	assert(skyArray_count_readable_rcv_packets(array) == 2);
	assert(memcmp(tgt, s0->data, s0->length) == 0);

	r = skyArray_read_next_received(array, tgt, &seq);
	assert(r == s1->length);
	assert(seq == sq0+1);
	assert(skyArray_count_readable_rcv_packets(array) == 1);
	assert(memcmp(tgt, s1->data, s1->length) == 0);

	r = skyArray_read_next_received(array, tgt, &seq);
	assert(r == s2->length);
	assert(seq == sq0+2);
	assert(memcmp(tgt, s2->data, s2->length) == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 0);

	r = skyArray_read_next_received(array, tgt, &seq);
	assert(r == RING_RET_EMPTY);
	assert(skyArray_count_readable_rcv_packets(array) == 0);

	uint16_t window5 = rcvRing_get_horizon_bitmap(array->rcvRing);
	assert(window5 == 0b01);


	//push data as rx, as sequence = sq0+3.
	r = skyArray_push_rx_packet(array, s3->data, s3->length, sq0+3, 11);
	assert(r == 2);
	assert(skyArray_count_readable_rcv_packets(array) == 2);
	uint16_t window6 = rcvRing_get_horizon_bitmap(array->rcvRing);
	assert(window6 == 0b00);

	//push data as rx, as sequence = sq0+6.
	r = skyArray_push_rx_packet(array, s6->data, s6->length, sq0+6, 11);
	assert(r == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 2);
	uint16_t window7 = rcvRing_get_horizon_bitmap(array->rcvRing);
	assert(window7 == 0b01); //next would be 5.

	//reset receive side sequence to sqn
	int sqn = 0;
	skyArray_wipe_to_arq_on_state(array, 77, 10);


	//push data as rx, as sequence = sqn+1.
	r = skyArray_push_rx_packet(array, s1->data, s1->length, sqn+1, 11);
	assert(r == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 0);
	uint16_t window8 = rcvRing_get_horizon_bitmap(array->rcvRing);
	assert(window8 == 0b01);

	//push data as rx, as sequence = sqn.
	r = skyArray_push_rx_packet(array, s0->data, s0->length, sqn, 11);
	assert(r == 2);
	assert(skyArray_count_readable_rcv_packets(array) == 2);
	uint16_t window9 = rcvRing_get_horizon_bitmap(array->rcvRing);
	assert(window9 == 0b00);

	//receive all. They should be packages 3 and 4 with old sequences, and 0 and 1 in new sequencing.
	r = skyArray_read_next_received(array, tgt, &seq);
	assert(r == s0->length);
	assert(seq == sqn);
	assert(memcmp(tgt, s0->data, s0->length) == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 1);

	r = skyArray_read_next_received(array, tgt, &seq);
	assert(r == s1->length);
	assert(seq == sqn+1);
	assert(memcmp(tgt, s1->data, s1->length) == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 0);


	destroy_arq_ring(array);
	destroy_string(s0);
	destroy_string(s1);
	destroy_string(s2);
	destroy_string(s3);
	destroy_string(s4);
	destroy_string(s5);
	destroy_string(s6);
	free(tgt);
}
//======================================================================================================================
//======================================================================================================================




//======================================================================================================================
//======================================================================================================================
static void test2_round();
static void test2(int count){
	PRINTFF(0,"[RING TEST 2: ring horizon fill]\n");
	for (int i = 0; i < count; ++i) {
		if(i % 1000 == 0){
			PRINTFF(0,"\ti=%d\n", i);
		}
		test2_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}

static void test2_round(){
	int elesize = randint_i32(44,85);
	int elecount = randint_i32(9000,11000);
	int rcv_ring_len = randint_i32(20,35);
	int send_ring_len = randint_i32(20,35);
	int horizon = 16;

	//int send_sq0 = randint_i32(0,100); //randomize starting sequence.

	//initialize
	SkyArrayConfig config;
	config.send_ring_len = send_ring_len;
	config.rcv_ring_len = rcv_ring_len;
	config.horizon_width = horizon;
	config.element_size = elesize;
	config.element_count = elecount;
	SkyArqRing* array_s = new_arq_ring(&config);
	SkyArqRing* array_r = new_arq_ring(&config);

	int final_tx_head1 = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
	int lag1 = randint_i32(0,16);
	int final_rx_head1 = wrap_seq(final_tx_head1 - lag1);
	uint16_t tgt_mask = (uint16_t) randint_i32(0,pow(2, i32_max( lag1-1, 0)  )-1);
	//PRINTFF(0,":: %d %d %d %d\n", final_tx_head1, final_rx_head1, lag1, tgt_mask);
	populate_horizon(array_s, array_r, final_tx_head1, final_rx_head1, tgt_mask,  100, NULL);
	assert(array_s->sendRing->head_sequence == final_tx_head1);
	assert(array_r->rcvRing->head_sequence == final_rx_head1);

	populate_horizon(array_r, array_s, final_tx_head1, final_rx_head1, randint_i32(0,pow(2,lag1-1)-1),  100, NULL);
	assert(array_s->sendRing->head_sequence == final_tx_head1);
	assert(array_r->rcvRing->head_sequence == final_rx_head1);

	assert(array_r->sendRing->head_sequence == final_tx_head1);
	assert(array_s->rcvRing->head_sequence == final_rx_head1);

	destroy_arq_ring(array_r);
	destroy_arq_ring(array_s);
}
//======================================================================================================================
//======================================================================================================================






//test receive side.
//======================================================================================================================
//======================================================================================================================
static void test3_round();

static void test3_rcv(int count){
	PRINTFF(0,"[RING TEST 3: receive side]\n");
	for (int i = 0; i < count; ++i) {
		test3_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}


static void test3_round(){
	//randomize operational parameters
	int elesize = randint_i32(64,85);
	int elecount = randint_i32(10000,11000);
	int rcv_ring_len = randint_i32(20,35);
	int send_ring_len = randint_i32(20,35);
	int horizon = randint_i32(0, 16);
	int NMSG = randint_i32(3000, 6000);
	if(randint_i32(0, 6) == 0){
		NMSG = ARQ_SEQUENCE_MODULO+1000;
	}
	int n_strings = NMSG+500;
	String** messages = x_alloc(n_strings * sizeof(String*));
	for (int i = 0; i < n_strings; ++i) {
		messages[i] = get_random_string(randint_i32(0, SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE));
	}
	//int send_sq0 = randint_i32(0,100); //randomize starting sequence.
	int rcv_sq0 = 0;
	PRINTFF(0,"\t[ring length: %d] [horizon length: %d] [N_msgs: %d]\n", rcv_ring_len, horizon, NMSG);


	//initialize
	SkyArrayConfig config;
	config.send_ring_len = send_ring_len;
	config.rcv_ring_len = rcv_ring_len;
	config.horizon_width = horizon;
	config.element_size = elesize;
	config.element_count = elecount;
	SkyArqRing* array_r = new_arq_ring(&config);
	uint8_t* tgt = x_alloc(10000);
	int next_rcv_idx = 0;						//order index of the next message to be received by FSW side
	int next_continuous_seq = rcv_sq0;			//sequence of the next message to be pushed into array
	int next_continuous_idx = 0;				//order index of the next message to be pushed in
	int* succesfully_given = x_calloc(sizeof(int)*(NMSG+500)*2); //boolean mapping on which messages have been given in.
	int X = horizon+4;
	while (next_rcv_idx < NMSG){

		//This goes through messages from already given sequences, to sequences that are too far ahead.
		for (int i = 0; i < X; ++i) {

			int head_seq = next_continuous_seq;
			if((next_continuous_idx - next_rcv_idx) > (rcv_ring_len - (horizon+1))){
				head_seq = wrap_seq(rcv_sq0 + next_rcv_idx + rcv_ring_len - (horizon+1));
			}
			if(array_r->rcvRing->head_sequence != head_seq){
				PRINTFF(0, "%d  %d\n",array_r->rcvRing->head_sequence, head_seq);
			}
			assert(array_r->rcvRing->head_sequence == head_seq);

			int idx = next_continuous_idx + i - 2;
			if(idx < 0){ //avoid negative inexes in the beginning
				continue;
			}
			int seq = wrap_seq(rcv_sq0 + idx); //This computes the next sequence from index and beginning sequence

			if(randint_i32(0,1) == 0){ //50% chance of packet being lost
				continue;
			}
			String* s = messages[idx];
			//int head = array_r->primaryRcvRing->head_sequence;
			int r = skyArray_push_rx_packet(array_r, s->data, s->length, seq, 10);

			if(wrap_seq(seq - next_continuous_seq) > horizon){ //outside window
				assert(r < 0);
				continue;
			}

			if((idx - next_rcv_idx) >= rcv_ring_len){ //array full
				assert(r < 0);
				continue;
			}
			if(succesfully_given[idx] == 1){ //already in
				assert(r == RING_RET_PACKET_ALREADY_IN);
				continue;
			}

			assert(wrap_seq(rcv_sq0+next_rcv_idx) == array_r->rcvRing->tail_sequence);

			assert(r >= 0);
			assert(succesfully_given[idx] == 0);
			succesfully_given[idx] = 1;

			int next_c_idx = x_in_i32_arr(0, succesfully_given, NMSG+500);
			next_continuous_idx = next_c_idx;
			next_continuous_seq = wrap_seq(rcv_sq0 + next_continuous_idx);
		}

		if(randint_i32(0,2) == 0){
			while (skyArray_count_readable_rcv_packets(array_r) > 0){
				int seq = -1;
				int r = skyArray_read_next_received(array_r, tgt, &seq);
				assert(seq == wrap_seq(rcv_sq0+next_rcv_idx));
				assert(r == messages[next_rcv_idx]->length);
				assert(memcmp(tgt, messages[next_rcv_idx]->data, r) == 0);
				next_rcv_idx += 1;
			}
			uint16_t mask = rcvRing_get_horizon_bitmap(array_r->rcvRing);
			for (int i = 0; i < 16; ++i) {
				int got = ((mask & (1<<i)) > 0);
				if (got){
					assert(succesfully_given[next_continuous_idx+i+1] == 1);
				} else{
					assert(succesfully_given[next_continuous_idx+i+1] == 0);
				}
				//if(mask) PRINTFF(0,"%d  %d\n",got, succesfully_given[next_continuous_idx+i]);
			}
			//if(mask) PRINTFF(0,"\n");
		}

	}

	for (int i = 0; i < n_strings; ++i) {
		destroy_string(messages[i]);
	}
	free(messages);
	destroy_arq_ring(array_r);
	free(tgt);
	free(succesfully_given);
}
//======================================================================================================================
//======================================================================================================================







//test send side
//======================================================================================================================
//======================================================================================================================
static void test4_round();

static void test4_send(int count){
	PRINTFF(0,"[RING TEST 4: send side]\n");
	for (int i = 0; i < count; ++i) {
		test4_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}

static void test4_round(){
	int elesize = randint_i32(64,85);
	int elecount = randint_i32(10000,11000);
	int rcv_ring_len = randint_i32(20,35);
	int send_ring_len = randint_i32(38,39);
	int horizon = randint_i32(0, 16);
	int n_recall = horizon;
	int NMSG = randint_i32(3000, 9000);
	if(randint_i32(0,6) == 0){
		NMSG = ARQ_SEQUENCE_MODULO+1000;
	}
	int n_strings = NMSG+500;
	String** messages = x_alloc(n_strings * sizeof(String*));
	for (int i = 0; i < n_strings; ++i) {
		messages[i] = get_random_string(randint_i32(0, SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE));
	}
	int s_seq0 = 0; //randint_i32(0,100);
	//int r_seq0 = randint_i32(0,100);

	SkyArrayConfig config;
	config.send_ring_len = send_ring_len;
	config.rcv_ring_len = rcv_ring_len;
	config.horizon_width = horizon;
	config.element_size = elesize;
	config.element_count = elecount;
	SkyArqRing* array = new_arq_ring(&config);
	uint8_t* tgt = x_alloc(10000);
	PRINTFF(0,"\t[ring length: %d] [recall depth: %d] [N_msgs: %d]\n", send_ring_len, n_recall, NMSG);
	int next_idx_to_tx = 0;
	int next_idx_to_push = 0;
	int tail_seq = s_seq0;
	int in_buffer = 0;
	while (next_idx_to_tx < NMSG) {
		int send_buffer = positive_mod(array->sendRing->tx_head - array->sendRing->tail, array->sendRing->length);


		if(send_buffer <= ARQ_MAXIMUM_HORIZON){
			assert(skyArray_count_packets_to_tx(array, 0) == (next_idx_to_push - next_idx_to_tx));
		} else{
			assert(skyArray_count_packets_to_tx(array, 0) == 0);
		}


		assert(tail_seq == array->sendRing->tail_sequence);
		assert(wrap_seq(s_seq0+next_idx_to_push) == array->sendRing->head_sequence);

		if ((randint_i32(0, 10000) % 7) == 0) { //PUSH
			if(next_idx_to_push == NMSG){
				continue;
			}
			String* s = messages[next_idx_to_push];
			int r = skyArray_push_packet_to_send(array, s->data, s->length);
			//full
			if(wrap_seq(wrap_seq(s_seq0+next_idx_to_push) - tail_seq) == (send_ring_len-1)){
				assert(r < 0);
			}
			//not full
			else {
				assert(r == wrap_seq(s_seq0+next_idx_to_push));
				in_buffer++;
				next_idx_to_push++;
			}
		}

		//PRINTFF(0,"2");
		if ((randint_i32(0, 10000) % 7) == 0) { //PULL
			int sq_tx = -1;
			int peeked_seq0 = -1;
			int peeked_leng0 = -1;
			int peeked_seq1 = -1;
			int peeked_leng1 = -1;
			skyArray_peek_next_tx_size_and_sequence(array, 0, &peeked_leng0, &peeked_seq0);
			skyArray_peek_next_tx_size_and_sequence(array, 1, &peeked_leng1, &peeked_seq1);
			int r = skyArray_read_packet_for_tx(array, tgt, &sq_tx, randint_i32(0,1));

			if(in_buffer == 0) {
				assert(peeked_leng0 == -1);
				assert(peeked_leng1 == -1);
				assert(peeked_seq0 == -1);
				assert(peeked_seq1 == -1);
				assert(peeked_seq1 == -1);
				assert(peeked_leng1 == -1);
				assert(r == RING_RET_EMPTY);
				assert(sq_tx == -1);
			} else {
				sendRing_clean_tail_up_to(array->sendRing, array->elementBuffer,wrap_seq(array->sendRing->tx_sequence - n_recall));
				assert(peeked_leng1 == r);
				assert(peeked_seq1 == sq_tx);
				assert(r >= 0);
				assert(sq_tx == wrap_seq(s_seq0+next_idx_to_tx));
				assert(r == messages[next_idx_to_tx]->length);
				assert(memcmp(tgt, messages[next_idx_to_tx]->data, messages[next_idx_to_tx]->length) == 0);
				next_idx_to_tx++;
				in_buffer--;
				if(wrap_seq(wrap_seq(s_seq0+next_idx_to_tx) - tail_seq) > n_recall){
					tail_seq = wrap_seq(tail_seq + 1);
					assert(tail_seq == array->sendRing->tail_sequence);
				}
			}
		}

		if ((randint_i32(0, 10000) % 7) == 0) { //RECALL
			for (int i = 0; i < 10; ++i) {
				int next_seq_to_tx = wrap_seq(s_seq0 + next_idx_to_tx);
				int tail_idx = next_idx_to_tx - wrap_seq(next_seq_to_tx - tail_seq);
				int n_successful_schedules = 0;
				int successful_scheduled_indexes[35];
				//int successful_scheduled_sequences[35];
				int n_schedule_trials = randint_i32(1, 35);
				for (int j = 0; j < n_schedule_trials; ++j) {
					int seq_shift = randint_i32(-2, n_recall+2);
					int seq = wrap_seq(tail_seq + seq_shift);
					int idx = tail_idx + seq_shift;
					if (idx < 0){
						continue;
					}
					int r1 = skyArray_can_recall(array, seq);
					int r2 = skyArray_schedule_resend(array, seq);
					if( wrap_seq(seq - tail_seq) < wrap_seq(next_seq_to_tx - tail_seq) ){
						assert(r1 == 1);
						if(n_successful_schedules < 16){
							assert(r2 == 0);
							assert(array->sendRing->resend_count <= 16);
							if(x_in_i32_arr(idx, successful_scheduled_indexes, n_successful_schedules) < 0){
								successful_scheduled_indexes[n_successful_schedules] = idx;
								//successful_scheduled_sequences[n_successful_schedules] = seq;
								n_successful_schedules++;
							}
						} else{
							assert(r2 == RING_RET_RESEND_FULL);
							assert(array->sendRing->resend_count == 16);
						}
					} else {
						assert(r1 == 0);
						if(n_successful_schedules < 16){
							assert(r2 == RING_RET_CANNOT_RECALL);
						}
						else{
							assert(r2 == RING_RET_RESEND_FULL);
						}
					}
				}
				for (int j = 0; j < n_successful_schedules; ++j) {

					int idx = successful_scheduled_indexes[j];
					//int seq = successful_scheduled_sequences[j];
					int srcall = 1000;
					int peeked_len = -1;
					int peeked_seq = -1;
					skyArray_peek_next_tx_size_and_sequence(array, 1, &peeked_len, &peeked_seq);
					int r3 = skyArray_read_packet_for_tx(array, tgt, &srcall, 1);

					//PRINTFF(0,"rread: %d  %d  %d\n", r3, messages[idx]->length, idx);
					assert(r3 == messages[idx]->length);
					assert(memcmp(tgt, messages[idx]->data, r3) == 0);
					assert(r3 == peeked_len);
				}
				assert(array->sendRing->resend_count == 0);
			}
		}
	}
	for (int i = 0; i < n_strings; ++i) {
		destroy_string(messages[i]);
	}
	free(messages);
	destroy_arq_ring(array);
	free(tgt);
}
//======================================================================================================================
//======================================================================================================================









