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
	config->hmac.magic_sequence 	= 7777; //42863
	config->hmac.maximum_jump 		= 24;
	memcpy(config->hmac.key, arr_, config->hmac.key_length);

	config->mac.maximum_gap_size = 200;
	config->mac.minimum_gap_size = 25;
	config->mac.maximum_window_size = 200;
	config->mac.minimum_window_size = 25;
	config->mac.default_window_length = 50;
	config->mac.default_gap_length = 150;
	config->mac.default_tail_length = 5;
	config->mac.unauthenticated_mac_updates = 0;

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
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		handle->arrayBuffers[i] = new_arq_ring(&config->array[i]);
	}
	return handle;
}


void destroy_handle(SkyHandle self){
	destroy_mac_system(self->mac);
	destroy_hmac(self->hmac);
	destroy_diagnostics(self->diag);
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		destroy_arq_ring(self->arrayBuffers[i]);
	}
	free(self);
}




















