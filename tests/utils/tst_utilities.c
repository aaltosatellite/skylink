#include "tst_utilities.h"

SkyConfig* new_vanilla_config()
{
	SkyConfig* config = SKY_MALLOC(sizeof(SkyConfig));

	config->vc[0].horizon_width 		= 16;
	config->vc[0].send_ring_len 		= 24;
	config->vc[0].rcv_ring_len 			= 22;
	config->vc[0].element_size  		= 179;

	config->vc[1].horizon_width 		= 16;
	config->vc[1].send_ring_len 		= 24;
	config->vc[1].rcv_ring_len 			= 22;
	config->vc[1].element_size  		= 179;

	config->vc[2].horizon_width 		= 6;
	config->vc[2].send_ring_len 		= 12;
	config->vc[2].rcv_ring_len 			= 12;
	config->vc[2].element_size  		= 179;

	config->vc[3].horizon_width 		= 6;
	config->vc[3].send_ring_len 		= 12;
	config->vc[3].rcv_ring_len 			= 12;
	config->vc[3].element_size  		= 179;

	config->vc[0].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_REQUIRE_SEQUENCE;
	config->vc[1].require_authentication = SKY_CONFIG_FLAG_AUTHENTICATE_TX | SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION | SKY_CONFIG_FLAG_REQUIRE_SEQUENCE;
	config->vc[2].require_authentication = 0;
	config->vc[3].require_authentication = 0;

	config->arq.timeout_ticks           = 26000;
	config->arq.idle_frame_threshold    = config->arq.timeout_ticks / 4;
	config->arq.idle_frames_per_window  = 1;

	const uint8_t dummy_key[32] = {209, 20, 248, 100, 175, 77, 5, 118, 38, 204, 144, 17, 56, 109, 2, 158, 41, 177, 86, 7, 46, 17, 190, 165, 110, 32, 139, 229, 74, 13, 111, 179};
	const uint8_t *key_list[1] = { dummy_key };

	config->hmac.key_length = sizeof(dummy_key);
	memcpy(config->hmac.key, dummy_key, sizeof(dummy_key));
	config->hmac.maximum_jump           = 32;

	config->mac.gap_constant_ticks 				= 600;
	config->mac.tail_constant_ticks 			= 80;
	config->mac.minimum_window_length_ticks 	= 250;
	config->mac.maximum_window_length_ticks 	= 1000;
	config->mac.window_adjust_increment_ticks	= 250;
	config->mac.window_adjustment_period 		= 2;
	config->mac.unauthenticated_mac_updates 	= 0;
	config->mac.shift_threshold_ticks 			= 10000;
	config->mac.idle_frames_per_window 			= 0;
	config->mac.idle_timeout_ticks 				= 30000;
	config->mac.carrier_sense_ticks				= 200;


	memcpy(config->identity, "test", 4);
	config->identity_len = 4;

	return config;
}



uint16_t spin_to_seq(SkyVirtualChannel* sring, SkyVirtualChannel* rring, int target_sequence, int32_t now_ms){
	uint8_t* tgt = malloc(1000);
	int i = 0;
	int first_tgt_sequence = wrap_sequence(target_sequence);
	String* s1 = get_random_string(randint_i32(0,100));
	String* s2 = get_random_string(randint_i32(0,100));
	String* ss[2] = {s1,s2};
	int r_tx_0 = rring->sendRing->tx_sequence;
	int s_rx_0 = sring->rcvRing->head_sequence;

	int fe_s = sring->elementBuffer->free_elements;
	int fe_r = rring->elementBuffer->free_elements;

	while (1){
		i++;
		String* s = ss[randint_i32(0,1)];
		int seq2;
		int seq = sky_vc_push_packet_to_send(sring, s->data, s->length);
		int red = sky_vc_read_packet_for_tx(sring, tgt, &seq2, 1);
		sendRing_clean_tail_up_to(sring->sendRing, sring->elementBuffer, sring->sendRing->tx_sequence);
		assert(seq == seq2);
		assert(red == s->length);
		assert(sring->sendRing->tx_head == sring->sendRing->head);
		assert(sring->sendRing->tx_head == sring->sendRing->tail);
		//quick_exit(1);

		//assert(sring->elementBuffer->free_elements == sring->elementBuffer->element_count - element_buffer_element_requirement_for(sring->elementBuffer, s->length));
		int head_advanced = sky_vc_push_rx_packet(rring, tgt, s->length, seq, now_ms);
		red = sky_vc_read_next_received(rring, tgt, s->length);
		assert(head_advanced == 1);
		assert(red == s->length);
		assert(sring->sendRing->tx_sequence == rring->rcvRing->head_sequence);
		if((i > 20) && (sring->sendRing->tx_sequence == first_tgt_sequence)){
			break;
		}
	}

	assert(fe_s == sring->elementBuffer->free_elements);
	assert(fe_r == rring->elementBuffer->free_elements);

	assert(r_tx_0 == rring->sendRing->tx_sequence);
	assert(s_rx_0 == sring->rcvRing->head_sequence);
	destroy_string(s1);
	destroy_string(s2);
	assert(rcvRing_get_horizon_bitmap(rring->rcvRing) == 0);
	assert(sky_vc_count_readable_rcv_packets(sring) == 0);
	assert(sky_vc_count_readable_rcv_packets(rring) == 0);
	assert(sky_vc_count_packets_to_tx(sring, 1) == 0);
	assert(sky_vc_count_packets_to_tx(rring, 1) == 0);
	free(tgt);
	return 0;
}



