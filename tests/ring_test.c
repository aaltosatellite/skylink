//
// Created by elmore on 16.10.2021.
//

#include "ring_test.h"
#include "../src/skylink/skylink.h"
#include <assert.h>

static void test1(int count);
static void test1_round();
static void test2_rcv(int count);
static void test3_send(int count);

void ring_tests(){
	test1(100);
	test2_rcv(18);
	test3_send(18);
}


static int wrap_seq(int x){
	return ((x % ARQ_SEQUENCE_MODULO) + ARQ_SEQUENCE_MODULO) % ARQ_SEQUENCE_MODULO;
}


//a quick generic test of base functionality with a known case.
//======================================================================================================================
//======================================================================================================================
static void test1(int count){
	PRINTFF(0,"[ARQ ARRAY TEST 1: a known case]\n");
	for (int i = 0; i < count; ++i) {
		test1_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}

static void test1_round(){
	//create array.
	SkyArrayConfig config;
	int sq0 = randint_i32(0,15);
	int sq0_send = randint_i32(0,15);
	config.send_ring_len = 10;
	config.rcv_ring_len = 10;
	config.initial_rcv_sequence = sq0;
	config.initial_send_sequence = sq0_send;
	config.n_recall = 4;
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

	//check the msg counts are 0.
	assert(skyArray_count_packets_to_tx(array, 1) == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 0);

	//generate random string and push it to send.
	int r = skyArray_push_packet_to_send(array, s0->data, s0->length);
	assert(r == 0);
	assert(skyArray_count_packets_to_tx(array, 1) == 1);

	//attempt recall. Should fail.
	assert(skyArray_can_recall(array, sq0_send) == 0);
	r = skyArray_schedule_resend(array, sq0_send);
	assert(r == RING_RET_CANNOT_RECALL);

	//read for tx. assert data matches.
	int seq_read_for_tx = -1;
	r = skyArray_read_packet_for_tx(array, tgt, &seq_read_for_tx, 1);
	assert(r == s0->length);
	assert(seq_read_for_tx == sq0_send);
	assert(memcmp(tgt, s0->data, s0->length) == 0);
	assert(skyArray_count_packets_to_tx(array, 1) == 0);

	//recall. This should succeed.
	fillrand(tgt, s0->length+10);
	assert(skyArray_can_recall(array, sq0_send) == 1);
	assert(skyArray_can_recall(array, sq0_send+1) == 0);
	r = skyArray_schedule_resend(array, sq0_send);
	assert(r == 0);
	skyArray_read_packet_for_tx(array, tgt, &seq_read_for_tx, 1);
	assert(seq_read_for_tx == sq0_send);
	assert(memcmp(tgt, s0->data, s0->length) == 0);


	//push data as rx, as sequence = sq0.
	uint16_t window0 = skyArray_get_horizon_bitmap(array);
	assert(window0 == 0b00);
	assert(skyArray_count_readable_rcv_packets(array) == 0);
	r = skyArray_push_rx_packet(array, s0->data, s0->length, sq0);
	assert(r == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 1);
	uint16_t window1 = skyArray_get_horizon_bitmap(array);
	assert(window1 == 0b00);

	//push data as rx, as sequence = sq0+2.
	r = skyArray_push_rx_packet(array, s2->data, s2->length, sq0+2);
	assert(r == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 1);
	uint16_t window2 = skyArray_get_horizon_bitmap(array);
	assert(window2 == 0b01);

	//push data as rx, as sequence = sq0+4.
	r = skyArray_push_rx_packet(array, s4->data, s4->length, sq0+4);
	assert(r == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 1);
	uint16_t window3 = skyArray_get_horizon_bitmap(array);
	assert(window3 == 0b101);

	//push data as rx, as sequence = sq0+6. should fail.
	r = skyArray_push_rx_packet(array, s6->data, s6->length, sq0+6);
	assert(r == RING_RET_INVALID_SEQUENCE);
	assert(skyArray_count_readable_rcv_packets(array) == 1);

	//push data as rx, as sequence = sq0+1.
	r = skyArray_push_rx_packet(array, s1->data, s1->length, sq0+1);
	assert(r == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 3);
	uint16_t window4 = skyArray_get_horizon_bitmap(array);
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

	uint16_t window5 = skyArray_get_horizon_bitmap(array);
	assert(window5 == 0b01);


	//push data as rx, as sequence = sq0+3.
	r = skyArray_push_rx_packet(array, s3->data, s3->length, sq0+3);
	assert(r == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 2);
	uint16_t window6 = skyArray_get_horizon_bitmap(array);
	assert(window6 == 0b00);

	//push data as rx, as sequence = sq0+6.
	r = skyArray_push_rx_packet(array, s6->data, s6->length, sq0+6);
	assert(r == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 2);
	uint16_t window7 = skyArray_get_horizon_bitmap(array);
	assert(window7 == 0b01); //next would be 5.

	//reset receive side sequence to sqn
	int sqn = (sq0+50) % 250;
	skyArray_set_receive_sequence(array, sqn, 0);

	//push data as rx, as sequence = sqn+1.
	r = skyArray_push_rx_packet(array, s1->data, s1->length, sqn+1);
	assert(r == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 2);
	uint16_t window8 = skyArray_get_horizon_bitmap(array);
	assert(window8 == 0b01);

	//push data as rx, as sequence = sqn.
	r = skyArray_push_rx_packet(array, s0->data, s0->length, sqn);
	assert(r == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 4);
	uint16_t window9 = skyArray_get_horizon_bitmap(array);
	assert(window9 == 0b00);

	//receive all. They should be packages 3 and 4 with old sequences, and 0 and 1 in new sequencing.
	r = skyArray_read_next_received(array, tgt, &seq);
	assert(r == s3->length);
	assert(seq == sq0+3);
	assert(memcmp(tgt, s3->data, s3->length) == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 3);

	r = skyArray_read_next_received(array, tgt, &seq);
	assert(r == s4->length);
	assert(seq == sq0+4);
	assert(memcmp(tgt, s4->data, s4->length) == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 2);

	r = skyArray_read_next_received(array, tgt, &seq);
	assert(r == s0->length);
	assert(seq == sqn);
	assert(memcmp(tgt, s0->data, s0->length) == 0);
	assert(skyArray_count_readable_rcv_packets(array) == 1);

	//clean the secondary ring. It should contain the sole package 6 of old sequenceing.
	assert(array->secondaryRcvRing->storage_count == 1);
	int s6_ate = element_buffer_element_requirement_for(array->elementBuffer, s6->length);
	int old_ebuffer_free = array->elementBuffer->free_elements;
	skyArray_clean_unreachable(array);
	assert(array->secondaryRcvRing->storage_count == 0);
	assert(array->elementBuffer->free_elements == (old_ebuffer_free + s6_ate));

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




//test receive side.
//======================================================================================================================
//======================================================================================================================
static void test2_round();

static void test2_rcv(int count){
	PRINTFF(0,"[ARQ ARRAY TEST 2: receive side]\n");
	for (int i = 0; i < count; ++i) {
		test2_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}


static void test2_round(){
	//randomize operational parameters
	int elesize = randint_i32(64,85);
	int elecount = randint_i32(10000,11000);
	int rcv_ring_len = randint_i32(20,35);
	int send_ring_len = randint_i32(20,35);
	int horizon = randint_i32(0, 16);
	int n_recall = horizon;
	int NMSG = randint_i32(3000, 6000);
	int n_strings = NMSG+500;
	String** messages = x_alloc(n_strings * sizeof(String*));
	for (int i = 0; i < n_strings; ++i) {
		messages[i] = get_random_string(randint_i32(0, SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE));
	}
	int send_sq0 = randint_i32(0,100); //randomize starting sequence.
	int rcv_sq0 = randint_i32(0,100);
	PRINTFF(0,"\t[ring length: %d] [horizon length: %d] [N_msgs: %d]\n", rcv_ring_len, horizon, NMSG);


	//initialize
	SkyArrayConfig config;
	config.send_ring_len = send_ring_len;
	config.rcv_ring_len = rcv_ring_len;
	config.initial_rcv_sequence = rcv_sq0;
	config.initial_send_sequence = send_sq0;
	config.n_recall = n_recall;
	config.horizon_width = horizon;
	config.element_size = elesize;
	config.element_count = elecount;
	SkyArqRing* array_r = new_arq_ring(&config);
	uint8_t* tgt = x_alloc(10000);
	int last_received_idx = -1;					//order index of the last message received by FSW side
	int next_continuous_seq = rcv_sq0;			//sequence of the next message to be pushed into array
	int next_continuous_idx = 0;				//order index of the next message to be pushed in
	int* succesfully_given = x_calloc(sizeof(int)*(NMSG+500)*2); //boolean mapping on which messages have been given in.
	int X = horizon+4;
	while (last_received_idx < (NMSG-1)){

		//This goes through messages from already given sequences, to sequences that are too far ahead.
		for (int i = 0; i < X; ++i) {

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
			int r = skyArray_push_rx_packet(array_r, s->data, s->length, seq);

			if(wrap_seq(seq - next_continuous_seq) > horizon){ //outside window
				assert(r < 0);
				continue;
			}

			if((idx - (last_received_idx+1)) >= rcv_ring_len){ //array full
				assert(r < 0);
				continue;
			}
			if(succesfully_given[idx] == 1){ //already in
				assert(r < 0);
				continue;
			}

			assert(wrap_seq(rcv_sq0+last_received_idx+1) == array_r->primaryRcvRing->tail_sequence);
			assert(r == 0);
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
				assert(seq == wrap_seq(rcv_sq0+(last_received_idx+1)));
				assert(r == messages[last_received_idx+1]->length);
				assert(memcmp(tgt, messages[last_received_idx+1]->data, r) == 0);
				last_received_idx += 1;
			}
			uint16_t mask = skyArray_get_horizon_bitmap(array_r);
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
static void test3_round();

static void test3_send(int count){
	PRINTFF(0,"[ARQ ARRAY TEST 3: send side]\n");
	for (int i = 0; i < count; ++i) {
		test3_round();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}

static void test3_round(){
	int elesize = randint_i32(64,85);
	int elecount = randint_i32(10000,11000);
	int rcv_ring_len = randint_i32(20,35);
	int send_ring_len = randint_i32(20,35);
	int horizon = randint_i32(0, 16);
	int n_recall = horizon;
	int NMSG = randint_i32(3000, 6000);
	int n_strings = NMSG+500;
	String** messages = x_alloc(n_strings * sizeof(String*));
	for (int i = 0; i < n_strings; ++i) {
		messages[i] = get_random_string(randint_i32(0, SKY_ARRAY_MAXIMUM_PAYLOAD_SIZE));
	}
	int s_seq0 = randint_i32(0,100);
	int r_seq0 = randint_i32(0,100);

	SkyArrayConfig config;
	config.send_ring_len = send_ring_len;
	config.rcv_ring_len = rcv_ring_len;
	config.initial_rcv_sequence = r_seq0;
	config.initial_send_sequence = s_seq0;
	config.n_recall = n_recall;
	config.horizon_width = horizon;
	config.element_size = elesize;
	config.element_count = elecount;
	SkyArqRing* array = new_arq_ring(&config);
	uint8_t* tgt = x_alloc(10000);
	PRINTFF(0,"\t[ring length: %d] [recall depth: %d] [N_msgs: %d]\n", send_ring_len, n_recall, NMSG);
	//PRINTFF(0,"1");
	int next_idx_to_tx = 0;
	int next_idx_to_push = 0;
	int tail_seq = s_seq0;
	int in_buffer = 0;
	while (next_idx_to_tx < NMSG) {

		assert(skyArray_count_packets_to_tx(array, 0) == (next_idx_to_push - next_idx_to_tx));
		assert(tail_seq == array->primarySendRing->tail_sequence);
		assert(wrap_seq(s_seq0+next_idx_to_push) == array->primarySendRing->head_sequence);

		if ((randint_i32(0, 10000) % 7) == 0) { //push
			if(next_idx_to_push == NMSG){
				continue;
			}
			String* s = messages[next_idx_to_push];
			int r =skyArray_push_packet_to_send(array, s->data, s->length);
			if(wrap_seq(wrap_seq(s_seq0+next_idx_to_push) - tail_seq) == (send_ring_len-1)){
				assert(r < 0);
			}
			else {
				assert(r == 0);
				in_buffer++;
				next_idx_to_push++;
			}
		}
		//PRINTFF(0,"2");
		if ((randint_i32(0, 10000) % 7) == 0) { //pull
			int sq_tx = -1;
			int peeked0 = skyArray_peek_next_tx_size(array, 0);
			int peeked1 = skyArray_peek_next_tx_size(array, 1);
			int r = skyArray_read_packet_for_tx(array, tgt, &sq_tx, randint_i32(0,1));
			assert(peeked0 == r);
			assert(peeked1 == r);
			if(in_buffer == 0) {
				assert(r == RING_RET_EMPTY);
				assert(sq_tx == -1);
			} else {
				assert(r >= 0);
				assert(sq_tx == wrap_seq(s_seq0+next_idx_to_tx));
				assert(r == messages[next_idx_to_tx]->length);
				assert(memcmp(tgt, messages[next_idx_to_tx]->data, messages[next_idx_to_tx]->length) == 0);
				next_idx_to_tx++;
				in_buffer--;
				if(wrap_seq(wrap_seq(s_seq0+next_idx_to_tx) - tail_seq) > n_recall){
					tail_seq = wrap_seq(tail_seq + 1);
				}
			}
		}
		//PRINTFF(0,"3");
		if ((randint_i32(0, 10000) % 7) == 0) { //recall
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
							assert(array->primarySendRing->resend_count <= 16);
							if(x_in_i32_arr(idx, successful_scheduled_indexes, n_successful_schedules) < 0){
								successful_scheduled_indexes[n_successful_schedules] = idx;
								//successful_scheduled_sequences[n_successful_schedules] = seq;
								n_successful_schedules++;
							}
						} else{
							assert(r2 == RING_RET_RESEND_FULL);
							assert(array->primarySendRing->resend_count == 16);
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
					int peeked = skyArray_peek_next_tx_size(array, 1);
					int r3 = skyArray_read_packet_for_tx(array, tgt, &srcall, 1);
					//PRINTFF(0,"(%d/%d): %d %d    L: %d vs %d\n", j , n_successful_schedules, seq, srcall,  messages[idx]->length, r3);

					assert(r3 == messages[idx]->length);
					assert(memcmp(tgt, messages[idx]->data, r3) == 0);
					assert(r3 == peeked);
				}
				assert(array->primarySendRing->resend_count == 0);
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









