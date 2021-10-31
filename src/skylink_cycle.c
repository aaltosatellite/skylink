//
// Created by elmore on 31.10.2021.
//

#include "skylink_cycle.h"

#define RADIO_OFF	0
#define RADIO_RX	1
#define RADIO_TX	2
int radio_mode = RADIO_OFF;


void set_radio_rx(){
	radio_mode = RADIO_RX;
}

void set_radio_tx(){
	radio_mode = RADIO_TX;
}


SkyHandle new_skylink(SkyConfig* config){
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


void destroy_skylink(SkyHandle self){
	destroy_mac_system(self->mac);
	destroy_hmac(self->hmac);
	destroy_diagnostics(self->diag);
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		destroy_arq_ring(self->arrayBuffers[i]);
	}
}






void cycle(SkyHandle self){
	int32_t now_ms = get_time_ms();
	if(mac_can_send(self->mac, now_ms)){
		if(radio_mode != RADIO_TX){
			set_radio_tx();
		}
		//tx_cycle(self);
	}

	else {
		if(radio_mode != RADIO_RX){
			set_radio_rx();
		}
		//rx_cycle(self);
	}


}






