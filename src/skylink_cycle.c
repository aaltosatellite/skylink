//
// Created by elmore on 31.10.2021.
//

#include "skylink_cycle.h"






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


void tx_cycle(SkyHandle self, SkyRadioFrame* frame){
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		uint8_t vc = self->conf->vc_priority[i];
		int content = sky_tx(self, frame, vc);
		if(content || (self->radio->packets_transmitted_this_cycle < 2)){
			radio_transmit(self->radio, frame->raw, frame->length);
		}
	}
}



void rx_cycle(SkyHandle self, SkyRadioFrame* frame){
	int length = 0;
	radio_receive(self->radio, frame->raw, &length);
	frame->length = (uint16_t) length;
	if(frame->length > 0){
		sky_rx_0(self, frame);
	}
}



void cycle(SkyHandle self, SkyRadioFrame* frame){
	int32_t now_ms = get_time_ms();
	int can_send = mac_can_send(self->mac, now_ms);
	if(can_send){
		if(self->radio->mode != RADIO_TX){
			set_radio_tx(self->radio);
		}
		tx_cycle(self, frame);
	}
	else {
		if(self->radio->mode != RADIO_RX){
			set_radio_rx(self->radio);
		}
		rx_cycle(self, frame);
	}
}






