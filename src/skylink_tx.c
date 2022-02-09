//
// Created by elmore on 29.10.2021.
//

#include "skylink/skylink.h"
#include "skylink/conf.h"
#include "skylink/fec.h"
#include "skylink/reliable_vc.h"
#include "skylink/frame.h"
#include "skylink/mac.h"
#include "skylink/hmac.h"
#include "skylink/utilities.h"


#define TDD_ADJUST_FREQ 		2



static int _content_in_vc(SkyHandle self, int vc, int32_t now_ms){
	int r = sky_vc_content_to_send(self->virtual_channels[vc], self->conf, now_ms, self->mac->frames_sent_in_current_window_per_vc[vc]);
	return r;
}



/* This zeroes the tracking of how many frames have been sent in current window. */
static void _sky_tx_track_tdd_state(SkyHandle self, int can_send, int content_to_send){
	if((!can_send) && self->mac->window_on) { //window is closing.
		if(content_to_send){
			self->mac->window_adjust_plan += 1;  //indicate need to grow window.
		}
		if(!content_to_send){
			self->mac->window_adjust_plan -= 1;  //indicate need to grow window.
		}
	}
	if(can_send && (!self->mac->window_on)){ //window is opening
		if(self->mac->window_adjust_plan <= -TDD_ADJUST_FREQ){ //need to shrink window?
			mac_shrink_window(self->mac, &self->conf->mac);
			self->mac->window_adjust_plan = 0; //reset indications.
		}
		if(self->mac->window_adjust_plan >= TDD_ADJUST_FREQ){ //need to grow window?
			mac_expand_window(self->mac, &self->conf->mac);
			self->mac->window_adjust_plan = 0; //reset indications.
		}
	}
	if(!can_send){
		self->mac->window_on = 0;
		for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
			self->mac->frames_sent_in_current_window_per_vc[i] = 0;
		}
		self->mac->total_frames_sent_in_current_window = 0;
	} else {
		self->mac->window_on = 1;
	}
}



/* Returns boolean 0/1 wether virtual channel necessitates reset of HMAC authentication sequence. */
static int _sky_tx_extension_needed_hmac_reset(SkyHandle self, uint8_t vc){
	return (self->hmac->vc_enfocement_need[vc] != 0) && (self->conf->vc[vc].require_authentication);
}
static int _sky_tx_extension_eval_hmac_reset(SkyHandle self, SkyRadioFrame* frame, uint8_t vc){
	if(!_sky_tx_extension_needed_hmac_reset(self, vc)){
		return 0;
	}
	self->hmac->vc_enfocement_need[vc] = 0;
	uint16_t sequence = wrap_hmac_sequence(self->hmac->sequence_rx[vc] + 3); //+3 so that immediate sends don't invalidate what we give here. Jump constant must be bigger.
	sky_packet_add_extension_hmac_sequence_reset(frame, sequence);
	//SKY_PRINTF(SKY_DIAG_DEBUG, "\tEnforcing AUTH sequence.\n");
	return 1;
}




static void _sky_tx_advance_vc_round_robin(SkyHandle self){
	self->mac->vc_round_robin_start = (self->mac->vc_round_robin_start + 1) % SKY_NUM_VIRTUAL_CHANNELS;
}
static int _sky_tx_pick_vc(SkyHandle self, int32_t now_ms){
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		int vc = (self->mac->vc_round_robin_start + i) % SKY_NUM_VIRTUAL_CHANNELS;
		if(_sky_tx_extension_needed_hmac_reset(self, vc)){
			return vc;
		}
		if(_content_in_vc(self, vc, now_ms) > 0){
			return vc;
		}
	}
	// This was here to ensure that the peer advances its window through TDD gap even if there are no messages to send
	//if(self->mac->total_frames_sent_in_current_window < UTILITY_FRAMES_PER_WINDOW){
	//	return 0;
	//}
	return -1;
}


static void _sky_tx_poll_arq_timeouts(SkyHandle self, int32_t now_ms, int32_t timeout_ms){
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		sky_vc_poll_arq_state_timeout(self->virtual_channels[i], now_ms, timeout_ms);
	}
}



/* Returns boolean value 0/1 as to if there is need to actually send something. */
int sky_tx(SkyHandle self, SkyRadioFrame* frame, int insert_golay){
	timestamp_t now_ms = sky_get_tick_time();
	mac_silence_shift_check(self->mac, &self->conf->mac, now_ms);
	int can_send = mac_can_send(self->mac, now_ms);
	_sky_tx_poll_arq_timeouts(self, now_ms, self->conf->arq_timeout_ms);
	int vc = _sky_tx_pick_vc(self, now_ms);
	int content_to_send = (vc >= 0);
	_sky_tx_track_tdd_state(self, can_send, content_to_send);

	if ((!can_send) || (vc < 0)){
		return 0;  //This is supposed to return 0, Not "-1": sky_tx returns a boolean value as to if there is need to send somethign.
	}

	_sky_tx_advance_vc_round_robin(self);

	const SkyVCConfig* vc_conf = &self->conf->vc[vc];

	/* Identity gets copied to the raw-vc from conf, and initialize other fields. */
	frame->start_byte = SKYLINK_START_BYTE;
	memcpy(frame->identity, self->conf->identity, SKY_IDENTITY_LEN);
	frame->vc = (uint8_t)vc;
	frame->flags = 0;
	frame->length = EXTENSION_START_IDX;
	frame->ext_length = 0;


	/* Add TDD extension. */
	mac_set_frame_fields(self->mac, frame, now_ms);


	/* ARQ status. */
	if (self->virtual_channels[vc]->arq_state_flag == ARQ_STATE_ON){
		frame->flags |= SKY_FLAG_ARQ_ON;
	}


	/* HMAC/AUTH extension addition is evaluated separately from payload mechanics. */
	_sky_tx_extension_eval_hmac_reset(self, frame, vc);


	/* Add necessary extensions and a payload if one is in the ring buffer. This is a rather involved function. */
	sky_vc_fill_frame(self->virtual_channels[vc], self->conf, frame, now_ms,
					  self->mac->frames_sent_in_current_window_per_vc[vc]);


	/* Set HMAC state and sequence */
	frame->auth_sequence = sky_hmac_get_next_hmac_tx_sequence_and_advance(self, vc);
	frame->auth_sequence = sky_hton16(frame->auth_sequence);

	/* Authenticate the frame. Ie. appends a hash digest to the end of the frame. */
	if (vc_conf->require_authentication & SKY_VC_FLAG_AUTHENTICATE_TX){
		frame->flags |= SKY_FLAG_AUTHENTICATED;
		sky_hmac_extend_with_authentication(self, frame);
	}



	/* Apply Forward Error Correction (FEC) coding */
	sky_fec_encode(frame);

	self->diag->tx_bytes += frame->length;

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
