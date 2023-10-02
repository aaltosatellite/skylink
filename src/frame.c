#include "skylink/frame.h"
#include "skylink/fec.h"
#include "skylink/hmac.h"
#include "skylink/utilities.h"

#include "sky_platform.h"

#include <string.h> // memset, memcpy

// Allocate memory for new frame and set memory to zero.
SkyRadioFrame* sky_frame_create()
{
	SkyRadioFrame* frame = SKY_MALLOC(sizeof(SkyRadioFrame));
	memset(frame, 0, sizeof(SkyRadioFrame));
	return frame;
}

// Free memory of frame.
void sky_frame_destroy(SkyRadioFrame* frame)
{
	SKY_FREE(frame);
}

// Set memory of frame to zero.
void sky_frame_clear(SkyRadioFrame* frame)
{
	memset(frame, 0, sizeof(SkyRadioFrame));
}



//=== ENCODING =========================================================================================================
//======================================================================================================================

// Add ARQ sequence number to the frame.
int sky_frame_add_extension_arq_sequence(SkyTransmitFrame *tx_frame, sky_arq_sequence_t sequence)
{
	// Ensure that the extensions field is the last field in the frame and the frame still has room for the extension.
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length + 1 + sizeof(ExtARQSeq) < SKY_PAYLOAD_MAX_LEN);

	// Cast a pointer to the cursor position and fill the extension header.
	SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame->ptr;
	extension->type = EXTENSION_ARQ_SEQUENCE;
	extension->length = sizeof(ExtARQSeq);
	extension->ARQSeq.sequence = sky_arq_seq_hton(sequence);

	// Move cursor forward and update frame and extension length.
	const unsigned int len = 1 + sizeof(ExtARQSeq);
	tx_frame->hdr->extension_length += len;
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

// Add ARQ Retransmit Request header to the frame.
int sky_frame_add_extension_arq_request(SkyTransmitFrame *tx_frame, sky_arq_sequence_t sequence, sky_arq_mask_t mask)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length + 1 + sizeof(ExtARQReq) < SKY_PAYLOAD_MAX_LEN);

	// Cast a pointer to the extension header and fill the extension header.
	SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame->ptr;
	extension->type = EXTENSION_ARQ_REQUEST;
	extension->length = sizeof(ExtARQReq);
	extension->ARQReq.sequence = sky_arq_seq_hton(sequence);
	extension->ARQReq.mask = sky_arq_mask_hton(mask);

	// Move cursor forward and update frame and extension length.
	const unsigned int len = 1 + sizeof(ExtARQReq);
	tx_frame->hdr->extension_length += len;
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

// Add ARQ Control header to the frame.
int sky_frame_add_extension_arq_ctrl(SkyTransmitFrame *tx_frame, sky_arq_sequence_t tx_sequence, sky_arq_sequence_t rx_sequence)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length + 1 + sizeof(ExtARQCtrl) < SKY_PAYLOAD_MAX_LEN);

	// Cast a pointer to the extension header and fill the extension header.
	SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame->ptr;
	extension->type = EXTENSION_ARQ_CTRL;
	extension->length = sizeof(ExtARQCtrl);
	extension->ARQCtrl.tx_sequence = sky_arq_seq_hton(tx_sequence);
	extension->ARQCtrl.rx_sequence = sky_arq_seq_hton(rx_sequence);

	// Move cursor forward and update frame and extension length.
	const unsigned int len = 1 + sizeof(ExtARQCtrl);
	tx_frame->hdr->extension_length += len;
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

// Add ARQ Handshake header to the frame.
int sky_frame_add_extension_arq_handshake(SkyTransmitFrame *tx_frame, uint8_t state_flag, uint32_t identifier)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length + 1 + sizeof(ExtARQHandshake) < SKY_PAYLOAD_MAX_LEN);

	// Cast a pointer to the extension header and fill the extension header.
	SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame->ptr;
	extension->type = EXTENSION_ARQ_HANDSHAKE;
	extension->length = sizeof(ExtARQHandshake);
	extension->ARQHandshake.peer_state = state_flag;
	extension->ARQHandshake.identifier = sky_hton32(identifier);

	// Move cursor forward and update frame and extension length.
	const unsigned int len = 1 + sizeof(ExtARQHandshake);
	tx_frame->hdr->extension_length += len;
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

// Add MAC TDD control header to the frame.
int sky_frame_add_extension_mac_tdd_control(SkyTransmitFrame *tx_frame, uint16_t window, uint16_t remaining)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length < SKY_PAYLOAD_MAX_LEN - sizeof(ExtTDDControl));

	// Cast a pointer to the extension header and fill the extension header.
	SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame->ptr;
	extension->type = EXTENSION_MAC_TDD_CONTROL;
	extension->length = sizeof(ExtTDDControl);
	extension->TDDControl.window = sky_hton16(window);
	extension->TDDControl.remaining = sky_hton16(remaining);

	// Move cursor forward and update frame and extension length.
	const unsigned int len = 1 + sizeof(ExtTDDControl);
	tx_frame->hdr->extension_length += len;
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

