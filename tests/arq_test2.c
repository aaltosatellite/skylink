//
// Created by elmore on 22.11.2021.
//

#include "arq_test2.h"
#include "../src/skylink/skylink.h"
#include "../src/skylink/utilities.h"
#include "../src/skylink/reliable_vc.h"
#include "tst_utilities.h"

extern tick_t _global_ticks_now;

void arq_system_test3_cycle();



void arq_system_test3(int load){
	PRINTFF(0, "[ARQ system test 3: process received pl and extensions]\n");
	for (int i = 0; i < (load*1000 +1); ++i) {
		arq_system_test3_cycle();
		if(i % 2000 == 0){
			PRINTFF(0,"\ti=%d\n", i);
		}
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}

/*
 * This is a rather excellent battery of tests. If this passes reliably, not too many things can be wrong.
 */
void arq_system_test3_cycle(){
	uint8_t tgt[1000];
	String* msgs[ARQ_MAXIMUM_HORIZON+10];
	for (int i = 0; i < ARQ_MAXIMUM_HORIZON+10; ++i) {
		msgs[i] = get_random_string(randint_i32(0, 173));
	}

	SkyConfig* sky_config = new_vanilla_config();

	SkyVCConfig config;
	config.send_ring_len = randint_i32(22,35);
	config.rcv_ring_len = randint_i32(20,35);
	config.element_size = randint_i32(30,80);
	config.horizon_width = 16;
	SkyVirtualChannel* array = new_virtual_channel(&config);
	SkyVirtualChannel* array_r = new_virtual_channel(&config);

	int32_t ts_base = randint_i32(0,100000);						//coarse timestamp
	int32_t ts_last_ctrl = ts_base + randint_i32(0,2000);			//random times for all timestamps
	int32_t ts_send = ts_base + randint_i32(0,2000);
	int32_t ts_recv = ts_base + randint_i32(0,2000);
	array->last_ctrl_send_tick = ts_last_ctrl;
	int seq_send0 = randint_i32(0, ARQ_SEQUENCE_MODULO-1);				//sequence number for send dierction
	int seq_recv0 = randint_i32(0, ARQ_SEQUENCE_MODULO-1);			//sequence number for receive direction
	spin_to_seq(array, array_r, seq_send0, ts_send);							//spin the arrays into the sequences we want.
	spin_to_seq(array_r, array, seq_recv0, ts_recv);
	array->last_tx_tick = ts_send;										//spin function sets receive timestamp, but not tx
	assert(array->last_tx_tick == ts_send);
	assert(array->last_rx_tick == ts_recv);
	assert(array->sendRing->head == array->sendRing->tail);
	assert(array->sendRing->head == array->sendRing->tx_head);
	assert(array->sendRing->head_sequence == array->sendRing->tx_sequence);
	assert(array->sendRing->head_sequence == array->sendRing->tail_sequence);
	assert(array->sendRing->head_sequence == seq_send0);
	assert(array->rcvRing->head == array->rcvRing->tail);
	assert(array->rcvRing->head_sequence == seq_recv0);
	assert(array->rcvRing->head_sequence == array->rcvRing->tail_sequence);

	tick_t now = ts_base + randint_i32(2001, sky_config->arq_timeout_ticks + 2000);
	_global_ticks_now = now;

	//ARQ state
	array->arq_state_flag = ARQ_STATE_OFF;
	if(randint_i32(1,10) <= 9){
		array->arq_state_flag = ARQ_STATE_ON;
		if(randint_i32(1,10) <= 2){
			array->arq_state_flag = ARQ_STATE_IN_INIT;
		}
	}
	int arq_state0 = array->arq_state_flag;
	uint32_t identifier = randint_u32(0, 0xFFFFFFFF);
	array->arq_session_identifier = identifier;


	//n_in_tail is how much send side tail lags tx_head. These are sent but unacked packets. (packets that can be recalled)
	int n_in_tail = randint_i32(0, ARQ_MAXIMUM_HORIZON-1);
	for (int i = 0; i < n_in_tail; ++i) {
		int _seq;
		sky_vc_push_packet_to_send(array, msgs[i]->data, msgs[i]->length);
		int r_ = sky_vc_read_packet_for_tx(array, tgt, &_seq, 1);
		assert(_seq == wrap_sequence(seq_send0 + i));
		assert(r_ == msgs[i]->length);
	}
	int seq_send1 = wrap_sequence(seq_send0 + n_in_tail);
	assert(array->sendRing->tx_sequence == seq_send1);
	assert(array->sendRing->tail_sequence == seq_send0);

	int unsent_packet = randint_i32(1,10) <= 5;
	if(unsent_packet){
		sky_vc_push_packet_to_send(array, msgs[n_in_tail]->data, msgs[n_in_tail]->length);
	}

	int recall = randint_i32(1,10) <= 5;
	int recall_seq = -1;
	int recall_mask = -1;
	if(recall){
		int _n_back = randint_i32(-3, n_in_tail + 3);
		recall_seq = wrap_sequence(seq_send0 + _n_back);
		recall_mask = randint_i32(0, 0xFFFF);
	}

	int hs = randint_i32(1,10) <= 5;
	int8_t hs_state = 0;
	uint32_t hs_identifier = 0;
	if(hs){
		if(randint_i32(0,1)==0){
			hs_state = ARQ_STATE_IN_INIT;
		} else {
			hs_state = ARQ_STATE_ON;
		}
		if(randint_i32(0,1)==0){
			hs_identifier = identifier;
		} else {
			hs_identifier = randint_u32(0, 0xFFFFFFFF);
		}
	}

	int ctrl_on = randint_i32(1,10) <= 5;
	int ctrl_peer_tx = -1;
	int ctrl_peer_rx = -1;
	if(ctrl_on){
		int _x = randint_i32(-3, n_in_tail+3);
		ctrl_peer_rx = wrap_sequence(seq_send0 + _x);

		_x = randint_i32(-5, 5);
		ctrl_peer_tx = wrap_sequence(seq_recv0 + _x);
	}

	int seq_on = randint_i32(1,10) <= 5;
	int arq_seq = -1;
	if(seq_on){
		int _x = randint_i32(-3, ARQ_MAXIMUM_HORIZON+3);
		arq_seq = wrap_sequence(seq_recv0 + _x);
	}

	int len_pl;
	uint8_t pl[300];
	fillrand(pl, 300);
	//wether there is a fresh payload to be sent.
	if(randint_i32(0,10) < 8){
		len_pl = randint_i32(0, 170);
	} else {
		len_pl = -1;
	}

	
	SkyParsedExtensions exts;
	memset(&exts, 0, sizeof(SkyParsedExtensions));
	SkyPacketExtension ext_arq_seq, ext_arq_ctrl, ext_arq_handshake, ext_arq_request;

	if(seq_on){
		exts.arq_sequence = &ext_arq_seq;
		ext_arq_seq.type = EXTENSION_ARQ_SEQUENCE;
		ext_arq_seq.length = sizeof(ExtARQSeq) + 1;
		ext_arq_seq.ARQSeq.sequence = sky_hton16(arq_seq);
	}
	if(ctrl_on){
		exts.arq_ctrl  = &ext_arq_ctrl;
		ext_arq_ctrl.type = EXTENSION_ARQ_CTRL;
		ext_arq_ctrl.length = sizeof(ExtARQCtrl) + 1;
		ext_arq_ctrl.ARQCtrl.tx_sequence = sky_hton16(ctrl_peer_tx);
		ext_arq_ctrl.ARQCtrl.rx_sequence = sky_hton16(ctrl_peer_rx);
	}
	if(hs){
		exts.arq_handshake = &ext_arq_handshake;
		ext_arq_handshake.type = EXTENSION_ARQ_HANDSHAKE;
		ext_arq_handshake.length = sizeof(ExtARQHandshake) + 1;
		ext_arq_handshake.ARQHandshake.peer_state = hs_state;
		ext_arq_handshake.ARQHandshake.identifier = sky_hton32(hs_identifier);
	}
	if(recall){
		exts.arq_request = &ext_arq_request;
		ext_arq_request.type = EXTENSION_ARQ_REQUEST;
		ext_arq_request.length = sizeof(ExtARQReq) + 1;
		ext_arq_request.ARQReq.sequence = sky_hton16(recall_seq);
		ext_arq_request.ARQReq.mask 	= sky_hton16(recall_mask);
	}

	sky_vc_process_content(array, pl, len_pl, &exts, now);


	int wiped = 0;
	if(hs){
		int b0 = arq_state0 == ARQ_STATE_OFF;
		int b1 = (arq_state0 == ARQ_STATE_ON) && (identifier != hs_identifier);
		int b2 = (arq_state0 == ARQ_STATE_IN_INIT) && (identifier < hs_identifier);
		//PRINTFF(0,"%d %d %d \n", b0, b1, b2);
		if(b0 || b1 || b2){
			//PRINTFF(0,"A");
			wiped = 1;
			int pl_through = (len_pl > -1) && (arq_seq <= array->rcvRing->horizon_width);
			int head_move = pl_through && (arq_seq == 0);
			assert(array->arq_state_flag == ARQ_STATE_ON);
			assert(array->arq_session_identifier == hs_identifier);
			assert(array->sendRing->head_sequence == 0);
			assert(array->sendRing->tx_sequence == 0);
			assert(array->sendRing->tail == 0);
			assert(array->sendRing->head == 0);
			assert(array->rcvRing->tail_sequence == 0);
			assert(array->rcvRing->head_sequence == head_move*1);
			assert(array->rcvRing->tail == 0);
			assert(array->rcvRing->head == head_move*1);
			assert(array->last_rx_tick == now);
			assert(array->last_tx_tick == now);
			if(pl_through && (arq_seq > 0)){
				assert(rcvRing_get_horizon_bitmap(array->rcvRing) != 0);
			} else {
				assert(rcvRing_get_horizon_bitmap(array->rcvRing) == 0);
			}
		}
	}

	if(!wiped){
		if(arq_state0 == ARQ_STATE_OFF){
			if(len_pl > -1){
				//PRINTFF(0,"B");
				assert(array->rcvRing->head_sequence == wrap_sequence(seq_recv0 + 1));
				assert(array->rcvRing->tail_sequence == wrap_sequence(seq_recv0));
				uint8_t temp[300];
				int maxlen = randint_i32(MAX(0, len_pl-3), len_pl+15);
				int red = sky_vc_read_next_received(array, &temp, maxlen);
				if(maxlen >= len_pl){
					assert(red == len_pl);
					assert(memcmp(temp, pl, len_pl) == 0);
					assert(array->rcvRing->tail_sequence == wrap_sequence(seq_recv0 + 1));
				} else {
					assert(red == SKY_RET_TOO_LONG_PAYLOAD);
					assert(array->rcvRing->tail_sequence == wrap_sequence(seq_recv0 + 1));
				}

			} else {
				//PRINTFF(0,"C");
				assert(array->rcvRing->head_sequence == wrap_sequence(seq_recv0));
				assert(array->rcvRing->tail_sequence == wrap_sequence(seq_recv0));
			}
			assert(rcvRing_get_horizon_bitmap(array->rcvRing) == 0);
			assert(array->sendRing->resend_count == 0);
		}


		int transition_to_on = ( (arq_state0 == ARQ_STATE_IN_INIT) && (hs && (hs_identifier == identifier)));
		if((arq_state0 == ARQ_STATE_IN_INIT) && (!transition_to_on)){
			//PRINTFF(0,"D");
			assert(array->arq_state_flag == ARQ_STATE_IN_INIT);
			assert(array->rcvRing->head_sequence == wrap_sequence(seq_recv0));
			assert(array->rcvRing->tail_sequence == wrap_sequence(seq_recv0));
			assert(array->last_tx_tick == ts_send);
			assert(array->last_rx_tick == ts_recv);
			assert(array->last_ctrl_send_tick == ts_last_ctrl);
			assert(rcvRing_get_horizon_bitmap(array->rcvRing) == 0);
			assert(array->sendRing->resend_count == 0);
		}


		if((arq_state0 == ARQ_STATE_ON) || transition_to_on){
			assert(array->arq_state_flag == ARQ_STATE_ON);
			int last_rx_updated = 0;


			if((len_pl > -1) && (seq_on) && (arq_seq == seq_recv0)){
				//PRINTFF(0,"(E %d)", transition_to_on);
				assert(array->rcvRing->head_sequence == wrap_sequence(seq_recv0 + 1));
				assert(array->rcvRing->tail_sequence == wrap_sequence(seq_recv0));
				last_rx_updated = 1;
				uint8_t temp[300];
				int maxlen = randint_i32(MAX(0, len_pl-3), len_pl+15);
				int red = rcvRing_read_next_received(array->rcvRing, array->elementBuffer, &temp, maxlen);
				if(maxlen >= len_pl){
					assert(red == len_pl);
					assert(memcmp(temp, pl, len_pl) == 0);
					assert(array->rcvRing->tail_sequence == wrap_sequence(seq_recv0 + 1));
					assert(rcvRing_get_horizon_bitmap(array->rcvRing) == 0);
				} else {
					assert(red == SKY_RET_TOO_LONG_PAYLOAD);
					assert(array->rcvRing->tail_sequence == wrap_sequence(seq_recv0 + 1));
					assert(rcvRing_get_horizon_bitmap(array->rcvRing) == 0);
				}
			}


			if ((len_pl == -1) || (!seq_on) || (wrap_sequence(arq_seq - seq_recv0) > array->rcvRing->horizon_width) ) {
				//PRINTFF(0,"F");
				assert(array->rcvRing->head_sequence == wrap_sequence(seq_recv0));
				assert(array->rcvRing->tail_sequence == wrap_sequence(seq_recv0));
				assert(rcvRing_get_horizon_bitmap(array->rcvRing) == 0);
				uint8_t temp[300];
				int maxlen = 500;
				int red = rcvRing_read_next_received(array->rcvRing, array->elementBuffer, &temp, maxlen);
				assert(red == SKY_RET_RING_EMPTY);
			}


			if ((len_pl > -1) && (seq_on) && (arq_seq != seq_recv0) && (wrap_sequence(arq_seq - seq_recv0) <= array->rcvRing->horizon_width) ) {
				//PRINTFF(0,"G");
				assert(array->rcvRing->head_sequence == wrap_sequence(seq_recv0));
				assert(array->rcvRing->tail_sequence == wrap_sequence(seq_recv0));
				assert(rcvRing_get_horizon_bitmap(array->rcvRing) != 0);
				uint8_t temp[300];
				int maxlen = 500;
				int red = rcvRing_read_next_received(array->rcvRing, array->elementBuffer, &temp, maxlen);
				assert(red == SKY_RET_RING_EMPTY);
			}


			int new_seq_send0 = seq_send0;
			int new_n_in_tail = n_in_tail;
			if(ctrl_on){
				//PRINTFF(0,"H");
				int _after_tail = wrap_sequence(ctrl_peer_rx - seq_send0) > 0;
				int _before_head = wrap_sequence(ctrl_peer_rx - seq_send0) < n_in_tail;
				int at_current = ctrl_peer_rx == seq_send1;
				if((_after_tail && _before_head ) || at_current){
					//PRINTFF(0,"H1 ");
					assert(array->last_tx_tick == now);
					assert(array->sendRing->tail_sequence == ctrl_peer_rx);
					new_seq_send0 = ctrl_peer_rx;
					new_n_in_tail = wrap_sequence(array->sendRing->tx_sequence - new_seq_send0);
					assert(new_n_in_tail == (n_in_tail - wrap_sequence(ctrl_peer_rx - seq_send0)));
				} else {
					//PRINTFF(0,"H2 ");
					assert(array->last_tx_tick == ts_send);
					assert(array->sendRing->tail_sequence == seq_send0);
				}

				if(ctrl_peer_tx == seq_recv0){
					//PRINTFF(0,"H3 ");
					last_rx_updated = 1;
					assert(array->need_recall == 0);
				}
				if((wrap_sequence(ctrl_peer_tx - seq_recv0) > 0) && (wrap_sequence(ctrl_peer_tx - seq_recv0) <= array->rcvRing->horizon_width) ) {
					//PRINTFF(0,"H4 ");
					assert(array->need_recall == 1);
				}
			}



			if(recall){
				//PRINTFF(0,"I");
				int ok_recall_s = 0;
				if((wrap_sequence(recall_seq - new_seq_send0) < new_n_in_tail) && (new_n_in_tail != 0) ){
					//PRINTFF(0,"I1 ");
					assert(array->sendRing->resend_count > 0);
					assert(x_in_u16_array(recall_seq, array->sendRing->resend_list, array->sendRing->resend_count) > -1);
					ok_recall_s++;
				}

				for (int i = 0; i < 16; ++i) {
					int s = wrap_sequence(recall_seq + i + 1);
					int b = recall_mask & (1 << i);
					if(b){
						continue;
					}
					if((wrap_sequence(s - new_seq_send0) < new_n_in_tail) && (new_n_in_tail != 0) ){
						//PRINTFF(0,"I2 ");
						assert(array->sendRing->resend_count > 0);
						assert(x_in_u16_array(s, array->sendRing->resend_list, array->sendRing->resend_count) > -1);
						ok_recall_s++;
					}
				}
				ok_recall_s = i32_min(ok_recall_s, 16);
				//PRINTFF(0,"\n--------\n");
				//PRINTFF(0,"resend count:%d    expected:%d\n", vc->sendRing->resend_count, ok_recall_s);
				//PRINTFF(0,"recall seq:%d   send_seq0:%d    n_in_tail:%d\n", recall_seq, seq_send0, new_n_in_tail);
				//PRINTFF(0,"MASK:%d  \n", recall_mask);
				//for (int i = 0; i < vc->sendRing->resend_count; ++i) {
					//PRINTFF(0,"%d ", vc->sendRing->resend_list[i]);
				//}
				//PRINTFF(0,"\n");
				assert(array->sendRing->resend_count == ok_recall_s);
			} else {
				//PRINTFF(0,"J");
				assert(array->sendRing->resend_count == 0);
			}


			if(last_rx_updated){
				assert(array->last_rx_tick == now);
			} else {
				assert(array->last_rx_tick == ts_recv);
			}

		}
	}






	for (int i = 0; i < ARQ_MAXIMUM_HORIZON+10; ++i) {
		destroy_string(msgs[i]);
	}
	destroy_virtual_channel(array);
	destroy_virtual_channel(array_r);
	destroy_config(sky_config);
}
