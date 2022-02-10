//
// Created by elmore on 27.11.2021.
//

#include "sky_tx_test.h"
#include "../src/skylink/skylink.h"
#include "../src/skylink/utilities.h"
#include "../src/skylink/reliable_vc.h"
#include "../src/skylink/fec.h"
#include "tst_utilities.h"
#include "tools/tools.h"
#include <assert.h>

void sky_tx_test_cycle();

void sky_tx_test(int load){
	PRINTFF(0, "[sky_tx test: randomized state]\n");
	for (int i = 0; i < load*1000 +1; ++i) {
		sky_tx_test_cycle();
		if(i%1000 == 0){
			PRINTFF(0,"\ti=%d\n",i);
		}
	}
	PRINTFF(0,"\t[\033[1;32mOK\033[0m]\n");
}

extern timestamp_t _global_time_now_ms;

void sky_tx_test_cycle(){
	uint8_t tgt[1000];

	SkyConfig* config = new_vanilla_config();
	SkyRadioFrame* frame = new_frame();
	fillrand(config->identity, SKY_IDENTITY_LEN);

	fillrand(frame->raw, SKY_FRAME_MAX_LEN);


	int golay_on = randint_i32(1,10) <= 5;
	int32_t now_ms = randint_i32(0, MOD_TIME_MS-1);
	_global_time_now_ms = now_ms;

	int mac_shift_threshold = randint_i32(MAC_IDLE_FRAMES_PER_WINDOW/2, MAC_IDLE_FRAMES_PER_WINDOW*2);
	config->mac.shift_threshold_ms = mac_shift_threshold;


	int arq_timeout = randint_i32(100,30000);
	config->arq_timeout_ms = arq_timeout;

	int auth_required[SKY_NUM_VIRTUAL_CHANNELS];
	int array_tx_seq0[SKY_NUM_VIRTUAL_CHANNELS];
	int array_rx_seq0[SKY_NUM_VIRTUAL_CHANNELS];
	int has_new_pl[SKY_NUM_VIRTUAL_CHANNELS];
	int n_new_pl[SKY_NUM_VIRTUAL_CHANNELS];
	int has_recall[SKY_NUM_VIRTUAL_CHANNELS];
	int n_recalled_pl[SKY_NUM_VIRTUAL_CHANNELS];
	int stuff_in_horizon[SKY_NUM_VIRTUAL_CHANNELS];
	int rr_need_on[SKY_NUM_VIRTUAL_CHANNELS];
	for (int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; ++vc) {
		auth_required[vc] = randint_i32(1,10) <= 5;
		config->vc[vc].require_authentication = auth_required[vc];

		array_tx_seq0[vc] = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
		array_rx_seq0[vc] = randint_i32(0, ARQ_SEQUENCE_MODULO-1);
		has_new_pl[vc] = randint_i32(1,10) <= 4;
		n_new_pl[vc] = 0;
		if(has_new_pl[vc]){
			n_new_pl[vc] = randint_i32(1, 4);
		}
		has_recall[vc] = randint_i32(1,10) <= 3;
		n_recalled_pl[vc] = 0;
		if(has_recall[vc]){
			n_recalled_pl[vc] = randint_i32(1, 4);
		}
	}

	// =================================================================================================================
	SkyHandle self = new_handle(config);
	SkyHandle self2 = new_handle(config);
	// =================================================================================================================

	int arq_on[SKY_NUM_VIRTUAL_CHANNELS];
	uint16_t auth_seq_rx[SKY_NUM_VIRTUAL_CHANNELS];
	uint16_t auth_seq_tx[SKY_NUM_VIRTUAL_CHANNELS];
	int frames_sent_per_vc[SKY_NUM_VIRTUAL_CHANNELS];
	int total_frames_sent = 0;
	int last_ctrl[SKY_NUM_VIRTUAL_CHANNELS];
	int arq_tx_ts[SKY_NUM_VIRTUAL_CHANNELS];
	int arq_rx_ts[SKY_NUM_VIRTUAL_CHANNELS];
	int arq_tx_timed_out[SKY_NUM_VIRTUAL_CHANNELS];
	int arq_rx_timed_out[SKY_NUM_VIRTUAL_CHANNELS];
	int hmac_reset_need[SKY_NUM_VIRTUAL_CHANNELS];
	for (int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; ++vc) {
		arq_on[vc] = randint_i32(1,10) <= 5;
		self->virtual_channels[vc]->arq_state_flag = ARQ_STATE_OFF;
		if(arq_on[vc]){
			self->virtual_channels[vc]->arq_state_flag = ARQ_STATE_ON;
		}

		auth_seq_rx[vc] = randint_i32(0, HMAC_CYCLE_LENGTH-1);
		auth_seq_tx[vc] = randint_i32(0, HMAC_CYCLE_LENGTH-1);
		self->hmac->sequence_rx[vc] = auth_seq_rx[vc];
		self->hmac->sequence_tx[vc] = auth_seq_tx[vc];

		frames_sent_per_vc[vc] = randint_i32(0, ARQ_IDLE_FRAMES_PER_WINDOW + 1);
		total_frames_sent += frames_sent_per_vc[vc];
		self->mac->frames_sent_in_current_window_per_vc[vc] = frames_sent_per_vc[vc];
		self->mac->total_frames_sent_in_current_window = total_frames_sent;

		spin_to_seq(self->virtual_channels[vc], self2->virtual_channels[vc], array_tx_seq0[vc], now_ms);
		spin_to_seq(self2->virtual_channels[vc], self->virtual_channels[vc], array_rx_seq0[vc], now_ms);
		assert(self->virtual_channels[vc]->sendRing->head_sequence == array_tx_seq0[vc]);
		assert(self->virtual_channels[vc]->rcvRing->head_sequence == array_rx_seq0[vc]);

		for (int i = 0; i < n_new_pl[vc] + n_recalled_pl[vc]; ++i) {
			String* s = get_random_string(randint_i32(0, 170));
			memset(s->data, s->length, s->length);
			sky_vc_push_packet_to_send(self->virtual_channels[vc], s->data, s->length);
			destroy_string(s);
		}
		for (int i = 0; i < n_recalled_pl[vc]; ++i) {

			int seq;
			sky_vc_read_packet_for_tx(self->virtual_channels[vc], tgt, &seq, 0);
			sky_vc_schedule_resend(self->virtual_channels[vc], wrap_sequence(array_tx_seq0[vc] + i));
			assert(seq == wrap_sequence(array_tx_seq0[vc] + i));
		}

		arq_tx_timed_out[vc] = randint_i32(1,12) <= 2;
		arq_rx_timed_out[vc] = randint_i32(1,12) <= 2;

		int _tx_since = randint_i32(0, arq_timeout-1);
		int _rx_since = randint_i32(0, arq_timeout-1);
		if(arq_tx_timed_out[vc]){
			_tx_since = randint_i32(arq_timeout+1, arq_timeout*2);
			//_tx_since = arq_timeout;
		}
		if(arq_rx_timed_out[vc]){
			_rx_since = randint_i32(arq_timeout+1, arq_timeout*2);
			//_rx_since = arq_timeout;
		}

		arq_tx_ts[vc] = wrap_time_ms(now_ms - _tx_since);
		arq_rx_ts[vc] = wrap_time_ms(now_ms - _rx_since);
		self->virtual_channels[vc]->last_tx_ms = arq_tx_ts[vc];
		self->virtual_channels[vc]->last_rx_ms = arq_rx_ts[vc];

		last_ctrl[vc] = wrap_time_ms(now_ms - randint_i32(1, arq_timeout/2));
		self->virtual_channels[vc]->last_ctrl_send = last_ctrl[vc];

		if(arq_on[vc]){
			stuff_in_horizon[vc] = 0;
			if(randint_i32(1, 10) <= 3){
				stuff_in_horizon[vc] = 1;
				fillrand(tgt, 100);
				int _len = randint_i32(0,100);
				memset(tgt, _len, 100);
				int _seq = wrap_sequence(
						array_rx_seq0[vc] + 1 + randint_i32(0, self->virtual_channels[vc]->rcvRing->horizon_width - 1));
				//PRINTFF(0,"h(%d) hz(%d) ", self->virtual_channels[vc]->rcvRing->head_sequence, self->virtual_channels[vc]->rcvRing->horizon_width);
				sky_vc_push_rx_packet(self->virtual_channels[vc], tgt, _len, _seq, now_ms); //this should not interfere with ts's
				//PRINTFF(0,"seq0(%d) _seq(%d) h(%d)\n", array_rx_seq0[vc], _seq, self->virtual_channels[vc]->rcvRing->head_sequence);
				assert(rcvRing_get_horizon_bitmap(self->virtual_channels[vc]->rcvRing) != 0);
			}
		}

		rr_need_on[vc] = 0;
		if(randint_i32(1, 10) <= 2){
			rr_need_on[vc] = 1;
			self->virtual_channels[vc]->need_recall = 1;
		}

		hmac_reset_need[vc] = randint_i32(1,10) <= 2;
		self->hmac->vc_enfocement_need[vc] = hmac_reset_need[vc];



	}

	int mac_silence = randint_i32(0, mac_shift_threshold * 2);
	int mac_last_belief_update = wrap_time_ms(now_ms - mac_silence);
	self->mac->last_belief_update = mac_last_belief_update;

	int round_robin_start = randint_i32(0, SKY_NUM_VIRTUAL_CHANNELS-1);
	self->mac->vc_round_robin_start = round_robin_start;

	int my_window = randint_i32(config->mac.minimum_window_length, config->mac.maximum_window_length);
	int peer_window = randint_i32(config->mac.minimum_window_length, config->mac.maximum_window_length);
	int mac_gap = config->mac.default_gap_length;
	int mac_tail = config->mac.default_tail_length;
	int mac_cycle = my_window + peer_window + mac_gap + mac_tail;
	int mac_window_on = randint_i32(0, 10) < 5;
	int mac_window_adjust_plan0 = randint_i32(-3, 3);
	self->mac->my_window_length = my_window;
	self->mac->peer_window_length = peer_window;
	self->mac->gap_constant = mac_gap;
	self->mac->tail_constant = mac_tail;
	self->mac->window_on = mac_window_on;
	self->mac->window_adjust_plan = mac_window_adjust_plan0;

	int can_send = randint_i32(1,10) <= 7;
	int mac_t0;
	if(can_send){
		mac_t0 = wrap_time_ms( (now_ms + mac_cycle * randint_i32(-1000, 0)) - randint_i32(0, my_window-1));
	}
	else {
		mac_t0 = wrap_time_ms( (now_ms + mac_cycle * randint_i32(-1000, 0)) - randint_i32(my_window +1, mac_cycle -1));
	}
	self->mac->T0_ms = mac_t0;


	int _content_to_send = 0;
	for (int ii = 0; ii < SKY_NUM_VIRTUAL_CHANNELS; ++ii) {
		_content_to_send |= sky_vc_content_to_send(self->virtual_channels[ii], config, now_ms, self->mac->frames_sent_in_current_window_per_vc[ii]);
		_content_to_send |= (hmac_reset_need[ii] && auth_required[ii]);
	}
	_content_to_send |= (total_frames_sent > 0);


	// =================================================================================================================
	// =================================================================================================================
	int ret = sky_tx(self, frame, golay_on);
	// =================================================================================================================
	if(ret){
		if(golay_on){
			uint32_t coded_len = (frame->raw[0] << 16) | (frame->raw[1] << 8) | frame->raw[2];
			int _golay_ret = decode_golay24(&coded_len);
			assert(_golay_ret >= 0);
			assert((coded_len & 0xF00) == (SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED));
			frame->length = (int32_t)coded_len & SKY_GOLAY_PAYLOAD_LENGTH_MASK;
			for (unsigned int i = 0; i < frame->length; i++)
				frame->raw[i] = frame->raw[i + 3];
		}
		// Decode FEC
		int fec_decode_ret = sky_fec_decode(frame, self->diag);
		assert(fec_decode_ret == 0);

		if(frame->flags & SKY_FLAG_AUTHENTICATED){
			//assert(frame->flags & SKY_FLAG_AUTHENTICATED);
			//todo check auth
			frame->length = frame->length - SKY_HMAC_LENGTH;
		}
		assert(frame->start_byte == SKYLINK_START_BYTE);
		assert(frame->length >= EXTENSION_START_IDX);
	}
	// =================================================================================================================


	//PRINTFF(0,"\n\t%d\n",can_send);
	if(mac_silence > mac_shift_threshold){
		assert(self->mac->last_belief_update == now_ms);
		int m = mac_cycle;
		int dt = wrap_time_ms(now_ms - self->mac->T0_ms);
		if( (((dt % m) + m) % m) < my_window){
			can_send = 1;
		} else {
			can_send = 0;
		}
	}

	if(mac_silence > mac_shift_threshold){
		assert(self->mac->last_belief_update == now_ms);
	}

	if(!can_send){
		if(ret != 0){
			PRINTFF(0,"\t%d\n", mac_silence > mac_shift_threshold);
			PRINTFF(0,"\tnow %d\n", now_ms);
			PRINTFF(0,"\tt0: %d  (%d)\n", mac_t0, self->mac->T0_ms);
			PRINTFF(0,"\tcycle: %d   (%d)\n", mac_cycle, self->mac->gap_constant + self->mac->tail_constant + self->mac->my_window_length + self->mac->peer_window_length);
			PRINTFF(0,"\tmy window: %d   (%d)\n", my_window, self->mac->my_window_length);
		}
		assert(ret == 0);
	}


	if(can_send){
		assert(self->mac->window_on == 1);
		if(mac_window_on == 0){
			if(mac_window_adjust_plan0 <= -2){
				//PRINTFF(0,"-");
				assert(self->mac->my_window_length == MAX(config->mac.minimum_window_length, my_window - config->mac.window_adjust_increment) );
				assert(self->mac->window_adjust_plan == 0);
			}
			else if(mac_window_adjust_plan0 >= 2){
				//PRINTFF(0,"+");
				assert(self->mac->my_window_length == MIN(config->mac.maximum_window_length, my_window + config->mac.window_adjust_increment) );
				assert(self->mac->window_adjust_plan == 0);
			} else {
				assert(self->mac->my_window_length == my_window);
			}
		} else {
			assert(self->mac->my_window_length == my_window);
		}
	}


	if(!can_send){
		assert(self->mac->total_frames_sent_in_current_window == 0);
		for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
			assert(self->mac->frames_sent_in_current_window_per_vc[i] == 0);
		}
	}
	if(!can_send){
		assert(self->mac->window_on == 0);
		goto exit;
	}


	// can_send == 1





	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		if(arq_on[i]){
			if(arq_rx_timed_out[i] || arq_tx_timed_out[i]){
				assert(self->virtual_channels[i]->last_tx_ms == 0);
				assert(self->virtual_channels[i]->last_rx_ms == 0);
				assert(self->virtual_channels[i]->last_ctrl_send == 0);
				assert(self->virtual_channels[i]->arq_state_flag == ARQ_STATE_OFF);
				assert(self->virtual_channels[i]->sendRing->head == 0);
				assert(self->virtual_channels[i]->sendRing->head_sequence == 0);
				assert(self->virtual_channels[i]->sendRing->tail == 0);
				assert(self->virtual_channels[i]->rcvRing->head == 0);
				assert(self->virtual_channels[i]->rcvRing->head_sequence == 0);
				assert(self->virtual_channels[i]->rcvRing->tail == 0);
			} else {
				assert(self->virtual_channels[i]->arq_state_flag == ARQ_STATE_ON);
			}
		}
	}



	int content = 0;
	int ctrl_ext = 0;
	int rr_ext = 0;
	int pl = 0;
	int recall = 0;
	int deduced_vc = -1;

	/*
	PRINTFF(0,"--------------\n");
	PRINTFF(0,"vc: %d\n", frame->vc);
	PRINTFF(0,"ext tdd: %d\n", get_extension(frame, EXTENSION_MAC_TDD_CONTROL) != NULL);
	PRINTFF(0,"ext hmac: %d\n", get_extension(frame, EXTENSION_HMAC_SEQUENCE_RESET) != NULL);
	PRINTFF(0,"ext arq seq: %d\n", get_extension(frame, EXTENSION_ARQ_SEQUENCE) != NULL);
	PRINTFF(0,"ext ctrl: %d\n", get_extension(frame, EXTENSION_ARQ_CTRL) != NULL);
	PRINTFF(0,"ext arq rr: %d\n", get_extension(frame, EXTENSION_ARQ_REQUEST) != NULL);
	PRINTFF(0,"arq on: %d\n", frame->flags & SKY_FLAG_ARQ_ON);
	PRINTFF(0,"pl: %d\n", frame->flags & SKY_FLAG_HAS_PAYLOAD);
	PRINTFF(0,"len pl: %d\n", frame->length - (EXTENSION_START_IDX + frame->ext_length));
	PRINTFF(0,"--------------\n");
	*/

	for (int j = 0; j < SKY_NUM_VIRTUAL_CHANNELS; ++j) {
		int i = ((round_robin_start + 0 + j) % SKY_NUM_VIRTUAL_CHANNELS);
		if(deduced_vc > -1){
			continue;
		}

		if(hmac_reset_need[i] && auth_required[i]){
			content = 1;
			assert(ret == 1);
			assert(get_extension(frame, EXTENSION_HMAC_SEQUENCE_RESET) != NULL);
		}

		if(!arq_on[i]){
			if(n_new_pl[i] > 0){
				content = 1;
				assert(ret == 1);
				pl = 1;
				assert(frame->flags & SKY_FLAG_HAS_PAYLOAD);
			}
		}


		if(arq_on[i] && (!arq_tx_timed_out[i]) && (!arq_rx_timed_out[i]) ){
			int util_frame = frames_sent_per_vc[i] < ARQ_IDLE_FRAMES_PER_WINDOW;
			//assert(frame->flags & SKY_FLAG_ARQ_ON);
			if(util_frame && (wrap_time_ms(now_ms - arq_tx_ts[i]) > (arq_timeout / 4)) ){
				content = 1;
				ctrl_ext = 1;
				assert(get_extension(frame, EXTENSION_ARQ_CTRL) != NULL);
				assert(ret == 1);
			}
			if(util_frame && (wrap_time_ms(now_ms - arq_rx_ts[i]) > (arq_timeout / 4)) ){
				content = 1;
				ctrl_ext = 1;
				assert(get_extension(frame, EXTENSION_ARQ_CTRL) != NULL);
				assert(ret == 1);
			}
			if(util_frame && (wrap_time_ms(now_ms - last_ctrl[i]) > (arq_timeout / 4)) ){
				content = 1;
				ctrl_ext = 1;
				assert(get_extension(frame, EXTENSION_ARQ_CTRL) != NULL);
				assert(ret == 1);
			}

			if(util_frame && (rr_need_on[i] || stuff_in_horizon[i]) ){
				content = 1;
				assert(ret == 1);
				rr_ext = 1;
				assert(get_extension(frame, EXTENSION_ARQ_REQUEST) != NULL);
			}
			if(n_new_pl[i] > 0){
				content = 1;
				assert(ret == 1);
				pl = 1;
				ctrl_ext = 1;
				assert(get_extension(frame, EXTENSION_ARQ_SEQUENCE) != NULL);
				assert(get_extension(frame, EXTENSION_ARQ_CTRL) != NULL);
				assert(frame->flags & SKY_FLAG_HAS_PAYLOAD);
			}
			if(n_recalled_pl[i] > 0){
				content = 1;
				assert(ret == 1);
				pl = 1;
				recall = 1;
				ctrl_ext = 1;
				assert(get_extension(frame, EXTENSION_ARQ_SEQUENCE) != NULL);
				assert(get_extension(frame, EXTENSION_ARQ_CTRL) != NULL);
				assert(frame->flags & SKY_FLAG_HAS_PAYLOAD);
			}

		}

		if((deduced_vc == -1) && content){
			deduced_vc = i;
			assert(frame->vc == deduced_vc);
		}
	}


	if((content == 0) && (total_frames_sent < MAC_IDLE_FRAMES_PER_WINDOW) && ((now_ms - mac_last_belief_update)<MAC_IDLE_TIMEOUT) ){
		content = 1;
		deduced_vc = 0;
	}


	if(content == 0){
		assert(ret == 0);
		goto exit;
	}
	assert(ret == 1);



	int arq_timed_out = 0;
	if(arq_on[deduced_vc]){
		if(arq_tx_timed_out[deduced_vc] || arq_rx_timed_out[deduced_vc]){
			arq_timed_out = 1;
		}
	}

	int pl_lenn = ((int)frame->length - (int)(EXTENSION_START_IDX + (int)frame->ext_length));
	assert(pl_lenn >= 0);
	assert(get_extension(frame, EXTENSION_MAC_TDD_CONTROL) != NULL);


	assert(frame->start_byte == SKYLINK_START_BYTE);
	assert(memcmp(frame->identity, config->identity, SKY_IDENTITY_LEN) == 0);
	if(frame->vc != deduced_vc){
		PRINTFF(0,"VC: %d\n",frame->vc);
		PRINTFF(0,"deduced VC: %d\n",deduced_vc);
		PRINTFF(0,"round robin start: %d\n", round_robin_start);
		PRINTFF(0,"arq_on: %d\n",arq_on[round_robin_start]);
		PRINTFF(0,"arq_on: %d\n",arq_on[frame->vc]);
		PRINTFF(0,"arq_on: %d\n",arq_on[deduced_vc]);
		PRINTFF(0, "ctrl: %d %d\n", ctrl_ext, get_extension(frame, EXTENSION_ARQ_CTRL) != NULL);
		PRINTFF(0, "rr: %d %d\n", rr_ext, get_extension(frame, EXTENSION_ARQ_REQUEST) != NULL);
	}
	assert(frame->vc == deduced_vc);
	if(arq_on[deduced_vc] && (!arq_timed_out)){
		assert(frame->flags & SKY_FLAG_ARQ_ON);
	} else {
		assert(!(frame->flags & SKY_FLAG_ARQ_ON));
	}




	SkyPacketExtension* ext = get_extension(frame, EXTENSION_ARQ_CTRL);
	if(ctrl_ext){
		assert(ext != NULL);
	} else {
		assert(ext == NULL);
	}


	ext = get_extension(frame, EXTENSION_ARQ_REQUEST);
	if(rr_ext){
		assert(ext != NULL);
	} else {
		assert(ext == NULL);
	}

	ext = get_extension(frame, EXTENSION_HMAC_SEQUENCE_RESET);
	if(hmac_reset_need[deduced_vc] && auth_required[deduced_vc]){
		assert(ext != NULL);
	} else {
		assert(ext == NULL);
	}


	if(pl || recall){
		assert(frame->flags & SKY_FLAG_HAS_PAYLOAD);
	} else {
		assert((frame->flags & SKY_FLAG_HAS_PAYLOAD) == 0);
	}




exit:
	destroy_config(config);
	destroy_frame(frame);
	destroy_handle(self);
	destroy_handle(self2);
}
