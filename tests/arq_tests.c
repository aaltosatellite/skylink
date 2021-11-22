//
// Created by elmore on 21.11.2021.
//

#include "arq_tests.h"
#include "../src/skylink/skylink.h"
#include "../src/skylink/frame.h"
#include "../src/skylink/elementbuffer.h"
#include "../src/skylink/arq_ring.h"
#include "../src/skylink/utilities.h"
#include "tst_utilities.h"
#include <math.h>


void arq_test1();
void arq_test1_cycle();

void arq_test2();
void arq_test2_cycle();

void arq_tests(){
	arq_test1();
	arq_test2();
}

void arq_test1(){
	PRINTFF(0, "[ARQ system test 1]\n");
	for (int i = 0; i < 3000; ++i) {
		arq_test1_cycle();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}


void arq_test1_cycle(){
	String* msgs[ARQ_MAXIMUM_HORIZON+10];
	for (int i = 0; i < ARQ_MAXIMUM_HORIZON+10; ++i) {
		msgs[i] = get_random_string(randint_i32(0, 173));
	}
	uint8_t* tgt = malloc(1000);
	SkyArrayConfig config;
	config.send_ring_len = randint_i32(22,35);
	config.rcv_ring_len = randint_i32(20,35);
	config.element_size = 90;
	config.element_count = 9000;
	config.horizon_width = 16;
	SkyArqRing* array = new_arq_ring(&config);
	SkyArqRing* array_r = new_arq_ring(&config);

	int32_t now_ms = randint_i32(0,100000);
	int seq0 = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
	spin_to_seq(array, array_r, seq0, now_ms);

	for (int i = 0; i < ARQ_MAXIMUM_HORIZON+4; ++i) {
		int _seq;
		skyArray_push_packet_to_send(array, msgs[i]->data, msgs[i]->length);
		int r_ = skyArray_read_packet_for_tx(array, tgt, &_seq, 1);
		if(i > ARQ_MAXIMUM_HORIZON){
			assert(_seq == -1);
			assert(r_ < 0);
		} else{
			assert(_seq == sequence_wrap(seq0 + i));
			assert(r_ == msgs[i]->length);
		}
	}
	int n_confirm = randint_i32(-2,ARQ_MAXIMUM_HORIZON+3);
	int r_conf = sendRing_clean_tail_up_to(array->sendRing, array->elementBuffer, sequence_wrap(seq0 + n_confirm));
	if((n_confirm >= 0) && (n_confirm <= (ARQ_MAXIMUM_HORIZON+1))){
		//PRINTFF(0,"%d %d\n", r_conf, n_confirm);
		assert(r_conf == n_confirm);
	} else {
		assert(r_conf < 0);
	}

	for (int i = 0; i < ARQ_MAXIMUM_HORIZON+10; ++i) {
		destroy_string(msgs[i]);
	}
	destroy_arq_ring(array);
	destroy_arq_ring(array_r);
	free(tgt);
}







void arq_test2(){
	PRINTFF(0, "[ARQ system test 2]\n");
	for (int i = 0; i < 10000; ++i) {
		arq_test2_cycle();
		if(i%1000 == 0){
			PRINTFF(0,"\ti=%d\n", i);
		}
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}


void arq_test2_cycle(){
	String* msgs[ARQ_MAXIMUM_HORIZON+10];
	for (int i = 0; i < ARQ_MAXIMUM_HORIZON+10; ++i) {
		msgs[i] = get_random_string(randint_i32(0, 173));
	}
	uint8_t* tgt = malloc(1000);
	SkyArrayConfig config;
	config.send_ring_len = randint_i32(22,35);
	config.rcv_ring_len = randint_i32(20,35);
	config.element_size = randint_i32(30,85);
	config.element_count = randint_i32(9000,12000);
	config.horizon_width = 16;
	SkyArqRing* array = new_arq_ring(&config);
	SkyArqRing* array_r = new_arq_ring(&config);

	int vc = randint_i32(0, SKY_NUM_VIRTUAL_CHANNELS-1);
	int32_t ts_base = randint_i32(0,100000);
	int32_t ts_last_ctrl = ts_base + randint_i32(0,5000);
	int32_t ts_send = ts_base + randint_i32(0,5000);
	int32_t ts_recv = ts_base + randint_i32(0,5000);
	array->last_ctrl_send = ts_last_ctrl;
	int seq0 = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
	int seq_rcv = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
	array->arq_state_flag = ARQ_STATE_OFF;
	if(randint_i32(0,100) < 90){
		array->arq_state_flag = ARQ_STATE_ON;
		if(randint_i32(0,10)==0){
			array->arq_state_flag = ARQ_STATE_IN_INIT;
		}
	}
	spin_to_seq(array, array_r, seq0, ts_send);
	spin_to_seq(array_r, array, seq_rcv, ts_recv);
	array->last_tx_ms = ts_send;
	assert(array->last_tx_ms == ts_send);
	assert(array->last_rx_ms == ts_recv);

	int n_in_tail = randint_i32(0, ARQ_MAXIMUM_HORIZON-1);
	for (int i = 0; i < n_in_tail; ++i) {
		int _seq;
		skyArray_push_packet_to_send(array, msgs[i]->data, msgs[i]->length);
		int r_ = skyArray_read_packet_for_tx(array, tgt, &_seq, 1);
		assert(_seq == sequence_wrap(seq0 + i));
		assert(r_ == msgs[i]->length);
	}
	int seq1 = sequence_wrap(seq0 + n_in_tail);

	int new_pl = randint_i32(0, 10) < 8;
	int new_pl_seq = -1;
	if(new_pl){
		new_pl_seq = skyArray_push_packet_to_send(array, msgs[n_in_tail]->data, msgs[n_in_tail]->length);
		if(new_pl_seq != seq1){
			PRINTFF(0,"%d %d\n", new_pl_seq, seq1);
		}
		assert(new_pl_seq == seq1);
	}

	int recalled = randint_i32(0,10) < 2;
	int recall_seq = -1;
	if(recalled){
		int _n_back = randint_i32(0, n_in_tail + 2);
		recall_seq = sequence_wrap((seq1-1) - _n_back );
		int ret_recall = skyArray_schedule_resend(array, recall_seq);
		if(_n_back < n_in_tail){
			//PRINTFF(0,"A  %d    %d\n", sequence_wrap(recall_seq - seq0), n_in_tail);
			assert(ret_recall == 0);
		} else {
			//PRINTFF(0,"B  %d    %d\n", sequence_wrap(recall_seq - seq0), n_in_tail);
			assert(ret_recall < 0);
			recalled = 0;
		}
	}

	int identifier = randint_i32(0, 900000);
	array->arq_session_identifier = identifier;
	int handshake_on = randint_i32(0,10) < 5;
	if(handshake_on){
		array->handshake_send = 1;
	}

	int frames_sent_in_vc = randint_i32(0,4);
	int now_ms = ts_base + randint_i32(5001, ARQ_TIMEOUT_MS+500);

	int own_recall = randint_i32(0, 10) < 4;
	int own_recall_mask_i = randint_i32(0, 13);
	int own_recall_mask = 1 << own_recall_mask_i;
	if (own_recall){
		int s = sequence_wrap(seq_rcv+1+own_recall_mask_i);
		skyArray_push_rx_packet(array, msgs[0]->data, msgs[0]->length, s, now_ms);
		assert(array->last_rx_ms == ts_recv);
		assert(array->last_tx_ms == ts_send);
	}

	SkyRadioFrame* frame = new_frame();
	frame->length = 0;
	frame->ext_length = 0;
	frame->flags = 0;
	frame->start_byte = SKYLINK_START_BYTE;
	frame->vc = vc;
	frame->length = EXTENSION_START_IDX;
	frame->auth_sequence = 7777;

	int content0 = skyArray_content_to_send(array, now_ms, frames_sent_in_vc);
	int content = skyArray_fill_frame(array, frame, now_ms, frames_sent_in_vc);
	SkyPacketExtension* extArqCtrl = get_extension(frame, EXTENSION_ARQ_CTRL);
	SkyPacketExtension* extArqSeq = get_extension(frame, EXTENSION_ARQ_SEQUENCE);
	SkyPacketExtension* extArqRr = get_extension(frame, EXTENSION_ARQ_REQUEST);
	SkyPacketExtension* extArqHs = get_extension(frame, EXTENSION_ARQ_HANDSHAKE);
	SkyPacketExtension* extTDDCtrl = get_extension(frame, EXTENSION_MAC_TDD_CONTROL);
	SkyPacketExtension* extHMAC = get_extension(frame, EXTENSION_HMAC_SEQUENCE_RESET);

	assert(content == content0);
	assert(extTDDCtrl == NULL);
	assert(extHMAC == NULL);
	assert(frame->vc == vc);
	assert(frame->auth_sequence == 7777);
	assert(frame->start_byte == SKYLINK_START_BYTE);

	if(array->arq_state_flag == ARQ_STATE_ON){
		if(handshake_on){
			assert(extArqHs != NULL);
			assert(extArqHs->ARQHandshake.peer_state == array->arq_state_flag);
			assert(extArqHs->ARQHandshake.identifier == identifier);
		} else {
			assert(extArqHs == NULL);
		}

		if(own_recall && (frames_sent_in_vc < 2)){
			assert(extArqRr != NULL);
			assert(extArqRr->ARQReq.sequence == seq_rcv);
			assert(extArqRr->ARQReq.sequence == array->rcvRing->head_sequence);
			assert(extArqRr->ARQReq.mask == own_recall_mask);
			//PRINTFF(0, "!");
		} else {
			//PRINTFF(0, "=");
			assert(extArqRr == NULL);
		}


		if(new_pl || recalled){
			if(extArqSeq == NULL){
				PRINTFF(0,"=!!=\n");
				PRINTFF(0,"seq0:%d  seq1:%d\n", seq0, seq1);
				PRINTFF(0,"new_pl:%d  new_pl_seq:%d\n", new_pl, new_pl_seq);
				PRINTFF(0,"recalled:%d  recall:%d\n", recalled, recall_seq);
			}
			assert(extArqSeq != NULL);
			assert(frame->flags & SKY_FLAG_HAS_PAYLOAD);
			if(recalled){
				assert(extArqSeq->ARQSeq.sequence == recall_seq);
			} else {
				assert(extArqSeq->ARQSeq.sequence == new_pl_seq);
			}
		}

		int b0 = (frames_sent_in_vc < 2);
		int b1 = wrap_time_ms(now_ms - ts_send) > ARQ_TIMEOUT_MS/4;
		int b2 = wrap_time_ms(now_ms - ts_recv) > ARQ_TIMEOUT_MS/4;
		int b3 = wrap_time_ms(now_ms - ts_last_ctrl) > ARQ_TIMEOUT_MS/4;
		if(b0 && (b1 || b2 || b3)){
			assert(extArqCtrl != NULL);
			assert(extArqCtrl->ARQCtrl.tx_sequence == seq1);
			if(new_pl && (!recalled)){
				assert(array->sendRing->tx_sequence == sequence_wrap(seq1+1));
			} else {
				assert(array->sendRing->tx_sequence == seq1);
			}
			assert(extArqCtrl->ARQCtrl.rx_sequence == seq_rcv);
		} else {
			//PRINTFF(0,"%d %d %d %d\n", b0, b1, b2, b3);
			//PRINTFF(0,"%d %d %d\n", extArqCtrl == NULL, ts_base, MOD_TIME_MS);
			assert(extArqCtrl == NULL);
		}

		if(!(new_pl || recalled)){
			assert(extArqSeq == NULL);
			assert(!(frame->flags & SKY_FLAG_HAS_PAYLOAD));
		}

		int b_a = !new_pl && !recalled && !handshake_on;
		int b_b	= !(b0 && (b1 || b2 || b3));
		int b_c = !(own_recall && (frames_sent_in_vc < 2))  ;
		if(b_a && b_b && b_c){
			assert(content == 0);
		} else{
			assert(content == 1);
		}

	}


	if(array->arq_state_flag == ARQ_STATE_OFF){
		assert(extArqHs == NULL);
		assert(extArqSeq == NULL);
		assert(extArqCtrl == NULL);
		assert(extArqRr == NULL);
		if(new_pl){
			assert(frame->flags & SKY_FLAG_HAS_PAYLOAD);
		} else {
			assert(!(frame->flags & SKY_FLAG_HAS_PAYLOAD));
		}
	}


	if(frame->flags & SKY_FLAG_HAS_PAYLOAD){
		int pl_i = n_in_tail;
		if(recalled && (array->arq_state_flag == ARQ_STATE_ON)){
			pl_i = sequence_wrap(recall_seq - seq0);
		}
		int pl_start_idx = (frame->ext_length + EXTENSION_START_IDX);
		assert((int)(frame->length - pl_start_idx) == msgs[pl_i]->length);
		assert(memcmp(msgs[pl_i]->data, frame->raw + pl_start_idx, msgs[pl_i]->length) == 0);
	}




	if(array->arq_state_flag == ARQ_STATE_IN_INIT){
		assert(extArqSeq == NULL);
		assert(extArqRr == NULL);
		assert(extArqCtrl == NULL);
		if(frames_sent_in_vc < 2){
			assert(extArqHs != NULL);
		}
		assert(!(frame->flags & SKY_FLAG_HAS_PAYLOAD));
	}


	for (int i = 0; i < ARQ_MAXIMUM_HORIZON+10; ++i) {
		destroy_string(msgs[i]);
	}
	destroy_frame(frame);
	destroy_arq_ring(array);
	destroy_arq_ring(array_r);
	free(tgt);
}















