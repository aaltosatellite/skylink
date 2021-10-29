//
// Created by elmore on 29.10.2021.
//

#include "skylink_tx.h"




int sky_tx(SkyHandle self, SkyRadioFrame *frame, uint8_t vc)
{
	/* identity gets copied to the raw-array from frame's own identity field */
	memcpy(frame->identity, self->conf->identity, SKY_IDENTITY_LEN);

	frame->vc = vc;

	/* Set HMAC state and sequence */
	frame->hmac_sequence = 0;
	if(self->conf->vc[vc].require_authentication){
		frame->hmac_on = 1;
		frame->hmac_sequence = sky_hmac_get_next_hmac_tx_sequence_and_advance(self, vc);
	}

	/* ARQ status. This is kind of dumb thing to do here, since we don't yet know if there will be a payload. */
	frame->arq_on = self->conf->vc[vc].arq_on;
	frame->arq_sequence = ARQ_SEQUENCE_NAN;

	//todo: set extension headers.

	/* Set MAC data fields. Could in principle be moved after the payload and ecoding phases, but would constitute a stamp procedure. */
	int32_t now_ms = get_time_ms();
	mac_set_frame_fields(self->mac, frame, now_ms);

	/* Encode the above information, and the extensions. After this step, we know the remaining space in frame */
	encode_skylink_packet(frame);

	/* If there is enough space in frame, copy a payload to the end. Then add ARQ the sequence number obtained from ArqRing. */
	int next_pl_size = skyArray_peek_next_tx_size(self->arrayBuffers[vc]);
	if((next_pl_size >= 0) && (sky_packet_available_payload_space(frame) >= next_pl_size)){
		int arq_sequence = -1;
		int r = skyArray_read_packet_for_tx(self->arrayBuffers[vc],  frame->raw + frame->length, &arq_sequence);
		if((r >= 0) && (self->conf->vc[vc].arq_on)){
			sky_packet_stamp_arq(frame, (uint8_t)arq_sequence);
		}
	}

	/* Authenticate the frame */
	if(self->conf->vc[vc].require_authentication){
		sky_hmac_extend_with_authentication(self, frame);
	}

	/* Apply Forward Error Correction (FEC) coding */
	sky_fec_encode(frame);

	/* Move the data by 3 bytes to make room for the PHY header */
	for (unsigned int i = frame->length; i != 0; i--){
		frame->raw[i + 3] = frame->raw[i];
	}

	/* Encode length field. */
	uint32_t phy_header = frame->length | SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED;
	encode_golay24(&phy_header);
	frame->raw[0] = 0xff & (phy_header >> 16);
	frame->raw[1] = 0xff & (phy_header >> 8);
	frame->raw[2] = 0xff & (phy_header >> 0);

	frame->length += 3; //todo: necessary?

	++self->diag->tx_frames;

	return 0;
}













