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

int sky_frame_add_extension_arq_sequence(SkyTransmitFrame *tx_frame, sky_arq_sequence_t sequence)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length + 1 + sizeof(ExtARQSeq) < SKY_PAYLOAD_MAX_LEN);

	SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame->ptr;
	extension->type = EXTENSION_ARQ_SEQUENCE;
	extension->length = sizeof(ExtARQSeq);
	extension->ARQSeq.sequence = sky_hton16(sequence);

	const unsigned int len = 1 + sizeof(ExtARQSeq);
	tx_frame->hdr->extension_length += len;
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

int sky_frame_add_extension_arq_request(SkyTransmitFrame *tx_frame, sky_arq_sequence_t sequence, uint16_t mask)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length + 1 + sizeof(ExtARQReq) < SKY_PAYLOAD_MAX_LEN);

	SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame->ptr;
	extension->type = EXTENSION_ARQ_REQUEST;
	extension->length = sizeof(ExtARQReq);
	extension->ARQReq.sequence = sky_hton16(sequence);
	extension->ARQReq.mask = sky_hton16(mask);

	const unsigned int len = 1 + sizeof(ExtARQReq);
	tx_frame->hdr->extension_length += len;
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

int sky_frame_add_extension_arq_ctrl(SkyTransmitFrame *tx_frame, sky_arq_sequence_t tx_sequence, sky_arq_sequence_t rx_sequence)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length + 1 + sizeof(ExtARQCtrl) < SKY_PAYLOAD_MAX_LEN);

	SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame->ptr;
	extension->type = EXTENSION_ARQ_CTRL;
	extension->length = sizeof(ExtARQCtrl);
#if 0
	extension->ARQCtrl.tx_sequence = sky_hton16(tx_sequence);
	extension->ARQCtrl.rx_sequence = sky_hton16(rx_sequence);
#else
	extension->ARQCtrl.tx_sequence = tx_sequence;
	extension->ARQCtrl.rx_sequence = rx_sequence;
#endif

	const unsigned int len = 1 + sizeof(ExtARQCtrl);
	tx_frame->hdr->extension_length += len;
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

int sky_frame_add_extension_arq_handshake(SkyTransmitFrame *tx_frame, uint8_t state_flag, uint32_t identifier)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length + 1 + sizeof(ExtARQHandshake) < SKY_PAYLOAD_MAX_LEN);

	SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame->ptr;
	extension->type = EXTENSION_ARQ_HANDSHAKE;
	extension->length = sizeof(ExtARQHandshake);
	extension->ARQHandshake.peer_state = state_flag;
	extension->ARQHandshake.identifier = sky_hton32(identifier);

	const unsigned int len = 1 + sizeof(ExtARQHandshake);
	tx_frame->hdr->extension_length += len;
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

int sky_frame_add_extension_mac_tdd_control(SkyTransmitFrame *tx_frame, uint16_t window, uint16_t remaining)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length < SKY_PAYLOAD_MAX_LEN - sizeof(ExtTDDControl));

	SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame->ptr;
	extension->type = EXTENSION_MAC_TDD_CONTROL;
	extension->length = sizeof(ExtTDDControl);
	extension->TDDControl.window = sky_hton16(window);
	extension->TDDControl.remaining = sky_hton16(remaining);

	const unsigned int len = 1 + sizeof(ExtTDDControl);
	tx_frame->hdr->extension_length += len;
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

int sky_frame_add_extension_hmac_sequence_reset(SkyTransmitFrame *tx_frame, uint16_t sequence)
{
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length < SKY_PAYLOAD_MAX_LEN - sizeof(ExtHMACSequenceReset));

	SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame->ptr;
	extension->type = EXTENSION_HMAC_SEQUENCE_RESET;
	extension->length = sizeof(ExtHMACSequenceReset);
	extension->HMACSequenceReset.sequence = sky_hton16(sequence);

	const unsigned int len = 1 + sizeof(ExtTDDControl);
	tx_frame->hdr->extension_length += 1 + sizeof(ExtHMACSequenceReset);
	tx_frame->frame->length += 1 + sizeof(ExtHMACSequenceReset);
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

