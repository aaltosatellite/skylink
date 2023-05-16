#include "skylink/frame.h"
#include "skylink/fec.h"
#include "skylink/hmac.h"
#include "skylink/utilities.h"

#include "sky_platform.h"

#include <string.h> // memset, memcpy

SkyRadioFrame* sky_frame_create() {
	SkyRadioFrame* frame = SKY_MALLOC(sizeof(SkyRadioFrame));
	memset(frame, 0, sizeof(SkyRadioFrame));
	return frame;
}

void sky_frame_destroy(SkyRadioFrame* frame) {
	SKY_FREE(frame);
}

void sky_frame_clear(SkyRadioFrame* frame) {
	memset(frame, 0, sizeof(SkyRadioFrame));
}



//=== ENCODING =========================================================================================================
//======================================================================================================================

int sky_frame_add_extension_arq_sequence(SkyRadioFrame* frame, sky_arq_sequence_t sequence) {
	SkyHeaderExtension* extension = (SkyHeaderExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_ARQ_SEQUENCE;
	extension->length = sizeof(ExtARQSeq) +1;
	extension->ARQSeq.sequence = sky_hton16(sequence);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_frame_add_extension_arq_request(SkyRadioFrame* frame, sky_arq_sequence_t sequence, uint16_t mask) {
	SkyHeaderExtension* extension = (SkyHeaderExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_ARQ_REQUEST;
	extension->length = sizeof(ExtARQReq) +1;
	extension->ARQReq.sequence = sky_hton16(sequence);
	extension->ARQReq.mask = sky_hton16(mask);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_frame_add_extension_arq_ctrl(SkyRadioFrame* frame, sky_arq_sequence_t tx_head_sequence, sky_arq_sequence_t rx_head_sequence){
	SkyHeaderExtension* extension = (SkyHeaderExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_ARQ_CTRL;
	extension->length = sizeof(ExtARQCtrl) +1;
	extension->ARQCtrl.tx_sequence = sky_hton16(tx_head_sequence);
	extension->ARQCtrl.rx_sequence = sky_hton16(rx_head_sequence);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_frame_add_extension_arq_handshake(SkyRadioFrame* frame, uint8_t state_flag, uint32_t identifier){
	SkyHeaderExtension* extension = (SkyHeaderExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_ARQ_HANDSHAKE;
	extension->length = sizeof(ExtARQHandshake) +1;
	extension->ARQHandshake.peer_state = state_flag;
	extension->ARQHandshake.identifier = sky_hton32(identifier);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_frame_add_extension_mac_tdd_control(SkyRadioFrame* frame, uint16_t window, uint16_t remaining) {
	SkyHeaderExtension* extension = (SkyHeaderExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_MAC_TDD_CONTROL;
	extension->length = sizeof(ExtTDDControl) +1;
	extension->TDDControl.window = sky_hton16(window);
	extension->TDDControl.remaining = sky_hton16(remaining);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}


int sky_frame_add_extension_hmac_sequence_reset(SkyRadioFrame* frame, uint16_t sequence) {
	SkyHeaderExtension* extension = (SkyHeaderExtension*)(frame->raw + EXTENSION_START_IDX + frame->ext_length);
	extension->type = EXTENSION_HMAC_SEQUENCE_RESET;
	extension->length = sizeof(ExtHMACSequenceReset) +1;
	extension->HMACSequenceReset.sequence = sky_hton16(sequence);

	frame->ext_length += extension->length;
	frame->length = EXTENSION_START_IDX + frame->ext_length;
	return SKY_RET_OK;
}

int sky_frame_get_space_left(const SkyRadioFrame *frame)
{
	return RS_MSGLEN - (frame->length + SKY_HMAC_LENGTH);
}

int sky_frame_extend_with_payload(SkyRadioFrame* frame, void* pl, unsigned int length)
{
	if (sky_frame_get_space_left(frame) < (int)length)
		return SKY_RET_NO_SPACE_FOR_PAYLOAD;
	
	memcpy(frame->raw + EXTENSION_START_IDX + frame->ext_length, pl, length);
	frame->length = EXTENSION_START_IDX + frame->ext_length + length;
	return 0;
}

//=== ENCODING =========================================================================================================
//======================================================================================================================



int sky_frame_parse_extension_headers(const SkyRadioFrame* frame, SkyParsedExtensions* exts) {

	memset(exts, 0, sizeof(SkyParsedExtensions));

	// Validate field
	if ((unsigned int)frame->ext_length < 2)
		return SKY_RET_INVALID_EXT_LENGTH;
	if ((unsigned int)frame->ext_length + EXTENSION_START_IDX > frame->length)
		return SKY_RET_INVALID_EXT_LENGTH;

	// Iterate all extension headers 
	unsigned int cursor = 0;
	while (cursor < frame->ext_length) {

		// Cast a pointer to cursor position
		SkyHeaderExtension* ext = (SkyHeaderExtension*)&frame->raw[EXTENSION_START_IDX + cursor];		

		// Check possible cursor overflow
		if (cursor + ext->length >= frame->length)
			return SKY_RET_INVALID_EXT_LENGTH;
		if(ext->length == 0)
			return SKY_RET_INVALID_EXT_LENGTH;
		
		// Validate header field lenght and store the pointer to the results structr.
		switch (ext->type)
		{
		case EXTENSION_ARQ_SEQUENCE:
			if (ext->length != sizeof(ExtARQSeq) + 1)
				return SKY_RET_INVALID_EXT_LENGTH;
			exts->arq_sequence = ext;
			break;

		case EXTENSION_ARQ_REQUEST:
			if (ext->length != sizeof(ExtARQReq) + 1)
				return SKY_RET_INVALID_EXT_LENGTH;
			exts->arq_request = ext;
			break;

		case EXTENSION_ARQ_CTRL:
			if (ext->length != sizeof(ExtARQCtrl) + 1)
				return SKY_RET_INVALID_EXT_LENGTH;
			exts->arq_ctrl = ext;
			break;

		case EXTENSION_ARQ_HANDSHAKE:
			if (ext->length != sizeof(ExtARQHandshake) + 1)
				return SKY_RET_INVALID_EXT_LENGTH;
			exts->arq_handshake = ext;
			break;

		case EXTENSION_MAC_TDD_CONTROL:
			if (ext->length != sizeof(ExtTDDControl) + 1)
				return SKY_RET_INVALID_EXT_LENGTH;
			exts->mac_tdd = ext;
			break;

		case EXTENSION_HMAC_SEQUENCE_RESET:
			if (ext->length != sizeof(ExtHMACSequenceReset) + 1)
				return SKY_RET_INVALID_EXT_LENGTH;
			exts->hmac_reset = ext;
			break;

		default:
			return SKY_RET_INVALID_EXT_TYPE;
		}

		cursor += ext->length;
	}

	return SKY_RET_OK;
}
