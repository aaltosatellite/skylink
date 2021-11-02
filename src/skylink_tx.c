//
// Created by elmore on 29.10.2021.
//

#include "skylink_tx.h"

/*
int sky_tx_content_to_transmit(SkyHandle self, uint8_t vc){
	if(skyArray_count_packets_to_tx(self->arrayBuffers[vc], 1)){
		return 1;
	}
	if(skyArray_get_horizon_bitmap(self->arrayBuffers[vc])){
		return 1;
	}
	if(self->arrayBuffers[vc]->state_enforcement_need == 0){
		return 1;
	}
	return 0;
}
*/


static int sky_tx_extension_eval_arq_rr(SkyHandle self, SendFrame* frame, uint8_t vc){
	uint16_t resend_map = skyArray_get_horizon_bitmap(self->arrayBuffers[vc]);
	if(resend_map == 0){
		return 0;
	}
	sky_packet_add_extension_arq_rr(frame, self->arrayBuffers[vc]->primaryRcvRing->head_sequence, resend_map&0xFF, ((resend_map &0xFF00) >> 8));
	return 1;
}


static int sky_tx_extension_eval_arq_enforce(SkyHandle self, SendFrame* frame, uint8_t vc){
	if(self->arrayBuffers[vc]->state_enforcement_need == 0){
		return 0;
	}
	self->arrayBuffers[vc]->state_enforcement_need = 0;
	uint8_t sequence = skyArray_get_next_transmitted_sequence(self->arrayBuffers[vc]);
	sky_packet_add_extension_arq_enforce(frame, self->conf->vc->arq_on, sequence);
	return 1;
}


static int sky_tx_extension_eval_hmac_enforce(SkyHandle self, SendFrame* frame, uint8_t vc){
	if(self->hmac->vc_enfocement_need[vc] == 0){
		return 0;
	}
	self->hmac->vc_enfocement_need[vc] = 0;
	uint16_t sequence = wrap_hmac_sequence(self->hmac->sequence_rx[vc] + 2); //+2 so that immediate sends don't ivalidate what we give here. Jump constant must be bigger.
	sky_packet_add_extension_hmac_enforce(frame, sequence);
	return 1;
}






int sky_tx(SkyHandle self, SendFrame* frame, uint8_t vc){
	int content = 0;

	/* identity gets copied to the raw-array from frame's own identity field */
	memcpy(frame->radioFrame.identity, self->conf->identity, SKY_IDENTITY_LEN);

	frame->radioFrame.vc = vc;

	frame->radioFrame.flags = 0;

	/* Set HMAC state and sequence */
	frame->radioFrame.auth_sequence = 0;
	if(self->conf->vc[vc].require_authentication){
		frame->radioFrame.flags |= SKY_FLAG_AUTHENTICATED;
		frame->radioFrame.auth_sequence = sky_hmac_get_next_hmac_tx_sequence_and_advance(self, vc);
	}

	/* ARQ status. This is kind of dumb thing to do here, since we don't yet know if there will be a payload. */
	if(self->conf->vc[vc].arq_on){
		frame->radioFrame.flags |= SKY_FLAG_ARQ_ON;
		frame->radioFrame.arq_sequence = ARQ_SEQUENCE_NAN;
	}


	/* Add extension to the packet. ARQ */
	frame->radioFrame.ext_length = 0;
	content |= sky_tx_extension_eval_arq_rr(self, frame, vc);
	content |= sky_tx_extension_eval_arq_enforce(self, frame, vc);
	content |= sky_tx_extension_eval_hmac_enforce(self, frame, vc);

	/* Encode the extensions. After this step, we know the remaining space in frame. */
	//encode_skylink_packet_extensions(frame);

	/* If there is enough space in frame, copy a payload to the end. Then add ARQ the sequence number obtained from ArqRing. */
	int next_pl_size = skyArray_peek_next_tx_size(self->arrayBuffers[vc], 1);
	if((next_pl_size >= 0) && (available_payload_space(&frame->radioFrame) >= next_pl_size)){
		int arq_sequence = -1;
		int read = skyArray_read_packet_for_tx(self->arrayBuffers[vc], frame->radioFrame.raw + frame->radioFrame.length, &arq_sequence, 1);
		if(self->conf->vc[vc].arq_on){
			frame->radioFrame.arq_sequence = (uint8_t)arq_sequence;
		}
		frame->radioFrame.length += read;
		content = 1;
	}

	/* Set MAC data fields. */
	int32_t now_ms = get_time_ms();
	mac_set_frame_fields(self->mac, &frame->radioFrame, now_ms);

	/* Encode the above information, not the extensions. */
	//encode_skylink_packet_header(frame);

	/* Authenticate the frame */
	if(self->conf->vc[vc].require_authentication){
		sky_hmac_extend_with_authentication(self, frame);
	}

	/* Apply Forward Error Correction (FEC) coding */
	sky_fec_encode(&frame->radioFrame);

	/* Move the data by 3 bytes to make room for the PHY header */
	for (unsigned int i = frame->radioFrame.length; i != 0; i--){
		frame->radioFrame.raw[i + 3] = frame->radioFrame.raw[i];
	}

	/* Encode length field. */
	uint32_t phy_header = frame->radioFrame.length | SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED;
	encode_golay24(&phy_header);
	frame->radioFrame.raw[0] = 0xff & (phy_header >> 16);
	frame->radioFrame.raw[1] = 0xff & (phy_header >> 8);
	frame->radioFrame.raw[2] = 0xff & (phy_header >> 0);

	frame->radioFrame.length += 3; //todo: necessary?

	++self->diag->tx_frames;

	return content;
}











