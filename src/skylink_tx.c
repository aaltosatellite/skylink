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





static int _content_in_vc(SkyHandle self, int vc, tick_t now){
	int r = sky_vc_content_to_send(self->virtual_channels[vc], self->conf, now, self->mac->frames_sent_in_current_window_per_vc[vc]);
	return r;
}



/* This zeroes the tracking of how many frames have been sent in current window. */
static void _sky_tx_track_tdd_state(SkyHandle self, int can_send, int content_to_send, tick_t now){
	if(can_send && (!content_to_send)){
		self->mac->unused_window_time = true; //We can send, but there is nothing to send.
	}
	if((!can_send) && self->mac->window_on) { //window is closing.
		if(self->mac->unused_window_time){
			self->mac->window_adjust_counter--;  //indicate need to shrink window.
		}
		else{
			self->mac->window_adjust_counter++;  //indicate need to grow window.
		}
	}
	if(can_send && (!self->mac->window_on)){ //window is opening
		if(self->mac->window_adjust_counter <= -self->conf->mac.window_adjustment_period){ //need to shrink window?
			mac_shrink_window(self->mac, now);
			self->mac->window_adjust_counter = 0;
		}
		if(self->mac->window_adjust_counter >= self->conf->mac.window_adjustment_period){ //need to grow window?
			mac_expand_window(self->mac, now);
			self->mac->window_adjust_counter = 0;
		}
	}
	if(!can_send){
		self->mac->window_on = 0;
		self->mac->unused_window_time = false;
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
	return (self->hmac->vc_enforcement_need[vc] != 0) && (self->conf->vc[vc].require_authentication);
}


static int _sky_tx_extension_eval_hmac_reset(SkyHandle self, SkyRadioFrame* frame, uint8_t vc){
	if(!_sky_tx_extension_needed_hmac_reset(self, vc)){
		return 0;
	}
	self->hmac->vc_enforcement_need[vc] = 0;
	uint16_t sequence = wrap_hmac_sequence(self->hmac->sequence_rx[vc] + 3); //+3 so that immediate sends don't invalidate what we give here. Jump constant must be bigger.
	sky_packet_add_extension_hmac_sequence_reset(frame, sequence);
	return 1;
}


static void _sky_tx_advance_vc_round_robin(SkyHandle self){
	self->mac->vc_round_robin_start = (self->mac->vc_round_robin_start + 1) % SKY_NUM_VIRTUAL_CHANNELS;
}


static int _sky_tx_pick_vc(SkyHandle self, tick_t now){
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		int vc = (self->mac->vc_round_robin_start + i) % SKY_NUM_VIRTUAL_CHANNELS;
		if(_sky_tx_extension_needed_hmac_reset(self, vc)){
			return vc;
		}
		if(_content_in_vc(self, vc, now) > 0){
			return vc;
		}
	}
	// This is here to ensure that the peer advances its window through TDD gap even if there are no messages to send
	if(mac_idle_frame_needed(self->mac, now)){
		return 0;
	}
	return -1;
}


static void _sky_tx_poll_arq_timeouts(SkyHandle self, tick_t now){
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		sky_vc_poll_arq_state_timeout(self->virtual_channels[i], now, self->conf->arq_timeout_ticks);
	}
}


int sky_tx(SkyHandle self, SkyRadioFrame* frame) {

	SKY_ASSERT(frame != NULL);

	tick_t now = sky_get_tick_time();
	int can_send = mac_can_send(self->mac, now);
	_sky_tx_poll_arq_timeouts(self, now);
	int vc = _sky_tx_pick_vc(self, now);
	int content_to_send = (vc >= 0);
	_sky_tx_track_tdd_state(self, can_send, content_to_send, now);

	if ((!can_send) || (vc < 0)){
		return 0;  //This is supposed to return 0, Not "-1": sky_tx returns a boolean value as to if there is need to send somethign.
	}

	_sky_tx_advance_vc_round_robin(self);

	const SkyVCConfig* vc_conf = &self->conf->vc[vc];

	/* Identity gets copied to the raw-vc from conf, and initialize other fields. */
	sky_frame_clear(frame);
	frame->start_byte = SKYLINK_START_BYTE;
	memcpy(frame->identity, self->conf->identity, SKY_IDENTITY_LEN);
	frame->vc = (uint8_t)vc;
	frame->flags = 0;
	frame->length = EXTENSION_START_IDX;
	frame->ext_length = 0;


	/* Add TDD extension. */
	mac_set_frame_fields(self->mac, frame, now);


	/* ARQ status. */
	if (self->virtual_channels[vc]->arq_state_flag == ARQ_STATE_ON){
		frame->flags |= SKY_FLAG_ARQ_ON;
	}


	/* HMAC/AUTH extension addition is evaluated separately from payload mechanics. */
	_sky_tx_extension_eval_hmac_reset(self, frame, vc);


	/* Add necessary extensions and a payload if one is in the ring buffer. This is a rather involved function. */
	sky_vc_fill_frame(self->virtual_channels[vc], self->conf, frame, now,
					  self->mac->frames_sent_in_current_window_per_vc[vc]);


	/* Set HMAC state and sequence */
	frame->auth_sequence = sky_hmac_get_next_tx_sequence(self, vc);
	frame->auth_sequence = sky_hton16(frame->auth_sequence);

	/* Authenticate the frame. Ie. appends a hash digest to the end of the frame. */
	if (vc_conf->require_authentication & SKY_VC_FLAG_AUTHENTICATE_TX){
		sky_hmac_extend_with_authentication(self, frame);
	}


	self->diag->tx_bytes += frame->length;

	++self->mac->total_frames_sent_in_current_window;
	++self->mac->frames_sent_in_current_window_per_vc[vc];
	++self->diag->tx_frames;

	return 1; // Return 1 to indicate that a new frame was created.
}


int sky_tx_with_fec(SkyHandle self, SkyRadioFrame *frame) {

	int ret = sky_tx(self, frame);
	if (ret == 1) {
		SKY_ASSERT(frame->length + RS_PARITYS <= sizeof(frame->raw));

		/* Apply Forward Error Correction (FEC) coding */
		sky_fec_encode(frame);
	}
	return ret;
}


int sky_tx_with_golay(SkyHandle self, SkyRadioFrame* frame) {
	
	int ret = sky_tx_with_fec(self, frame);
	if (ret == 1) {
		SKY_ASSERT(frame->length + 3 <= sizeof(frame->raw));

		/* Move the data by 3 bytes to make room for the PHY header */
		for (unsigned int i = frame->length; i != 0; i--)
			frame->raw[i + 3] = frame->raw[i];
		
		uint32_t phy_header = frame->length | SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED;
		encode_golay24(&phy_header);
		frame->raw[0] = 0xff & (phy_header >> 16);
		frame->raw[1] = 0xff & (phy_header >> 8);
		frame->raw[2] = 0xff & (phy_header >> 0);
		frame->length += 3;
	}

	return ret;
}
