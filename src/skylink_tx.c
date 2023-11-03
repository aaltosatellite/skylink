
#include "skylink/skylink.h"
#include "skylink/diag.h"
#include "skylink/conf.h"
#include "skylink/fec.h"
#include "skylink/reliable_vc.h"
#include "skylink/frame.h"
#include "skylink/mac.h"
#include "skylink/hmac.h"
#include "skylink/crc.h"
#include "skylink/utilities.h"

#include "ext/gr-satellites/golay24.h"

#include <string.h> // memset, memcpy




/* This zeroes the tracking of how many frames have been sent in current window. */
static void _sky_tx_track_tdd_state(SkyHandle self, int can_send, int content_to_send, sky_tick_t now)
{
	// We can send, but there is nothing to send.
	if (can_send && !content_to_send)
		self->mac->unused_window_time = true;

	// Window is closing.
	if (!can_send && self->mac->window_on) {

		 // Indicate need to shrink window.
		if(self->mac->unused_window_time)
			self->mac->window_adjust_counter--;

		// Indicate need to grow window.
		else
			self->mac->window_adjust_counter++;

	}

	// Window is opening.
	if (can_send && !self->mac->window_on) {

		// Indicate need to shrink window.
		if(self->mac->window_adjust_counter <= -self->conf->mac.window_adjustment_period){
			mac_shrink_window(self->mac, now);
			self->mac->window_adjust_counter = 0;
		}

		// Indicate need to grow window.
		if(self->mac->window_adjust_counter >= self->conf->mac.window_adjustment_period){
			mac_expand_window(self->mac, now);
			self->mac->window_adjust_counter = 0;
		}
	}

	// Can't send.
	if (!can_send) {

		// Update MAC state accordingly.
		self->mac->window_on = 0;
		self->mac->unused_window_time = false;
		for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
			self->mac->frames_sent_in_current_window_per_vc[i] = 0;
		}
		self->mac->total_frames_sent_in_current_window = 0;

	} else { // Can send.
		// MAC window on.
		self->mac->window_on = 1;
	}
}


static int _sky_tx_extension_eval_hmac_reset(SkyHandle self, SkyTransmitFrame *tx_frame, uint8_t vc)
{
	if (self->hmac->vc_enforcement_need[vc] != 0)
		return 0;

	self->hmac->vc_enforcement_need[vc] = 0;

	// +3 so that immediate sends don't invalidate what we give here. Jump constant must be bigger.
	uint16_t sequence = self->hmac->sequence_rx[vc] + 3; // uint16 naturally overflows
	sky_frame_add_extension_hmac_sequence_reset(tx_frame, sequence);
	return 1;
}


// Advance round robin index for virtual channels. Loops around using modulo with number of channels.
static void _sky_tx_advance_vc_round_robin(SkyHandle self)
{
	self->mac->vc_round_robin_start = (self->mac->vc_round_robin_start + 1) % SKY_NUM_VIRTUAL_CHANNELS;
}


/*
 * Run round robin logic to determine which virtual channel transmits next.
 * Returns channel index which will transmit next.
 * Negative index is returned if there's no need to transmit.
 */
static int _sky_tx_pick_vc(SkyHandle self, sky_tick_t now)
{
	// Loop through all virtual channels.
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i)
	{
		// Get the VC. Check VC's in order starting from round robin start, looping to 0 at SKY_NUM_VIRTUAL_CHANNELS.
		int vc = (self->mac->vc_round_robin_start + i) % SKY_NUM_VIRTUAL_CHANNELS;

		// Pending HMAC sequence enforcement?
		if (self->hmac->vc_enforcement_need[vc] != 0)
			return vc;

		// Something in the buffer?
		if (sky_vc_content_to_send(self->virtual_channels[vc], self->conf, now, self->mac->frames_sent_in_current_window_per_vc[vc]) > 0)
			return vc;
	}

	// This is here to ensure that the peer advances its window through TDD gap even if there are no messages to send
	if (mac_idle_frame_needed(self->mac, now))
		return 0;

	// No need to transmit.
	return -1;
}