int sky_frame_get_space_left(const SkyRadioFrame *frame)
{
	return RS_MSGLEN - (frame->length + SKY_HMAC_LENGTH);
}

int sky_frame_extend_with_payload(SkyTransmitFrame *tx_frame, const uint8_t *payload, unsigned int payload_length)
{
	// TODO: Unused function
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	if (sky_frame_get_space_left(tx_frame->frame) < (int)payload_length)
		return SKY_RET_NO_SPACE_FOR_PAYLOAD;

	memcpy(tx_frame->ptr, payload, payload_length);
	tx_frame->ptr += payload_length;
	tx_frame->frame->length += payload_length;
	tx_frame->hdr->flag_has_payload = 1;
	return 0;
}

//=== ENCODING =========================================================================================================
//======================================================================================================================



int sky_frame_parse_extension_headers(const SkyRadioFrame* frame, SkyParsedFrame* parsed) {

	// Validate field length
	const unsigned int extension_length = parsed->hdr.extension_length;
	if (extension_length < 2)
		return SKY_RET_INVALID_EXT_LENGTH;

	unsigned int cursor = 1 + (frame->raw[0] & SKYLINK_FRAME_IDENTITY_MASK) + sizeof(SkyStaticHeader);
	unsigned int end = cursor + extension_length;
	if (end > frame->length)
		return SKY_RET_INVALID_EXT_LENGTH;

	// Iterate all extension headers
	while (cursor < end)
	{
		// Cast a pointer to cursor position
		SkyHeaderExtension* ext = (SkyHeaderExtension*)&frame->raw[cursor];
		printf("extension %d %d \n", ext->type, ext->length);

		// Move cursor forward and check overflow
		cursor += 1 + ext->length;
		if (cursor > frame->length)
			return SKY_RET_INVALID_EXT_LENGTH;

		// Validate header field length and store the pointer to the results struct.
		switch (ext->type)
		{
		case EXTENSION_ARQ_SEQUENCE:
			if (parsed->arq_sequence != NULL)
				return SKY_RET_REDUNDANT_EXTENSIONS;
			if (ext->length != sizeof(ExtARQSeq))
				return SKY_RET_INVALID_EXT_LENGTH;
			parsed->arq_sequence = ext;
			break;

		case EXTENSION_ARQ_REQUEST:
			if (parsed->arq_request != NULL)
				return SKY_RET_REDUNDANT_EXTENSIONS;
			if (ext->length != sizeof(ExtARQReq))
				return SKY_RET_INVALID_EXT_LENGTH;
			parsed->arq_request = ext;
			break;

		case EXTENSION_ARQ_CTRL:
			if (parsed->arq_ctrl != NULL)
				return SKY_RET_REDUNDANT_EXTENSIONS;
			if (ext->length != sizeof(ExtARQCtrl))
				return SKY_RET_INVALID_EXT_LENGTH;
			parsed->arq_ctrl = ext;
			break;

		case EXTENSION_ARQ_HANDSHAKE:
			if (parsed->arq_handshake != NULL)
				return SKY_RET_REDUNDANT_EXTENSIONS;
			if (ext->length != sizeof(ExtARQHandshake))
				return SKY_RET_INVALID_EXT_LENGTH;
			parsed->arq_handshake = ext;
			break;

		case EXTENSION_MAC_TDD_CONTROL:
			if (parsed->mac_tdd != NULL)
				return SKY_RET_REDUNDANT_EXTENSIONS;
			if (ext->length != sizeof(ExtTDDControl))
				return SKY_RET_INVALID_EXT_LENGTH;
			parsed->mac_tdd = ext;
			break;

		case EXTENSION_HMAC_SEQUENCE_RESET:
			if (parsed->hmac_reset != NULL)
				return SKY_RET_REDUNDANT_EXTENSIONS;
			if (ext->length != sizeof(ExtHMACSequenceReset))
				return SKY_RET_INVALID_EXT_LENGTH;
			parsed->hmac_reset = ext;
			break;

		default:
			return SKY_RET_INVALID_EXT_TYPE;
		}


	}

	return SKY_RET_OK;
}
