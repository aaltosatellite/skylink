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

int sky_frame_add_extension_arq_sequence(SkyRadioFrame* frame, sky_arq_sequence_t sequence)
{
	// SKY_ASSERT(hdr->identity_length + sizeof(SkyStaticHeader) + hdr->extension_length == frame->length);
	SKY_ASSERT(frame->length < SKY_PAYLOAD_MAX_LEN - sizeof(ExtARQSeq));
	SkyHeaderExtension *extension = (SkyHeaderExtension *)(&frame->raw[frame->length]);

	extension->type = EXTENSION_ARQ_SEQUENCE;
	extension->length = sizeof(ExtARQSeq);
	extension->ARQSeq.sequence = sky_hton16(sequence);

	//frame->extension_length += sizeof(ExtARQSeq);
	frame->length += sizeof(ExtARQSeq);
	return SKY_RET_OK;
}


int sky_frame_add_extension_arq_request(SkyRadioFrame* frame, sky_arq_sequence_t sequence, uint16_t mask)
{
	// SKY_ASSERT(hdr->identity_length + sizeof(SkyStaticHeader) + hdr->extension_length == frame->length);
	SKY_ASSERT(frame->length < SKY_PAYLOAD_MAX_LEN - sizeof(ExtARQReq));
	SkyHeaderExtension *extension = (SkyHeaderExtension *)(&frame->raw[frame->length]);

	extension->type = EXTENSION_ARQ_REQUEST;
	extension->length = sizeof(ExtARQReq);
	extension->ARQReq.sequence = sky_hton16(sequence);
	extension->ARQReq.mask = sky_hton16(mask);

	//frame->extension_length += sizeof(ExtARQReq);
	frame->length += sizeof(ExtARQReq);
	return SKY_RET_OK;
}


int sky_frame_add_extension_arq_ctrl(SkyRadioFrame* frame, sky_arq_sequence_t tx_head_sequence, sky_arq_sequence_t rx_head_sequence)
{
	// SKY_ASSERT(hdr->identity_length + sizeof(SkyStaticHeader) + hdr->extension_length == frame->length);
	SKY_ASSERT(frame->length < SKY_PAYLOAD_MAX_LEN - sizeof(ExtARQCtrl));
	SkyHeaderExtension *extension = (SkyHeaderExtension *)(&frame->raw[frame->length]);

	extension->type = EXTENSION_ARQ_CTRL;
	extension->length = sizeof(ExtARQCtrl);
	extension->ARQCtrl.tx_sequence = sky_hton16(tx_head_sequence);
	extension->ARQCtrl.rx_sequence = sky_hton16(rx_head_sequence);

	//frame->extension_length += sizeof(ExtARQCtrl);
	frame->length += sizeof(ExtARQCtrl);
	return SKY_RET_OK;
}


int sky_frame_add_extension_arq_handshake(SkyRadioFrame* frame, uint8_t state_flag, uint32_t identifier)
{
	SKY_ASSERT(frame->length < SKY_PAYLOAD_MAX_LEN - sizeof(ExtARQHandshake));
	SkyHeaderExtension *extension = (SkyHeaderExtension *)(&frame->raw[frame->length]);
	// SkyHeaderExtension* extension = (SkyHeaderExtension*)(frame->raw + EXTENSION_START_IDX + frame->extension_length);
	extension->type = EXTENSION_ARQ_HANDSHAKE;
	extension->length = sizeof(ExtARQHandshake);
	extension->ARQHandshake.peer_state = state_flag;
	extension->ARQHandshake.identifier = sky_hton32(identifier);

	//frame->extension_length += sizeof(ExtARQHandshake);
	frame->length += sizeof(ExtARQHandshake);
	return SKY_RET_OK;
}


int sky_frame_add_extension_mac_tdd_control(SkyRadioFrame* frame, uint16_t window, uint16_t remaining)
{
	SKY_ASSERT(frame->length < SKY_PAYLOAD_MAX_LEN - sizeof(ExtTDDControl));
	SkyHeaderExtension *extension = (SkyHeaderExtension *)(&frame->raw[frame->length]);
	// SkyHeaderExtension* extension = (SkyHeaderExtension*)(frame->raw + EXTENSION_START_IDX + frame->extension_length);
	extension->type = EXTENSION_MAC_TDD_CONTROL;
	extension->length = sizeof(ExtTDDControl);
	extension->TDDControl.window = sky_hton16(window);
	extension->TDDControl.remaining = sky_hton16(remaining);

	//frame->extension_length += sizeof(ExtTDDControl);
	frame->length += sizeof(ExtTDDControl);
	return SKY_RET_OK;
}