// Add HMAC sequence reset header to the frame.
int sky_frame_add_extension_hmac_sequence_reset(SkyTransmitFrame *tx_frame, uint16_t sequence)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length < SKY_PAYLOAD_MAX_LEN - sizeof(ExtHMACSequenceReset));

	// Cast a pointer to the extension header and fill the extension header.
	SkyHeaderExtension *extension = (SkyHeaderExtension *)tx_frame->ptr;
	extension->type = EXTENSION_HMAC_SEQUENCE_RESET;
	extension->length = sizeof(ExtHMACSequenceReset);
	extension->HMACSequenceReset.sequence = sky_hton16(sequence);

	// Move cursor forward and update frame and extension length.
	const unsigned int len = 1 + sizeof(ExtHMACSequenceReset);
	tx_frame->frame->length += len;
	tx_frame->hdr->extension_length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

// Get number of bytes left in the frame.
int sky_frame_get_space_left(const SkyRadioFrame *frame)
{
	return RS_MSGLEN - (frame->length + SKY_HMAC_LENGTH);
}

// Fill the rest of the frame with payload data.
int sky_frame_extend_with_payload(SkyTransmitFrame *tx_frame, const uint8_t *payload, unsigned int payload_length)
{
	// TODO: Unused function
	SKY_ASSERT(tx_frame->hdr->flag_has_payload == 0);
	//Check that the payload fits in the frame.
	if (sky_frame_get_space_left(tx_frame->frame) < (int)payload_length)
		return SKY_RET_NO_SPACE_FOR_PAYLOAD;

	// Copy payload to the frame and update frame length and payload flag.
	memcpy(tx_frame->ptr, payload, payload_length);

	// Increment lengths and write pointer
	tx_frame->ptr += payload_length;
	tx_frame->frame->length += payload_length;
	tx_frame->hdr->flag_has_payload = 1;
	return SKY_RET_OK;
}

//=== ENCODING =========================================================================================================
//======================================================================================================================


// Parse and validate all header extensions inside the frame.
int sky_frame_parse_extension_headers(const SkyRadioFrame* frame, SkyParsedFrame* parsed)
{


	// Get cursor position for the start of the extension header.
	unsigned int cursor = 1 + (frame->raw[0] & SKYLINK_FRAME_IDENTITY_MASK) + sizeof(SkyStaticHeader);
	// Get the end position of the extension header.
	unsigned int end = cursor + parsed->hdr.extension_length;
	// Check for overflow
	if (end > frame->length)
		return SKY_RET_INVALID_EXT_LENGTH;

	// Iterate all extension headers
	while (cursor < end)
	{
		// Cast a pointer to cursor position
		SkyHeaderExtension* ext = (SkyHeaderExtension*)&frame->raw[cursor];

		// Move cursor forward and check overflow
		cursor += 1 + ext->length;
		if (cursor > frame->length)
			return SKY_RET_INVALID_EXT_LENGTH;

		// Validate header field length and store the pointer to the results struct.
		switch (ext->type)
		{
		case EXTENSION_ARQ_SEQUENCE:
			// Check for redundant extensions and invalid length.
			if (parsed->arq_sequence != NULL)
				return SKY_RET_REDUNDANT_EXTENSIONS;
			if (ext->length != sizeof(ExtARQSeq))
				return SKY_RET_INVALID_EXT_LENGTH;

			// Store pointer to the frame.
			parsed->arq_sequence = ext;
			break; // TODO: Endianess swaps could be done during this step.

		case EXTENSION_ARQ_REQUEST:
			// Check for redundant extensions and invalid length.
			if (parsed->arq_request != NULL)
				return SKY_RET_REDUNDANT_EXTENSIONS;
			if (ext->length != sizeof(ExtARQReq))
				return SKY_RET_INVALID_EXT_LENGTH;

			// Store pointer to the frame.
			parsed->arq_request = ext;
			break;

		case EXTENSION_ARQ_CTRL:
			// Check for redundant extensions and invalid length.
			if (parsed->arq_ctrl != NULL)
				return SKY_RET_REDUNDANT_EXTENSIONS;
			if (ext->length != sizeof(ExtARQCtrl))
				return SKY_RET_INVALID_EXT_LENGTH;

			// Store pointer to the frame.
			parsed->arq_ctrl = ext;
			break;

		case EXTENSION_ARQ_HANDSHAKE:
			// Check for redundant extensions and invalid length.
			if (parsed->arq_handshake != NULL)
				return SKY_RET_REDUNDANT_EXTENSIONS;
			if (ext->length != sizeof(ExtARQHandshake))
				return SKY_RET_INVALID_EXT_LENGTH;

			// Store pointer to the frame.
			parsed->arq_handshake = ext;
			break;

		case EXTENSION_MAC_TDD_CONTROL:
			// Check for redundant extensions and invalid length.
			if (parsed->mac_tdd != NULL)
				return SKY_RET_REDUNDANT_EXTENSIONS;
			if (ext->length != sizeof(ExtTDDControl))
				return SKY_RET_INVALID_EXT_LENGTH;

			// Store pointer to the frame.
			parsed->mac_tdd = ext;
			break;

		case EXTENSION_HMAC_SEQUENCE_RESET:
			// Check for redundant extensions and invalid length.
			if (parsed->hmac_reset != NULL)
				return SKY_RET_REDUNDANT_EXTENSIONS;
			if (ext->length != sizeof(ExtHMACSequenceReset))
				return SKY_RET_INVALID_EXT_LENGTH;

			// Store pointer to the frame.
			parsed->hmac_reset = ext;
			break;

		default: // Invalid extension type
			return SKY_RET_INVALID_EXT_TYPE;
		}


	}
	// Parsing was successful.
	return SKY_RET_OK;
}
