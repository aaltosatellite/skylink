//
// Created by elmore on 29.10.2021.
//

#include "skylink/skylink.h"
#include "skylink/conf.h"
#include "skylink/fec.h"
#include "skylink/arq_ring.h"
#include "skylink/frame.h"
#include "skylink/mac.h"
#include "skylink/hmac.h"
#include "skylink/utilities.h"
#include "skylink/phy.h"

#define UTILITY_FRAMES_PER_WINDOW	2

static int sky_tx_extension_needed_arq_rr(SkyHandle self, uint8_t vc){
	if(self->phy->frames_sent_in_current_window_per_vc[vc] >= UTILITY_FRAMES_PER_WINDOW){
		return 0;
	}
	uint16_t resend_map = skyArray_get_horizon_bitmap(self->arrayBuffers[vc]);
	if( (resend_map == 0) && (self->arrayBuffers[vc]->resend_request_need == 0) ){
		return 0;
	}
	return 1;
}
static int sky_tx_extension_eval_arq_rr(SkyHandle self, SkyRadioFrame* frame, uint8_t vc){
	if( !sky_tx_extension_needed_arq_rr(self, vc) ){
		return 0;
	}
	uint16_t resend_map = skyArray_get_horizon_bitmap(self->arrayBuffers[vc]);
	self->arrayBuffers[vc]->resend_request_need = 0;
	sky_packet_add_extension_arq_request(frame, self->arrayBuffers[vc]->primaryRcvRing->head_sequence, resend_map);
	//SKY_PRINTF(SKY_DIAG_DEBUG, "\tARQ resend request.\n");
	return 1;
}



static int sky_tx_extension_needed_arq_enforce(SkyHandle self, uint8_t vc){
	return self->arrayBuffers[vc]->state_enforcement_need != 0;
}
static int sky_tx_extension_eval_arq_enforce(SkyHandle self, SkyRadioFrame* frame, uint8_t vc){
	if(!sky_tx_extension_needed_arq_enforce(self, vc)){
		return 0;
	}
	self->arrayBuffers[vc]->state_enforcement_need = 0;
	uint8_t sequence = skyArray_get_next_transmitted_sequence(self->arrayBuffers[vc]);
	sky_packet_add_extension_arq_reset(frame, self->conf->vc->arq_on, sequence);
	//SKY_PRINTF(SKY_DIAG_DEBUG, "\tEnforcing ARQ.\n");
	return 1;
}



static int sky_tx_extension_needed_hmac_enforce(SkyHandle self, uint8_t vc){
	return self->hmac->vc_enfocement_need[vc] != 0;
}
static int sky_tx_extension_eval_hmac_enforce(SkyHandle self, SkyRadioFrame* frame, uint8_t vc){
	if(!sky_tx_extension_needed_hmac_enforce(self, vc)){
		return 0;
	}
	self->hmac->vc_enfocement_need[vc] = 0;
	uint16_t sequence = wrap_hmac_sequence(self->hmac->sequence_rx[vc] + 3); //+3 so that immediate sends don't invalidate what we give here. Jump constant must be bigger.
	sky_packet_add_extension_hmac_sequence_reset(frame, sequence);
	//SKY_PRINTF(SKY_DIAG_DEBUG, "\tEnforcing AUTH sequence.\n");
	return 1;
}




static int sky_tx_pick_vc(SkyHandle self, int32_t now_ms){
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		int vc = self->conf->vc_priority[i];
		if(sky_tx_extension_needed_hmac_enforce(self, vc)){
			return vc;
		}
		if(sky_tx_extension_needed_arq_enforce(self, vc)){
			return vc;
		}
		if(sky_tx_extension_needed_arq_rr(self, vc)){
			return vc;
		}
		if(skyArray_peek_next_tx_size(self->arrayBuffers[vc], 1) >= 0){
			return vc;
		}
	}
	if(self->phy->total_frames_sent_in_current_window < UTILITY_FRAMES_PER_WINDOW){
		return (now_ms & 0xFF) % SKY_NUM_VIRTUAL_CHANNELS;
	}
	return -1;
}




int sky_tx(SkyHandle self, SkyRadioFrame* frame, int insert_golay, int32_t now_ms){
	turn_to_tx(self->phy);
	int vc = sky_tx_pick_vc(self, now_ms);
	if (vc < 0)
		return vc;

	/* identity gets copied to the raw-array from frame's own identity field */
	frame->start_byte = SKYLINK_START_BYTE;
	memcpy(frame->identity, self->conf->identity, SKY_IDENTITY_LEN);
	frame->vc = (uint8_t)vc;
	frame->flags = 0;

	const SkyVCConfig* vc_conf = &self->conf->vc[vc];

	/* ARQ status. The purpose of arq_sequence number on frames without payload is to provide
	 * the peer with information where the sequencing goes. This permits asking resend for payloads
	 * that were the last in a series of transmissions. */
	frame->arq_sequence = self->arrayBuffers[vc]->primarySendRing->tx_sequence;
	if (vc_conf->arq_on)
		frame->flags |= SKY_FLAG_ARQ_ON;


	/* Add extension to the packet. ARQ */
	frame->length = EXTENSION_START_IDX;
	frame->ext_length = 0;
	sky_tx_extension_eval_arq_rr(self, frame, vc);
	sky_tx_extension_eval_arq_enforce(self, frame, vc);
	sky_tx_extension_eval_hmac_enforce(self, frame, vc);


	/* If there is enough space in frame, copy a payload to the end. Then add ARQ the sequence number obtained from ArqRing. */
	int next_pl_size = skyArray_peek_next_tx_size(self->arrayBuffers[vc], 1);
	if((next_pl_size >= 0) && (available_payload_space(frame) >= next_pl_size)){
		int arq_sequence = -1;
		int read = skyArray_read_packet_for_tx(self->arrayBuffers[vc], frame->raw + frame->length, &arq_sequence, 1);
		frame->arq_sequence = (uint8_t)arq_sequence;
		frame->flags |= SKY_FLAG_HAS_PAYLOAD;
		frame->length += read;
	}


	/* Set MAC data fields. */
	mac_set_frame_fields(self->mac, frame, now_ms);

	/* Set HMAC state and sequence */
	frame->auth_sequence = 0;
	if(vc_conf->require_authentication){
		frame->flags |= SKY_FLAG_AUTHENTICATED;
		frame->auth_sequence = sky_hmac_get_next_hmac_tx_sequence_and_advance(self, vc);
	}
	frame->auth_sequence = sky_hton16(frame->auth_sequence);


	/* Authenticate the frame */
	if (vc_conf->require_authentication){
		sky_hmac_extend_with_authentication(self, frame);
	}


	/* Apply Forward Error Correction (FEC) coding */
	sky_fec_encode(frame);


	/* Encode length field. */
	if(insert_golay){
		/* Move the data by 3 bytes to make room for the PHY header */
		for (unsigned int i = frame->length; i != 0; i--){
			frame->raw[i + 3] = frame->raw[i];
		}

		uint32_t phy_header = frame->length | SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED;
		encode_golay24(&phy_header);
		frame->raw[0] = 0xff & (phy_header >> 16);
		frame->raw[1] = 0xff & (phy_header >> 8);
		frame->raw[2] = 0xff & (phy_header >> 0);
		frame->length += 3;
	}


	++self->phy->frames_sent_in_current_window_per_vc[vc];
	++self->phy->total_frames_sent_in_current_window;
	++self->diag->tx_frames;
	return 1;
}
