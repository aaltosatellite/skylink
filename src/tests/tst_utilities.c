//
// Created by elmore on 28.10.2021.
//

#include "tst_utilities.h"


uint8_t arr_[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

SkyConfig* new_vanilla_config(){
	SkyConfig* config = SKY_MALLOC(sizeof(SkyConfig));
	config->array.n_recall 		= 5;
	config->array.horizon_width = 5;
	config->array.send_ring_len = 12;
	config->array.rcv_ring_len 	= 12;
	config->array.element_count = 3600;
	config->array.element_size  = 36;
	config->array.initial_send_sequence = 0;
	config->array.initial_rcv_sequence = 0;

	config->hmac.key_length = 8;
	memcpy(config->hmac.key, arr_, config->hmac.key_length);
	config->hmac.magic_sequence = 7777;
	config->hmac.maximum_jump = 24;

	config->mac.maximum_gap_size = 200;
	config->mac.minimum_gap_size = 25;
	config->mac.maximum_window_size = 200;
	config->mac.minimum_window_size = 25;
	config->mac.default_window_length = 50;
	config->mac.default_gap_length = 150;
	config->mac.default_tail_length = 5;

	config->identity[0] = 'O';
	config->identity[1] = 'H';
	config->identity[2] = 'F';
	config->identity[3] = 'S';
	config->identity[4] = '1';

	config->vc_priority[0] = 0;
	config->vc_priority[1] = 1;
	config->vc_priority[2] = 2;
	config->vc_priority[3] = 3;

	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		config->vc[i].require_authentication = 1;
	}
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
		handle->arrayBuffers[i] = new_arq_ring(&config->array);
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




















