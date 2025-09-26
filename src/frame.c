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
	//SKY_ASSERT(tx_frame->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length + sizeof(ExtARQSeq) < SKY_PAYLOAD_MAX_LEN);

	// Cast a pointer to the cursor position and fill the extension header.
	ExtARQSeq *extension = (ExtARQSeq *)tx_frame->ptr;
	extension->sequence = sky_arq_seq_hton(sequence);

	// Move cursor forward and update frame and extension length.
	const unsigned int len = sizeof(ExtARQSeq);
	tx_frame->hdr->included_extensions |= (1 << EXTENSION_ARQ_SEQUENCE);
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

// Add ARQ Retransmit Request header to the frame.
int sky_frame_add_extension_arq_request(SkyTransmitFrame *tx_frame, sky_arq_sequence_t sequence, sky_arq_mask_t mask)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	//SKY_ASSERT(tx_frame->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length + sizeof(ExtARQReq) < SKY_PAYLOAD_MAX_LEN);

	// Cast a pointer to the extension header and fill the extension header.
	ExtARQReq *extension = (ExtARQReq *)tx_frame->ptr;
	extension->sequence = sky_arq_seq_hton(sequence);
	extension->mask = sky_arq_mask_hton(mask);

	// Move cursor forward and update frame and extension length.
	const unsigned int len = sizeof(ExtARQReq);
	tx_frame->hdr->included_extensions |= (1 << EXTENSION_ARQ_REQUEST);
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

// Add ARQ Control header to the frame.
int sky_frame_add_extension_arq_ctrl(SkyTransmitFrame *tx_frame, sky_arq_sequence_t tx_sequence, sky_arq_sequence_t rx_sequence)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	//SKY_ASSERT(tx_frame->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length + sizeof(ExtARQCtrl) < SKY_PAYLOAD_MAX_LEN);

	// Cast a pointer to the extension header and fill the extension header.
	ExtARQCtrl *extension = (ExtARQCtrl *)tx_frame->ptr;
	extension->rx_sequence = sky_arq_seq_hton(rx_sequence);
	extension->tx_sequence = sky_arq_seq_hton(tx_sequence);

	// Move cursor forward and update frame and extension length.
	const unsigned int len = sizeof(ExtARQCtrl);
	tx_frame->hdr->included_extensions |= (1 << EXTENSION_ARQ_CTRL);
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

// Add ARQ Handshake header to the frame.
int sky_frame_add_extension_arq_handshake(SkyTransmitFrame *tx_frame, uint8_t state_flag, uint32_t identifier)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	//SKY_ASSERT(tx_frame->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length + sizeof(ExtARQHandshake) < SKY_PAYLOAD_MAX_LEN);

	// Cast a pointer to the extension header and fill the extension header.
	ExtARQHandshake *extension = (ExtARQHandshake *)tx_frame->ptr;

	// Just set peer state with overflow to prevent endianess issues.
	// Lowest 2 bits are peer state, rest is identifier.
	uint16_t id_and_state = (identifier % 0x3FFF) << 2 | (state_flag & 0x03);
	extension->identifier_and_peer_state = sky_hton16(id_and_state);

	// Move cursor forward and update frame and extension length.
	const unsigned int len = sizeof(ExtARQHandshake);
	tx_frame->hdr->included_extensions |= (1 << EXTENSION_ARQ_HANDSHAKE);
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

// Add MAC TDD control header to the frame.
int sky_frame_add_extension_mac_tdd_control(SkyTransmitFrame *tx_frame, uint16_t window, uint16_t remaining)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	//SKY_ASSERT(tx_frame->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length < SKY_PAYLOAD_MAX_LEN - sizeof(ExtTDDControl));

	// Cast a pointer to the extension header and fill the extension header.
	ExtTDDControl *extension = (ExtTDDControl *)tx_frame->ptr;
	extension->window = sky_hton16(window);
	extension->remaining = sky_hton16(remaining);

	// Move cursor forward and update frame and extension length.
	const unsigned int len = sizeof(ExtTDDControl);
	tx_frame->hdr->included_extensions |= (1 << EXTENSION_MAC_TDD_CONTROL);
	tx_frame->frame->length += len;
	tx_frame->ptr += len;
	return SKY_RET_OK;
}