// Generate a new frame to be sent.
int sky_tx(SkyHandle self, SkyRadioFrame* frame)
{
	// Check that the pointers are valid.
	SKY_ASSERT(self != NULL);
	SKY_ASSERT(frame != NULL);

	// Check If it is okay to transmit.
	sky_tick_t now = sky_get_tick_time();
	int can_send = mac_can_send(self->mac, now);


	// Check the virtual channels' timeout conditions.
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i)  // TODO: Do only every N ticks?
		sky_vc_check_timeouts(self->virtual_channels[i], now, self->conf->arq.timeout_ticks);

	//if (!can_send) return 0; ?????????

	// Pick a virtual channel to transmit on.
	int vc = _sky_tx_pick_vc(self, now);
	int content_to_send = (vc >= 0);
	_sky_tx_track_tdd_state(self, can_send, content_to_send, now);

	// No need to transmit.
	if (!can_send || vc < 0)
		return 0; // This is supposed to return 0, Not "-1": sky_tx returns a boolean value as to if there is need to send something.

	// Advance round robin index for virtual channels.
	_sky_tx_advance_vc_round_robin(self);
	const SkyVCConfig* vc_conf = &self->conf->vc[vc];


	/*
	 * It's okay to transmit and we have something to transmit,
	 * so let's start crafting a new frame.
	 */
	SkyTransmitFrame tx_frame;
	tx_frame.frame = frame;
	sky_frame_clear(frame);

	// Set the start byte
	const unsigned int identity_len = self->conf->identity_len;
	frame->raw[0] = SKYLINK_FRAME_VERSION_BYTE | identity_len;
	tx_frame.ptr = &frame->raw[1];
	frame->length = 1;

	// Copy source identifier
	memcpy(&frame->raw[1], self->conf->identity, identity_len);
	tx_frame.ptr += identity_len;
	frame->length += identity_len;

	// Set the static header
	SkyStaticHeader *hdr = (SkyStaticHeader*)tx_frame.ptr;
	tx_frame.hdr = hdr;
	tx_frame.ptr += sizeof(SkyStaticHeader);
	frame->length += sizeof(SkyStaticHeader);
	hdr->vc = vc;

#ifdef SKY_USE_TDD_MAC
	/* Add TDD MAC extension. */
	mac_set_frame_fields(self->mac, &tx_frame, now);
#endif

	/* Add HMAC reset extension if required. */
	_sky_tx_extension_eval_hmac_reset(self, &tx_frame, vc);

	/* Fill rest of the frame with payload data and necessary ARQ extensions. */
	int ret = sky_vc_fill_frame(self->virtual_channels[vc], self->conf, &tx_frame, now, self->mac->frames_sent_in_current_window_per_vc[vc]);

	// If there was an error, return it.
	if (ret < 0)
		return ret;
	if (ret == 0)
		SKY_PRINTF(SKY_DIAG_BUG, "Construction of new frame was started but there was nothing to transmit!");

	/* Set HMAC state and sequence */
	hdr->frame_sequence = sky_hmac_get_next_tx_sequence(self, vc);
	hdr->frame_sequence = sky_hton16(hdr->frame_sequence);

	/* Authenticate the frame. Ie. appends a hash digest to the end of the frame. */
	if (vc_conf->require_authentication & SKY_CONFIG_FLAG_AUTHENTICATE_TX)
		sky_hmac_extend_with_authentication(self, &tx_frame);

#if 0
	/* Append CRC-32 checksum to the end of frame */
	if (vc_conf->require_authentication & SKY_CONFIG_FLAG_USE_CRC32)
		sky_extend_with_crc32(frame);
#endif

	// Increment counters
	self->mac->total_frames_sent_in_current_window++;
	self->mac->frames_sent_in_current_window_per_vc[vc]++;

	// Update statistics
	self->diag->tx_bytes += frame->length;
	self->diag->vc_stats[vc].rx_frames++;
	self->diag->tx_frames++;

	return 1; // Return 1 to indicate that a new frame was created.
}

// Generate a new frame with FEC coding.
int sky_tx_with_fec(SkyHandle self, SkyRadioFrame *frame) {

	//Generate a new frame to be sent.
	int ret = sky_tx(self, frame);
	// If sky_tx created a new frame, apply FEC coding.
	if (ret == 1) {
		SKY_ASSERT(frame->length + RS_PARITYS <= sizeof(frame->raw));

		/* Apply Forward Error Correction (FEC) coding */
		sky_fec_encode(frame);
	}
	// Return whether or not the frame succesfully created.
	return ret;
}

// Generate a new frame with FEC and Golay coding.
int sky_tx_with_golay(SkyHandle self, SkyRadioFrame* frame) {

	// Generate a new frame with FEC coding.
	int ret = sky_tx_with_fec(self, frame);
	// If sky_tx_with_fec created a new frame, apply Golay coding.
	if (ret == 1) {
		SKY_ASSERT(frame->length + 3 <= sizeof(frame->raw));

		/* Move the data by 3 bytes to make room for the PHY header */
		for (unsigned int i = frame->length; i != 0; i--)
			frame->raw[i + 3] = frame->raw[i];

		/* Add PHY header */
		uint32_t phy_header = frame->length;
		encode_golay24(&phy_header);
		frame->raw[0] = 0xff & (phy_header >> 16);
		frame->raw[1] = 0xff & (phy_header >> 8);
		frame->raw[2] = 0xff & (phy_header >> 0);
		frame->length += 3;
	}

	// Return whether or not the frame succesfully created.
	return ret;
}
