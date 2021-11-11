//
// Created by elmore on 28.10.2021.
//

#include "tst_utilities.h"


uint8_t arr_[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

SkyConfig* new_vanilla_config(){
	SkyConfig* config = SKY_MALLOC(sizeof(SkyConfig));
	config->array[0].n_recall 			= 16;
	config->array[0].horizon_width 		= 16;
	config->array[0].send_ring_len 		= 24;
	config->array[0].rcv_ring_len 		= 24;
	config->array[0].element_count	 	= 3600;
	config->array[0].element_size  		= 36;
	config->array[0].initial_send_sequence 	= 0;
	config->array[0].initial_rcv_sequence 	= 0;

	config->array[1].n_recall 			= 16;
	config->array[1].horizon_width 		= 16;
	config->array[1].send_ring_len 		= 24;
	config->array[1].rcv_ring_len 		= 24;
	config->array[1].element_count 		= 3600;
	config->array[1].element_size  		= 36;
	config->array[1].initial_send_sequence 	= 0;
	config->array[1].initial_rcv_sequence 	= 0;

	config->array[2].n_recall 			= 0;
	config->array[2].horizon_width 		= 0;
	config->array[2].send_ring_len 		= 8;
	config->array[2].rcv_ring_len 		= 8;
	config->array[2].element_count 		= 800;
	config->array[2].element_size  		= 36;
	config->array[2].initial_send_sequence 	= 0;
	config->array[2].initial_rcv_sequence 	= 0;

	config->array[3].n_recall 			= 0;
	config->array[3].horizon_width 		= 0;
	config->array[3].send_ring_len 		= 8;
	config->array[3].rcv_ring_len 		= 8;
	config->array[3].element_count 		= 800;
	config->array[3].element_size  		= 36;
	config->array[3].initial_send_sequence 	= 0;
	config->array[3].initial_rcv_sequence 	= 0;

	config->hmac.key_length 		= 8;
	config->hmac.maximum_jump 		= 24;
	memcpy(config->hmac.key, arr_, config->hmac.key_length);

	config->mac.maximum_gap_length 			= 1000;
	config->mac.minimum_gap_length 			= 50;
	config->mac.default_gap_length 			= 600;

	config->mac.maximum_window_length 		= 350;
	config->mac.minimum_window_length 		= 25;
	config->mac.default_window_length 		= 220;

	config->mac.default_tail_length 		= 86;
	config->mac.unauthenticated_mac_updates = 0;
	config->mac.shift_threshold_ms 			= 4000;

	config->identity[0] = 'O';
	config->identity[1] = 'H';
	config->identity[2] = 'F';
	config->identity[3] = 'S';
	config->identity[4] = '1';

	config->vc_priority[0] = 0;
	config->vc_priority[1] = 1;
	config->vc_priority[2] = 2;
	config->vc_priority[3] = 3;

	config->vc[0].arq_on = 1;
	config->vc[0].require_authentication = 1;
	config->vc[1].arq_on = 1;
	config->vc[1].require_authentication = 1;
	config->vc[2].arq_on = 0;
	config->vc[2].require_authentication = 0;
	config->vc[3].arq_on = 0;
	config->vc[3].require_authentication = 0;
	return config;
}

void destroy_config(SkyConfig* config){
	free(config);
}



SkyHandle new_handle(SkyConfig* config){
	SkyHandle handle = SKY_MALLOC(sizeof(struct sky_all));
	handle->conf = config;
	handle->mac = new_mac_system(&config->mac);
	handle->hmac = new_hmac_instance(&config->hmac);
	handle->diag = new_diagnostics();
	handle->phy = new_physical();
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		handle->arrayBuffers[i] = new_arq_ring(&config->array[i]);
	}
	return handle;
}


void destroy_handle(SkyHandle self){
	destroy_mac_system(self->mac);
	destroy_hmac(self->hmac);
	destroy_diagnostics(self->diag);
	destroy_physical(self->phy);
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		destroy_arq_ring(self->arrayBuffers[i]);
	}
	free(self);
}







uint16_t spin_to_seq(SkyArqRing* ring1, SkyArqRing* ring2, int target_sequence){
	uint8_t* tgt = malloc(1000);
	int i = 0;
	int first_tgt_sequence = sequence_wrap(target_sequence);
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
		if((i > 20) && (ring1->primarySendRing->tx_sequence == first_tgt_sequence)){
			break;
		}
	}
	assert(skyArray_get_horizon_bitmap(ring2) == 0);
	assert(skyArray_count_readable_rcv_packets(ring1) == 0);
	assert(skyArray_count_readable_rcv_packets(ring2) == 0);
	free(tgt);
	return 0;
}