// Add HMAC sequence reset header to the frame.
int sky_frame_add_extension_hmac_sequence_reset(SkyTransmitFrame *tx_frame, uint16_t sequence)
{
	// Ensure that the extensions field is the last field in the frame and frame has still room for the extension.
	//SKY_ASSERT(tx_frame->flag_has_payload == 0);
	SKY_ASSERT(tx_frame->frame->length < SKY_PAYLOAD_MAX_LEN - sizeof(ExtHMACSequenceReset));

	// Cast a pointer to the extension header and fill the extension header.
	ExtHMACSequenceReset *extension = (ExtHMACSequenceReset *)tx_frame->ptr;
	extension->sequence = sky_hton16(sequence);

	// Move cursor forward and update frame and extension length.
	const unsigned int len = sizeof(ExtHMACSequenceReset);
	tx_frame->hdr->included_extensions |= (1 << EXTENSION_HMAC_SEQUENCE_RESET);
	tx_frame->frame->length += len;
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
	// SKY_ASSERT(tx_frame->flag_has_payload == 0);
	// Check that the payload fits in the frame.
	if (sky_frame_get_space_left(tx_frame->frame) < (int)payload_length)
		return SKY_RET_NO_SPACE_FOR_PAYLOAD;

	// Copy payload to the frame and update frame length and payload flag.
	memcpy(tx_frame->ptr, payload, payload_length);

	// Increment lengths and write pointer
	tx_frame->ptr += payload_length;
	tx_frame->frame->length += payload_length;
	return SKY_RET_OK;
}

//=== ENCODING =========================================================================================================
//======================================================================================================================


// Parse and validate all header extensions inside the frame.
int sky_frame_parse_extension_headers(const SkyRadioFrame* frame, SkyParsedFrame* parsed, unsigned int extension_length)
{
	// Get cursor position for the start of the extension header.
	unsigned int cursor = 6 + sizeof(SkyStaticHeader);
	// Get the end position of the extension header.
	unsigned int end = cursor + extension_length;
	// Check for overflow
	if (end > frame->length)
		return SKY_RET_INVALID_EXT_LENGTH;

	uint8_t extensions = parsed->hdr.included_extensions;
	uint8_t current_extension = 0;

	// Iterate all extension headers
	while (extensions)
	{
		uint8_t ext_found = (extensions & (0x01 << current_extension));
		extensions = extensions & ~(0x01 << current_extension);
		 // Extension not present, skip to next extension type.
		if (ext_found == 0) {
			current_extension++;
			continue;
		}
		switch (current_extension)
		{
		case EXTENSION_ARQ_SEQUENCE:
			ExtARQSeq *arq_seq = (ExtARQSeq*)&frame->raw[cursor];
			parsed->arq_sequence = arq_seq;
			cursor += sizeof(ExtARQSeq);
			break;
		case EXTENSION_ARQ_REQUEST:
			ExtARQReq *arq_req = (ExtARQReq*)&frame->raw[cursor];
			parsed->arq_request = arq_req;
			cursor += sizeof(ExtARQReq);
			break;
		case EXTENSION_ARQ_CTRL:
			ExtARQCtrl *arq_ctrl = (ExtARQCtrl*)&frame->raw[cursor];
			parsed->arq_ctrl = arq_ctrl;
			cursor += sizeof(ExtARQCtrl);
			break;
		case EXTENSION_ARQ_HANDSHAKE:
			ExtARQHandshake *arq_handshake = (ExtARQHandshake*)&frame->raw[cursor];
			parsed->arq_handshake = arq_handshake;
			cursor += sizeof(ExtARQHandshake);
			break;
		case EXTENSION_MAC_TDD_CONTROL:
			ExtTDDControl *mac_tdd = (ExtTDDControl*)&frame->raw[cursor];
			parsed->mac_tdd = mac_tdd;
			cursor += sizeof(ExtTDDControl);
			break;
		case EXTENSION_HMAC_SEQUENCE_RESET:
			ExtHMACSequenceReset *hmac_reset = (ExtHMACSequenceReset*)&frame->raw[cursor];
			parsed->hmac_reset = hmac_reset;
			cursor += sizeof(ExtHMACSequenceReset);
			break;
		default: // Invalid extension type
			return SKY_RET_INVALID_EXT_TYPE;
		}
		current_extension++;
	}
	// Parsing was successful.
	return SKY_RET_OK;
}
