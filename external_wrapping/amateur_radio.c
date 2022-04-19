//
// Created by elmore on 14.4.2022.
//

#include "amateur_radio.h"


int skylink_encode_amateur_pl(uint8_t* identity, uint8_t* pl, int32_t pl_len, uint8_t* tgt, int insert_golay){
	if(pl_len > SKY_MAX_PAYLOAD_LEN){
		return -1;
	}

	SkyRadioFrame frame_actual;
	SkyRadioFrame* frame = &frame_actual;
	sky_frame_clear(frame);
	frame->start_byte = SKYLINK_START_BYTE;
	memcpy(frame->identity, identity, SKY_IDENTITY_LEN);
	frame->vc = 3;
	frame->flags = 0;
	frame->length = EXTENSION_START_IDX;
	frame->ext_length = 0;


	/* Add TDD extension. */
	sky_packet_add_extension_mac_tdd_control(frame, 250, 250);


	/* Add necessary extensions and a payload if one is in the ring buffer. This is a rather involved function. */
	memcpy(frame->raw + frame->length, pl, pl_len);
	frame->length += pl_len;
	frame->flags |= SKY_FLAG_HAS_PAYLOAD;

	/* Set HMAC state and sequence */
	frame->auth_sequence = 0;

	/* Apply Forward Error Correction (FEC) coding */
	sky_fec_encode(frame);


	/* Encode length field. Golay. */
	if(insert_golay) {
		/* Move the data by 3 bytes to make room for the PHY header */
		for (unsigned int i = frame->length; i != 0; i--) {
			frame->raw[i + 3] = frame->raw[i];
		}
		uint32_t phy_header = frame->length | SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED;
		encode_golay24(&phy_header);
		frame->raw[0] = 0xff & (phy_header >> 16);
		frame->raw[1] = 0xff & (phy_header >> 8);
		frame->raw[2] = 0xff & (phy_header >> 0);
		frame->length += 3;
	}

	memcpy(tgt, frame->raw, frame->length);
	return frame->length; //Returns 1, not 0.  1 is a boolean TRUE value.
}