void populate_horizon(SkyVirtualChannel* sring, SkyVirtualChannel* rring, int final_tx_head_seq, int final_rx_head_seq, uint16_t target_mask, int32_t now_ms, String** payloads){
	spin_to_seq(sring, rring, final_rx_head_seq, now_ms);
	if(final_tx_head_seq == final_rx_head_seq){
		return;
	}
	uint8_t* tgt = malloc(1000);
	assert(wrap_sequence(final_tx_head_seq - final_rx_head_seq) <= 16);
	int r_tx_0 = rring->sendRing->tx_sequence;
	int s_rx_0 = sring->rcvRing->head_sequence;

	String* s0;
	if(payloads){
		s0 = payloads[0];
	} else {
		s0 = get_random_string(0);
	}
	int seq2;
	int seq = sky_vc_push_packet_to_send(sring, s0->data, s0->length);
	int red = sky_vc_read_packet_for_tx(sring, tgt, &seq2, 1);
	assert(seq == seq2);
	assert(red == s0->length);
	if(!payloads){
		destroy_string(s0);
	}

	for (int i = 0; i < 16; ++i) {
		if(sring->sendRing->head_sequence == final_tx_head_seq){
			break;
		}
		String* s;
		if(payloads){
			s = payloads[i+1];
		} else {
			s = get_random_string(1+i);
		}
		seq = sky_vc_push_packet_to_send(sring, s->data, s->length);
		red = sky_vc_read_packet_for_tx(sring, tgt, &seq2, 1);
		assert(seq == seq2);
		assert(red == s->length);
		int bol = target_mask & (1<<i);
		if(bol){
			int head_advanced = sky_vc_push_rx_packet(rring, tgt, s->length, seq, now_ms);
			assert(head_advanced == 0);
			red = sky_vc_read_next_received(rring, tgt, 1000);
			assert(red == SKY_RET_RING_EMPTY);
		}
		if(!payloads){
			destroy_string(s);
		}
	}

	assert(sring->sendRing->head_sequence == final_tx_head_seq);
	assert(rring->rcvRing->head_sequence == final_rx_head_seq);

	uint16_t mask = rcvRing_get_horizon_bitmap(rring->rcvRing);
	assert(mask == target_mask);

	assert(r_tx_0 == rring->sendRing->tx_sequence);
	assert(s_rx_0 == sring->rcvRing->head_sequence);
	free(tgt);
}




int roll_chance(double const chance){
	int r = rand(); // NOLINT(cert-msc50-cpp)
	double rd = (double)r;
	double rM = (double)RAND_MAX;
	double rr = rd/rM;
	return rr < chance;
}


uint8_t get_other_byte(uint8_t c){
	uint8_t c2;
	do {
		c2 = randint_i32(0,255);
	} while (c2 == c);
	return c2;
}

void corrupt_bytearray(uint8_t* arr, int length, double ratio){
	for (int i = 0; i < length; ++i) {
		int roll = roll_chance(ratio);
		if(roll){
			arr[i] = get_other_byte(arr[i]);
		}
	}
}


void tst_randoms(double chance1, double chance2, int NN){
	uint64_t count = 0;
	for (int i = 0; i < NN; ++i) {
		count += roll_chance(chance1);
	}
	double rate = (double) count / (double) NN;
	PRINTFF(0,"Roll chance %lf: %d out of %d.  (~%lf)\n", chance1, count, NN, rate);

	uint8_t* arr1 = malloc(NN);
	uint8_t* arr2 = malloc(NN);
	fillrand(arr1, NN);
	memcpy(arr2, arr1, NN);
	corrupt_bytearray(arr2, NN, chance2);
	count = 0;
	for (int i = 0; i < NN; ++i) {
		if(arr1[i] != arr2[i]){
			count++;
		}
	}
	rate = (double) count / (double) NN;
	PRINTFF(0,"Corrupt chance %lf: %d out of %d.  (~%lf)\n", chance2, count, NN, rate);
	free(arr1);
	free(arr2);
}







#if 0
SkyHeaderExtension* get_extension(SkyRadioFrame* frame, unsigned int extension_type)
{
	if((int)(frame->ext_length + EXTENSION_START_IDX) > (int)frame->length)
		return NULL; // Too short packet.

	if(frame->ext_length <= 1)
		return NULL; // No extensions.

	unsigned int cursor = 0;
	while (cursor < frame->ext_length) {

		SkyHeaderExtension* ext = (SkyHeaderExtension*)&frame->raw[EXTENSION_START_IDX + cursor];
		if (cursor + ext->length >= frame->length)
			return NULL;
		if(ext->length == 0)
			return NULL;

		if (extension_type == ext->type)
			return ext;

		cursor += ext->length;
	}
	return NULL;
}
#endif