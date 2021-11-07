//
// Created by elmore on 2.11.2021.
//

#include "tx_rx_cycle_test.h"
#include "../skylink/elementbuffer.h"
#include "../skylink/arq_ring.h"
#include "../skylink/frame.h"
#include "../skylink/skylink.h"
#include "tst_utilities.h"
#include "tools/tools.h"

static void test1();
static void test1_round(int auth_on, int auth_misalign, int arq_on, int arq_misalign, int golay_on);



void txrx_tests(){
	test1();
}

int32_t i32min(int32_t a, int32_t b){
	if(a < b){
		return a;
	}
	return b;
}

int32_t i32max(int32_t a, int32_t b){
	if(a > b){
		return a;
	}
	return b;
}


int string_same_comparison(String** arr1, String** arr2, int n){
	for (int i = 0; i < n; ++i) {
		String* s1 = arr1[i];
		String* s2 = arr2[i];
		if(s1->length != s2->length){
			return 0;
		}
		int same = memcmp(s1->data, s2->data, s1->length);
		if(same != 0){
			return 0;
		}
	}
	return 1;
}



void test1(){
	PRINTFF(0,"[TX-RX Test 1: basic case]\n");
	int N = 400;
	for (int i = 0; i < N; ++i) {
		int a1 = randint_i32(0,1);
		int a2 = randint_i32(0,1);
		int a3 = randint_i32(0,1);
		int a4 = randint_i32(0,1);
		int a5 = randint_i32(0,1);
		test1_round(a1,a2,a3,a4, a5);
		if(i % 1 == 0){
			PRINTFF(0, "%d / %d \t\t(%d %d %d %d %d)\n",i,N,  a1,a2,a3,a4,a5);
		}
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}





void instantiate_testjob(TestJob* job){
	SkyConfig* config1 = new_vanilla_config();
	SkyConfig* config2 = new_vanilla_config();
	config1->hmac.maximum_jump = job->max_jump;
	config2->hmac.maximum_jump = job->max_jump;
	for (int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; ++vc) {
		TestJobVC *vcjob = &job->vcjobs[vc];
		config1->vc[vc].require_authentication = vcjob->hmac_on1;
		config2->vc[vc].require_authentication = vcjob->hmac_on2;
		config1->vc[vc].arq_on = vcjob->arq_on1;
		config2->vc[vc].arq_on = vcjob->arq_on2;
		config1->array[vc].initial_send_sequence = vcjob->arq_seq_1to2;
		config1->array[vc].initial_rcv_sequence = vcjob->arq_seq_2to1;
		config2->array[vc].initial_send_sequence = vcjob->arq_seq_2to1;
		config2->array[vc].initial_rcv_sequence = vcjob->arq_seq_1to2;
		config1->array[vc].n_recall = vcjob->recall1;
		config2->array[vc].n_recall = vcjob->recall2;
		config1->array[vc].horizon_width = vcjob->horizon1;
		config2->array[vc].horizon_width = vcjob->horizon2;
		config1->array[vc].rcv_ring_len = vcjob->ring_len;
		config1->array[vc].send_ring_len = vcjob->ring_len;
		config2->array[vc].rcv_ring_len = vcjob->ring_len;
		config2->array[vc].send_ring_len = vcjob->ring_len;
	}
	SkyHandle handle1 = new_handle(config1);
	SkyHandle handle2 = new_handle(config2);
	for (int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; ++vc){
		handle1->hmac->sequence_tx[vc] = wrap_hmac_sequence( job->vcjobs[vc].auth_seq_1to2 + job->vcjobs[vc].auth1_tx_shift);
		handle1->hmac->sequence_rx[vc] = job->vcjobs[vc].auth_seq_2to1;
		handle2->hmac->sequence_tx[vc] = wrap_hmac_sequence( job->vcjobs[vc].auth_seq_2to1 + job->vcjobs[vc].auth2_tx_shift);
		handle2->hmac->sequence_rx[vc] = job->vcjobs[vc].auth_seq_1to2;
		SkyArqRing* ring1 = handle1->arrayBuffers[vc];
		SkyArqRing* ring2 = handle2->arrayBuffers[vc];
		spin_to_seq(ring1, ring2, job->vcjobs[vc].arq_seq_1to2, job->vcjobs[vc].tx_ahead1);
	}
	job->handle1 = handle1;
	job->handle2 = handle2;
}



void test1_round(int auth_on, int auth_misalign, int arq_on, int arq_misalign, int golay_on){
	TestJob job;
	job.max_jump = 35;
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		TestJobVC* vcjob = &job.vcjobs[i];
		vcjob->ring_len 		= 28;
		vcjob->arq_seq_1to2 	= randint_i32(0, ARQ_SEQUENCE_MODULO-1);
		vcjob->arq_seq_2to1 	= randint_i32(0, ARQ_SEQUENCE_MODULO-1);
		vcjob->horizon1 		= randint_i32(3, 16);
		vcjob->horizon2 		= randint_i32(3, 16);
		vcjob->recall1 			= randint_i32(3, 16);
		vcjob->recall2 			= randint_i32(3, 16);
		vcjob->auth_seq_1to2 	= randint_i32(0, HMAC_CYCLE_LENGTH-1);
		vcjob->auth_seq_2to1 	= randint_i32(0, HMAC_CYCLE_LENGTH-1);
		vcjob->arq_on1 			= arq_on > 0; //randint_i32(0, 3) > 0;
		vcjob->arq_on2 			= arq_on > 0;
		vcjob->hmac_on1 		= auth_on > 0; //randint_i32(0, 3) > 0;
		vcjob->hmac_on2 		= auth_on > 0; //randint_i32(0, 3) > 0;
		vcjob->auth1_tx_shift 	= (!auth_misalign) ? randint_i32(0, job.max_jump) : randint_i32(job.max_jump+1, job.max_jump*12);
		vcjob->auth2_tx_shift 	= (!auth_misalign) ? randint_i32(0, job.max_jump) : randint_i32(job.max_jump+1, job.max_jump*12);
		vcjob->tx_ahead1 		= 0;
	}
	instantiate_testjob(&job);
	int vc = randint_i32(0, SKY_NUM_VIRTUAL_CHANNELS-1);

	SkyHandle handle1 = job.handle1;
	SkyHandle handle2 = job.handle2;
	uint8_t* tgt = malloc(1000);
	SendFrame *sendFrame = new_send_frame();
	RCVFrame *rcvFrame = new_receive_frame();

	String* sent_payloads[800];
	String* received_payloads[800];
	//int received_sequences[800];
	int n_sent = 0;
	int n_received = 0;

	int ama = 0;
	if(auth_on && auth_misalign){
		ama = 1;
	}
	int tx_ahead = randint_i32(0, i32min(job.vcjobs[vc].horizon2 -ama, job.vcjobs[vc].recall1 -(1 + ama)) );
	if (arq_misalign){
		int x = job.vcjobs[vc].horizon2;
		tx_ahead = randint_i32(x+1, x + 19);
	}

	for (int i = 0; i < tx_ahead; ++i) {
		String* s = get_random_string(randint_i32(0,150) );
		sent_payloads[n_sent] = s;
		int seq;
		skyArray_push_packet_to_send(job.handle1->arrayBuffers[vc], s->data, s->length);
		skyArray_read_packet_for_tx(job.handle1->arrayBuffers[vc], tgt, &seq, 1);
		n_sent++;
	}

	SkyArqRing* ring1 = job.handle1->arrayBuffers[vc];
	SkyArqRing* ring2 = job.handle2->arrayBuffers[vc];

	int N_SEND = 100;
	for (int i = 0; i < N_SEND; ++i) {
		if(skyArray_count_packets_to_tx(ring1, 1) == 0){
			String* s = get_random_string(randint_i32(0, 150));
			sent_payloads[n_sent] = s;
			n_sent++;
			skyArray_push_packet_to_send(ring1, s->data, s->length);
		}
		//PRINTFF(0, "n_sent:     %d \n", n_sent);

		sky_tx(handle1, sendFrame, vc, golay_on);
		memcpy(&rcvFrame->radioFrame, &sendFrame->radioFrame, sizeof(RadioFrame));
		sky_rx(handle2, rcvFrame, golay_on);

		while (skyArray_count_readable_rcv_packets(ring2)){
			int seq = -1;
			int read = skyArray_read_next_received(ring2, tgt, &seq);
			//received_sequences[n_received] = seq;
			received_payloads[n_received] = new_string(tgt, read);
			n_received++;
		}
		//PRINTFF(0, "n_received: %d \n\n", n_received);
		sky_tx(handle2, sendFrame, vc, golay_on);
		memcpy(&rcvFrame->radioFrame, &sendFrame->radioFrame, sizeof(RadioFrame));
		sky_rx(handle1, rcvFrame, golay_on);
	}

	//PRINTFF(0, "tx_ahead:  %d\n", tx_ahead);
	//PRINTFF(0, "recall 1:  %d\n", job.vcjobs[vc].recall1);
	//PRINTFF(0, "horizon 2: %d\n", job.vcjobs[vc].horizon2);
	//PRINTFF(0, "auth1 shift: %d\n", job.vcjobs[vc].auth1_tx_shift);

	for (int i = 0; i < 15; ++i) {
		//PRINTFF(0, " %d", sent_payloads[i]->length);
	}
	//PRINTFF(0,"\n");
	for (int i = 0; i < 15; ++i) {
		//PRINTFF(0, " %d", received_payloads[i]->length);
	}
	//PRINTFF(0,"\n");



	if(!arq_on){
		int same = string_same_comparison(&sent_payloads[tx_ahead+ama], &received_payloads[0], 25);
		assert(n_received == N_SEND-ama);
		assert(same);
	}
	if(arq_on && (!arq_misalign)){
		int same = string_same_comparison(&sent_payloads[0], &received_payloads[0], 25);
		assert(n_received == N_SEND-ama);
		assert(same);
	}
	if(arq_on && arq_misalign){
		int same = string_same_comparison(&sent_payloads[tx_ahead+ama], &received_payloads[0], 25);
		assert(n_received == (N_SEND- ama));
		assert(same);
	}


	for (int i = 0; i < n_sent; ++i) {
		destroy_string(sent_payloads[i]);
	}
	for (int i = 0; i < n_received; ++i) {
		destroy_string(received_payloads[i]);
	}


	free(tgt);
	destroy_receive_frame(rcvFrame);
	destroy_send_frame(sendFrame);
	destroy_config(handle1->conf);
	destroy_config(handle2->conf);
	destroy_handle(handle1);
	destroy_handle(handle2);
}











