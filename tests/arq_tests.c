//
// Created by elmore on 21.11.2021.
//

#include "arq_tests.h"
#include "../src/skylink/skylink.h"
#include "../src/skylink/utilities.h"
#include "../src/skylink/reliable_vc.h"
#include "tst_utilities.h"


void arq_system_test1(int load);
void arq_system_test1_cycle();

void arq_system_test2(int load);
void arq_system_test2_cycle();



void arq_system_test1(int load){
	PRINTFF(0, "[ARQ system test 1: ring mechanics.]\n");
	for (int i = 0; i < (1000*load +1); ++i) {
		if(i % 2000 == 0){
			PRINTFF(0,"\ti=%d\n", i);
		}
		arq_system_test1_cycle();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}


void arq_system_test1_cycle(){
	String* msgs[ARQ_MAXIMUM_HORIZON+10];
	for (int i = 0; i < ARQ_MAXIMUM_HORIZON+10; ++i) {
		msgs[i] = get_random_string(randint_i32(0, 173));
	}
	uint8_t* tgt = malloc(1000);
	SkyVCConfig config;
	config.send_ring_len = randint_i32(22,35);
	config.rcv_ring_len = randint_i32(20,35);
	config.element_size = 90;
	config.horizon_width = 16;
	SkyVirtualChannel* array = new_arq_ring(&config);
	SkyVirtualChannel* array_r = new_arq_ring(&config);

	int32_t now_ms = randint_i32(0,100000);
	int seq0 = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
	spin_to_seq(array, array_r, seq0, now_ms);

	for (int i = 0; i < ARQ_MAXIMUM_HORIZON+4; ++i) {
		int _seq;
		sky_vc_push_packet_to_send(array, msgs[i]->data, msgs[i]->length);
		int r_ = sky_vc_read_packet_for_tx(array, tgt, &_seq, 1);
		if(i > ARQ_MAXIMUM_HORIZON){
			assert(_seq == -1);
			assert(r_ < 0);
		} else{
			assert(_seq == wrap_sequence(seq0 + i));
			assert(r_ == msgs[i]->length);
		}
	}
	int n_confirm = randint_i32(-2,ARQ_MAXIMUM_HORIZON+3);
	int r_conf = sendRing_clean_tail_up_to(array->sendRing, array->elementBuffer, wrap_sequence(seq0 + n_confirm));
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







void arq_system_test2(int load){
	PRINTFF(0, "[ARQ system test 2: packet generation]\n");
	for (int i = 0; i < (1000*load + 1); ++i) {
		if(i%2000 == 0){
			PRINTFF(0,"\ti=%d\n", i);
		}
		arq_system_test2_cycle();
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}

/*
 * This test puts an ARQ ring into a randomized configuration, and tests packet generation.
 * Most aspects of packet formulation are tested.
 */
void arq_system_test2_cycle(){
	String* msgs[ARQ_MAXIMUM_HORIZON+10];
	for (int i = 0; i < ARQ_MAXIMUM_HORIZON+10; ++i) {
		msgs[i] = get_random_string(randint_i32(0, 173));
	}
	uint8_t* tgt = malloc(1000);
	SkyVCConfig config;
	int arq_idle_frames = randint_i32(0, 6);
	config.send_ring_len = randint_i32(22,35);
	config.rcv_ring_len = randint_i32(20,35);
	config.element_size = randint_i32(30,80);
	config.horizon_width = 16;
	SkyVirtualChannel* array = new_arq_ring(&config);
	SkyVirtualChannel* array_r = new_arq_ring(&config);

	SkyConfig* sky_conf = new_vanilla_config();
	sky_conf->arq_timeout_ticks = randint_i32(4000, 25000);
	sky_conf->arq_idle_frames_per_window = randint_i32(0, 3);

	int vc = randint_i32(0, SKY_NUM_VIRTUAL_CHANNELS-1);			//virtual channel randomized
	int32_t ts_base = randint_i32(0,100000);						//coarse timestamp
	int32_t ts_last_ctrl = ts_base + randint_i32(0,5000);			//random times for all timestamps
	int32_t ts_send = ts_base + randint_i32(0,5000);
	int32_t ts_recv = ts_base + randint_i32(0,5000);
	array->last_ctrl_send_tick = ts_last_ctrl;
	int seq0 = randint_i32(0, ARQ_SEQUENCE_MODULO-1);				//sequence number for send dierction
	int seq_rcv = randint_i32(0, ARQ_SEQUENCE_MODULO-1);			//sequence number for receive direction
	array->arq_state_flag = ARQ_STATE_OFF;								//random state.
	if(randint_i32(0,100) < 90){
		array->arq_state_flag = ARQ_STATE_ON;
		if(randint_i32(0,10)==0){
			array->arq_state_flag = ARQ_STATE_IN_INIT;
		}
	}
	spin_to_seq(array, array_r, seq0, ts_send);							//spin the arrays into the sequences we want.
	spin_to_seq(array_r, array, seq_rcv, ts_recv);
	array->last_tx_tick = ts_send;										//spin function sets receive timestamp, but not tx
	assert(array->last_tx_tick == ts_send);
	assert(array->last_rx_tick == ts_recv);

	//n_in_tail is how much send side tail lags tx_head. These are sent but unacked packets. (packets that can be recalled)
	int n_in_tail = randint_i32(0, ARQ_MAXIMUM_HORIZON-1);
	for (int i = 0; i < n_in_tail; ++i) {
		int _seq;
		sky_vc_push_packet_to_send(array, msgs[i]->data, msgs[i]->length);
		int r_ = sky_vc_read_packet_for_tx(array, tgt, &_seq, 1);
		assert(_seq == wrap_sequence(seq0 + i));
		assert(r_ == msgs[i]->length);
	}
	int seq1 = wrap_sequence(seq0 + n_in_tail);

	//wether there is a fresh payload to be sent.
	int new_pl = randint_i32(0, 10) < 8;
	int new_pl_seq = -1;
	if(new_pl){
		new_pl_seq = sky_vc_push_packet_to_send(array, msgs[n_in_tail]->data, msgs[n_in_tail]->length);
		if(new_pl_seq != seq1){
			PRINTFF(0,"%d %d\n", new_pl_seq, seq1);
		}
		assert(new_pl_seq == seq1);
	}

	//randomly request resend of already sent packet (and assert that the returs is appropriate)
	int recalled = randint_i32(0,10) < 2;
	int recall_seq = -1;
	if(recalled){
		int _n_back = randint_i32(0, n_in_tail + 2);
		recall_seq = wrap_sequence((seq1 - 1) - _n_back);
		int ret_recall = sky_vc_schedule_resend(array, recall_seq);
		if(_n_back < n_in_tail){
			//PRINTFF(0,"A  %d    %d\n", wrap_sequence(recall_seq - seq0), n_in_tail);
			assert(ret_recall == 0);
		} else {
			//PRINTFF(0,"B  %d    %d\n", wrap_sequence(recall_seq - seq0), n_in_tail);
			assert(ret_recall < 0);
			recalled = 0;
		}
	}

	//arq handshake
	uint32_t identifier = randint_i32(0, 900000);
	array->arq_session_identifier = identifier;
	int handshake_on = randint_i32(0,10) < 5;
	if(handshake_on){
		array->handshake_send = 1;
	}

	//arq handshake
	int frames_sent_in_vc = randint_i32(0, sky_conf->arq_idle_frames_per_window + 2);
	int now_ms = ts_base + randint_i32(sky_conf->arq_timeout_ticks / 10, sky_conf->arq_timeout_ticks + 1400);

	int own_recall = randint_i32(0, 10) < 4;
	int own_recall_mask_i = randint_i32(0, 13);
	int own_recall_mask = 1 << own_recall_mask_i;
	if (own_recall){
		int s = wrap_sequence(seq_rcv + 1 + own_recall_mask_i);
		sky_vc_push_rx_packet(array, msgs[0]->data, msgs[0]->length, s, now_ms);
		assert(array->last_rx_tick == ts_recv);
		assert(array->last_tx_tick == ts_send);
	}

	SkyRadioFrame* frame = new_frame();
	frame->length = 0;
	frame->ext_length = 0;
	frame->flags = 0;
	frame->start_byte = SKYLINK_START_BYTE;
	frame->vc = vc;
	frame->length = EXTENSION_START_IDX;
	frame->auth_sequence = 7777;

	int content0 = sky_vc_content_to_send(array, sky_conf, now_ms, frames_sent_in_vc);
	int content = sky_vc_fill_frame(array, sky_conf, frame, now_ms, frames_sent_in_vc);
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
			assert(sky_ntoh32( extArqHs->ARQHandshake.identifier ) == identifier);
		} else {
			assert(extArqHs == NULL);
		}

		if(own_recall && (frames_sent_in_vc < sky_conf->arq_idle_frames_per_window)){
			assert(extArqRr != NULL);
			assert(sky_ntoh16( extArqRr->ARQReq.sequence ) == seq_rcv);
			assert(sky_ntoh16( extArqRr->ARQReq.sequence ) == array->rcvRing->head_sequence);
			assert(sky_ntoh16( extArqRr->ARQReq.mask ) == own_recall_mask);
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
				assert(sky_ntoh16( extArqSeq->ARQSeq.sequence ) == recall_seq);
			} else {
				assert(sky_ntoh16( extArqSeq->ARQSeq.sequence ) == new_pl_seq);
			}
		}

		int b0 = (frames_sent_in_vc < sky_conf->arq_idle_frames_per_window);
		int b1 = wrap_time_ticks(now_ms - ts_send) > sky_conf->arq_timeout_ticks / 4;
		int b2 = wrap_time_ticks(now_ms - ts_recv) > sky_conf->arq_timeout_ticks / 4;
		int b3 = wrap_time_ticks(now_ms - ts_last_ctrl) > sky_conf->arq_timeout_ticks / 4;
		if((b0 && (b1 || b2 || b3)) || (frame->flags & SKY_FLAG_HAS_PAYLOAD)){
			assert(extArqCtrl != NULL);
			assert(sky_ntoh16( extArqCtrl->ARQCtrl.tx_sequence ) == seq1);
			if(new_pl && (!recalled)){
				assert(array->sendRing->tx_sequence == wrap_sequence(seq1 + 1));
			} else {
				assert(array->sendRing->tx_sequence == seq1);
			}
			assert(sky_ntoh16( extArqCtrl->ARQCtrl.rx_sequence ) == seq_rcv);
		} else {
			//PRINTFF(0,"%d %d %d %d\n", b0, b1, b2, b3);
			//PRINTFF(0,"%d %d %d\n", extArqCtrl == NULL, ts_base, MOD_TIME_TICKS);
			assert(extArqCtrl == NULL);
		}

		if(!(new_pl || recalled)){
			assert(extArqSeq == NULL);
			assert(!(frame->flags & SKY_FLAG_HAS_PAYLOAD));
		}

		int b_a = !new_pl && !recalled && !handshake_on;
		int b_b	= !(b0 && (b1 || b2 || b3));
		int b_c = !(own_recall && (frames_sent_in_vc < sky_conf->arq_idle_frames_per_window))  ;
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
			pl_i = wrap_sequence(recall_seq - seq0);
		}
		int pl_start_idx = (frame->ext_length + EXTENSION_START_IDX);
		assert((int)(frame->length - pl_start_idx) == msgs[pl_i]->length);
		assert(memcmp(msgs[pl_i]->data, frame->raw + pl_start_idx, msgs[pl_i]->length) == 0);
	}




	if(array->arq_state_flag == ARQ_STATE_IN_INIT){
		assert(extArqSeq == NULL);
		assert(extArqRr == NULL);
		assert(extArqCtrl == NULL);
		if(frames_sent_in_vc < sky_conf->arq_idle_frames_per_window){
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
	destroy_config(sky_conf);
	free(tgt);
}















