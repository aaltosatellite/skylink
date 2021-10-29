//
// Created by elmore on 29.10.2021.
//

#include "skylink_rx.h"


int sky_rx_process_ext_mac(SkyHandle self, ExtMACSpec macSpec);
int sky_rx_process_ext_arq_setup(SkyHandle self, ExtArqSetup arqSetup, int vc);
int sky_rx_process_ext_arq_req(SkyHandle self, ExtArqReq arqReq, int vc);
int sky_rx_process_extensions(SkyHandle self, SkyRadioFrame* frame);






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



int sky_rx_0(SkyHandle self, SkyRadioFrame* frame);
int sky_rx_1(SkyHandle self, SkyRadioFrame* frame);



int sky_rx_0(SkyHandle self, SkyRadioFrame* frame){
	// Read Golay decoded len
	uint32_t coded_len = (frame->raw[0] << 16) | (frame->raw[1] << 8) | frame->raw[2];

	int ret = decode_golay24(&coded_len);
	if (ret < 0) {
		// TODO: log the number of corrected bits?
		self->diag->rx_fec_fail++;
		return SKY_RET_GOLAY_FAILED;
	}

	if ((coded_len & 0xF00) != (SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED)){
		return -1;
	}

	frame->length = coded_len & SKY_GOLAY_PAYLOAD_LENGTH_MASK;

	// Remove the length header from the rest of the data
	for (unsigned int i = 0; i < frame->length; i++){
		frame->raw[i] = frame->raw[i + 3];
	}

	// Decode FEC
	if ((ret = sky_fec_decode(frame, self->diag)) < 0){
		return ret;
	}

	return sky_rx_1(self, frame);
}


int sky_rx_1(SkyHandle self, SkyRadioFrame *frame)
{
	int ret;

	// Decode packet
	if((ret = decode_skylink_packet(frame)) < 0){
		return ret;
	}

	// Check authentication code if the frame claims it is authenticated.
	if (frame->hmac_on) {
		if ((ret = sky_hmac_check_authentication(self, frame)) < 0)
			return ret;
	}

	// If the virtual channel necessitates auth, return error.
	if(sky_hmac_vc_demands_auth(self, frame->vc) && (!(frame->hmac_on))){
		return SKY_RET_AUTH_MISSING;
	}

	// Update MAC status
	if((frame->hmac_on) || self->conf->mac.unauthenticated_mac_updates){
		mac_update_belief(self->mac, &self->conf->mac, frame->rx_time_ms, frame->mac_length, frame->mac_remaining);
	}

	sky_rx_process_extensions(self, frame);

	int r = -1;
	if(!self->conf->vc[frame->vc].arq_on){
		r = skyArray_push_rx_packet_monotonic(self->arrayBuffers[frame->vc], frame->payload_read_start, frame->payload_read_length);
	}

	if(self->conf->vc[frame->vc].arq_on){
		if(!frame->arq_on){
			return SKY_RET_NO_MAC_SEQUENCE;
		}
		r = skyArray_push_rx_packet(self->arrayBuffers[frame->vc], frame->payload_read_start, frame->payload_read_length, frame->arq_sequence);
	}
	//todo: log behavior based on r.
	return 0;
}


int sky_rx_process_extensions(SkyHandle self, SkyRadioFrame* frame){
	for (int i = 0; i < frame->n_extensions; ++i) {
		SkyPacketExtension* ext = &frame->extensions[i];
		if(ext->type == EXTENSION_MAC_PARAMETERS){
			sky_rx_process_ext_mac(self, ext->ext_union.MACSpec);
		}
		if(ext->type == EXTENSION_ARQ_SETUP){
			sky_rx_process_ext_arq_setup(self, ext->ext_union.ArqSetup, frame->vc);
		}
		if(ext->type == EXTENSION_ARQ_RESEND_REQ){
			sky_rx_process_ext_arq_req(self, ext->ext_union.ArqReq, frame->vc);
		}
	}
	return 0;
}


int sky_rx_process_ext_mac(SkyHandle self, ExtMACSpec macSpec){
	//todo: implement. (the contents of this extension should probably be thought through);
	return 0;
}


int sky_rx_process_ext_arq_setup(SkyHandle self, ExtArqSetup arqSetup, int vc){
	self->conf->vc[vc].arq_on = (arqSetup.toggle > 0);
	if(arqSetup.toggle){
		skyArray_set_receive_sequence(self->arrayBuffers[vc], arqSetup.enforced_sequence, 0); //sequence gets wrapped anyway..
	}
	return 0;
}


int sky_rx_process_ext_arq_req(SkyHandle self, ExtArqReq arqReq, int vc){
	uint16_t mask = arqReq.mask1 + (arqReq.mask2 << 8);
	for (int i = 0; i < 16; ++i) {
		if(mask & (1<<i)){
			continue;
		}
		uint8_t sequence = (uint8_t) positive_modulo(arqReq.sequence + i, ARQ_SEQUENCE_MODULO);
		int r = skyArray_schedule_resend(self->arrayBuffers[vc], sequence);
		if(r < 0){
			//todo: implement actions for when recall is impossible. (A reset of ARQ sequence)
		}
	}
	return 0;
}






