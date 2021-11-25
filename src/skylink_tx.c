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



void sky_tx_track_tdd_state(SkyHandle self, int can_send){
	if(!can_send){
		for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
			self->mac->frames_sent_in_current_window_per_vc[i] = 0;
		}
		self->mac->total_frames_sent_in_current_window = 0;
	}
}



static int sky_tx_extension_needed_hmac_reset(SkyHandle self, uint8_t vc){
	return self->hmac->vc_enfocement_need[vc] != 0;
}
static int sky_tx_extension_eval_hmac_reset(SkyHandle self, SkyRadioFrame* frame, uint8_t vc){
	if(!sky_tx_extension_needed_hmac_reset(self, vc)){
		return 0;
	}
	self->hmac->vc_enfocement_need[vc] = 0;
	uint16_t sequence = wrap_hmac_sequence(self->hmac->sequence_rx[vc] + 3); //+3 so that immediate sends don't invalidate what we give here. Jump constant must be bigger.
	sky_packet_add_extension_hmac_sequence_reset(frame, sequence);
	//SKY_PRINTF(SKY_DIAG_DEBUG, "\tEnforcing AUTH sequence.\n");
	return 1;
}




static int sky_tx_pick_vc(SkyHandle self, int32_t now_ms){
	self->mac->vc_round_robin_start = (self->mac->vc_round_robin_start + 1) % SKY_NUM_VIRTUAL_CHANNELS;
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		int vc = (self->mac->vc_round_robin_start + i) % SKY_NUM_VIRTUAL_CHANNELS;
		if(sky_tx_extension_needed_hmac_reset(self, vc)){
			return vc;
		}
		if(skyArray_content_to_send(self->arrayBuffers[vc], now_ms, self->mac->frames_sent_in_current_window_per_vc[vc]) > 0){
			return vc;
		}
	}
	if(self->mac->total_frames_sent_in_current_window < UTILITY_FRAMES_PER_WINDOW){
		return 0;
	}
	return -1;
}


void sky_tx_poll_arq_timeouts(SkyHandle self, int32_t now_ms){
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		skyArray_poll_arq_state_timeout(self->arrayBuffers[i], now_ms);
	}
}



/* Returns boolean value 0/1 as to if there is need to actually send something. */
int sky_tx(SkyHandle self, SkyRadioFrame* frame, int insert_golay, int32_t now_ms){
	mac_silence_shift_check(self->mac, &self->conf->mac, now_ms);
	int can_send = mac_can_send(self->mac, now_ms);
	sky_tx_track_tdd_state(self, can_send);
	sky_tx_poll_arq_timeouts(self, now_ms);
	if(!can_send){
		return 0;
	}

	int vc = sky_tx_pick_vc(self, now_ms);
	if (vc < 0){
		return 0;  //This is supposed to return 0, NOT "-1"!!! : sky_tx returns a boolean value as to if there is need to send somethign.
	}
	const SkyVCConfig* vc_conf = &self->conf->vc[vc];


	/* identity gets copied to the raw-array from conf */
	frame->start_byte = SKYLINK_START_BYTE;
	memcpy(frame->identity, self->conf->identity, SKY_IDENTITY_LEN);
	frame->vc = (uint8_t)vc;
	frame->flags = 0;

	/* Add extension to the packet. ARQ */
	frame->length = EXTENSION_START_IDX;
	frame->ext_length = 0;


	/* Set MAC data fields. */
	mac_set_frame_fields(self->mac, frame, now_ms);


	/* ARQ status. The purpose of arq_sequence number on frames without payload is to provide
	 * the peer with information where the sequencing goes. This permits asking resend for payloads
	 * that were the last in a series of transmissions. */
	if (self->arrayBuffers[vc]->arq_state_flag == ARQ_STATE_ON){
		frame->flags |= SKY_FLAG_ARQ_ON;
	}


	sky_tx_extension_eval_hmac_reset(self, frame, vc);
	skyArray_fill_frame(self->arrayBuffers[vc], frame, now_ms, self->mac->frames_sent_in_current_window_per_vc[vc]);


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

	++self->mac->total_frames_sent_in_current_window;
	++self->mac->frames_sent_in_current_window_per_vc[vc];
	++self->diag->tx_frames;
	return 1; //Returns 1, not 0.  1 is a boolean TRUE value.
}