int sky_frame_add_extension_hmac_sequence_reset(SkyRadioFrame* frame, uint16_t sequence)
{
	SKY_ASSERT(frame->length < SKY_PAYLOAD_MAX_LEN - sizeof(ExtHMACSequenceReset));
	SkyHeaderExtension *extension = (SkyHeaderExtension *)(&frame->raw[frame->length]);
	// SkyHeaderExtension* extension = (SkyHeaderExtension*)(frame->raw + EXTENSION_START_IDX + frame->extension_length);
	extension->type = EXTENSION_HMAC_SEQUENCE_RESET;
	extension->length = sizeof(ExtHMACSequenceReset);
	extension->HMACSequenceReset.sequence = sky_hton16(sequence);

	//frame->extension_length += sizeof(ExtHMACSequenceReset);
	frame->length += sizeof(ExtHMACSequenceReset);
	return SKY_RET_OK;
}

int sky_frame_get_space_left(const SkyRadioFrame *frame)
{
	return RS_MSGLEN - (frame->length + SKY_HMAC_LENGTH);
}

int sky_frame_extend_with_payload(SkyRadioFrame* frame, const uint8_t *payload, unsigned int payload_length)
{
	if (sky_frame_get_space_left(frame) < (int)payload_length)
		return SKY_RET_NO_SPACE_FOR_PAYLOAD;

	memcpy(&frame->raw[frame->length], payload, payload_length);
	frame->length += payload_length;
	return 0;
}

//=== ENCODING =========================================================================================================
//======================================================================================================================



int sky_frame_parse_extension_headers(const SkyRadioFrame* frame, SkyParsedFrame* parsed) {

	// Validate field lenght
	const unsigned int extension_length = parsed->hdr.extension_length;
	if (extension_length < 2)
		return SKY_RET_INVALID_EXT_LENGTH;

	unsigned int cursor = 1 + (frame->raw[0] & 0x7) + sizeof(SkyStaticHeader);
	if (cursor > frame->length)
		return SKY_RET_INVALID_EXT_LENGTH;

	// Iterate all extension headers
	while (cursor < extension_length) {

		// Cast a pointer to cursor position
		SkyHeaderExtension* ext = (SkyHeaderExtension*)&frame->raw[cursor];

		// Check possible cursor overflow
		if (cursor + ext->length >= frame->length)
			return SKY_RET_INVALID_EXT_LENGTH;
		if (ext->length == 0)
			return SKY_RET_INVALID_EXT_LENGTH;

		// Validate header field lenght and store the pointer to the results structr.
		switch (ext->type)
		{
		case EXTENSION_ARQ_SEQUENCE:
			if (ext->length != sizeof(ExtARQSeq))
				return SKY_RET_INVALID_EXT_LENGTH;
			parsed->arq_sequence = ext;
			break;

		case EXTENSION_ARQ_REQUEST:
			if (ext->length != sizeof(ExtARQReq))
				return SKY_RET_INVALID_EXT_LENGTH;
			parsed->arq_request = ext;
			break;

		case EXTENSION_ARQ_CTRL:
			if (ext->length != sizeof(ExtARQCtrl))
				return SKY_RET_INVALID_EXT_LENGTH;
			parsed->arq_ctrl = ext;
			break;

		case EXTENSION_ARQ_HANDSHAKE:
			if (ext->length != sizeof(ExtARQHandshake))
				return SKY_RET_INVALID_EXT_LENGTH;
			parsed->arq_handshake = ext;
			break;

		case EXTENSION_MAC_TDD_CONTROL:
			if (ext->length != sizeof(ExtTDDControl))
				return SKY_RET_INVALID_EXT_LENGTH;
			parsed->mac_tdd = ext;
			break;

		case EXTENSION_HMAC_SEQUENCE_RESET:
			if (ext->length != sizeof(ExtHMACSequenceReset))
				return SKY_RET_INVALID_EXT_LENGTH;
			parsed->hmac_reset = ext;
			break;

		default:
			return SKY_RET_INVALID_EXT_TYPE;
		}

		cursor += ext->length;
	}

	return SKY_RET_OK;
}
