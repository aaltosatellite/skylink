#include "skylink/skylink.h"
#include "skylink/diag.h"
#include "skylink/fec.h"
#include "skylink/reliable_vc.h"
#include "skylink/frame.h"
#include "skylink/mac.h"
#include "skylink/hmac.h"
#include "skylink/crc.h"
#include "skylink/utilities.h"

#include "ext/gr-satellites/golay24.h"

#include <string.h> // memcmp, memset


static void sky_rx_process_ext_mac_control(SkyHandle self, int rx_time_ticks, SkyParsedFrame *parsed);

// Pass recieved frame with FEC and Golay header for the protocol logic.
int sky_rx_with_golay(SkyHandle self, SkyRadioFrame* frame)
{
	// Read Golay coded length
	uint32_t coded_len = (frame->raw[0] << 16) | (frame->raw[1] << 8) | frame->raw[2];

	// Decode Golay
	int ret = decode_golay24(&coded_len);

	//ERROR: Golay decode failed
	if (ret < 0) {
		// Debug print and increment error counter.
		SKY_PRINTF(SKY_DIAG_FEC, COLOR_RED "Golay failed" COLOR_RESET "\n");
		self->diag->rx_fec_fail++;
		return SKY_RET_GOLAY_FAILED;
	}

	//	Get frame length
	frame->length = (int32_t)coded_len & SKY_GOLAY_PAYLOAD_LENGTH_MASK;

	// ERROR: Frame length is too long.
	if (frame->length > sizeof(frame->raw))
		return SKY_RET_INVALID_ENCODED_LENGTH;

	// Remove the length header from the rest of the data (Shift the data 3 bytes to the left)
	for (unsigned int i = 0; i < frame->length; i++)
		frame->raw[i] = frame->raw[i + 3];

	// Decode FEC
	return sky_rx_with_fec(self, frame);
}


// Pass recieved frame with FEC for the protocol logic.
int sky_rx_with_fec(SkyHandle self, SkyRadioFrame* frame)
{
	// ERROR: Frame length is too short. (Shorter than the minimum frame length + FEC parity bytes)
	if (frame->length < SKY_FRAME_MIN_LEN + RS_PARITYS) {
		SKY_PRINTF(SKY_DIAG_FRAMES, COLOR_RED "Too short frame" COLOR_RESET "\n");
		return SKY_RET_INVALID_ENCODED_LENGTH;
	}

	// Decode FEC
	int ret = sky_fec_decode(frame, self->diag);
	if (ret < 0)
		return ret;

	// Pass the decoded frame to the protocol logic.
	return sky_rx(self, frame);
}

// Returns boolean 1/0 whether the frame should be filtered out.
static int filter_by_identity(SkyHandle self, const uint8_t *identity)
{
	// Same identity as in configuration
	if ((memcmp(identity, self->conf->identity, 6) == 0))
		return 1;

	return SKY_RET_OK;
}
static unsigned int calculate_extension_length(uint8_t included_extensions)
{
	unsigned int length = 0;
	if (included_extensions & (1 << EXTENSION_ARQ_SEQUENCE))
		length += sizeof(ExtARQSeq);
	if (included_extensions & (1 << EXTENSION_ARQ_REQUEST))
		length += sizeof(ExtARQReq);
	if (included_extensions & (1 << EXTENSION_ARQ_CTRL))
		length += sizeof(ExtARQCtrl);
	if (included_extensions & (1 << EXTENSION_ARQ_HANDSHAKE))
		length += sizeof(ExtARQHandshake);
	if (included_extensions & (1 << EXTENSION_MAC_TDD_CONTROL))
		length += sizeof(ExtTDDControl);
	if (included_extensions & (1 << EXTENSION_HMAC_SEQUENCE_RESET))
		length += sizeof(ExtHMACSequenceReset);
	return length;
}

// Pass recieved frame for the protocol logic.
int sky_rx(SkyHandle self, const SkyRadioFrame* frame)
{
	// Check that frame is longer than required minimum.
	if(frame->length < SKY_FRAME_MIN_LEN){
		SKY_PRINTF(SKY_DIAG_FRAMES, COLOR_RED "Too short frame" COLOR_RESET "\n");
		return SKY_RET_INVALID_PLAIN_LENGTH;
	}

	// Update stats for diagnostics.
	self->diag->rx_frames++;
	self->diag->rx_bytes += frame->length;

	int ret;

	// Initialize parsed frame structure to zero.
	SkyParsedFrame parsed;
	memset(&parsed, 0, sizeof(SkyParsedFrame));

	// Validate identity field
	parsed.identity = &frame->raw[0];

	// Identity filtering
	if (filter_by_identity(self, parsed.identity))
		return SKY_RET_FILTERED_BY_IDENTITY;

	// Get start position for header and payload. Copy header to parsed frame structure.
	const unsigned header_start = 6; // Just the identity
	memcpy(&parsed.hdr, &frame->raw[header_start], sizeof(SkyStaticHeader));
	const unsigned extension_length = calculate_extension_length(parsed.hdr.included_extensions);
	const unsigned payload_start = header_start + sizeof(SkyStaticHeader) + extension_length;

	// Frame length is smaller than where the payload should start.
	if (payload_start > frame->length)
		return SKY_RET_INVALID_EXT_LENGTH;

	// Get virtual channel
	const unsigned vc = parsed.hdr.vc;

// VC validation, only done when using less than 4 VC's
#if SKY_NUM_VIRTUAL_CHANNELS < 4
	if (vc >= SKY_NUM_VIRTUAL_CHANNELS)
		return SKY_RET_INVALID_VC;
#endif

	// Extract the frame payload
	parsed.payload = &frame->raw[payload_start];
	parsed.payload_len = frame->length - payload_start;

	// Validate CRC32 if present
	if (parsed.hdr.flag_crced) {
		if ((ret = sky_check_crc32(frame, &parsed)) < 0)
			return ret;
	}

	// Parse and validate all extension headers
	if ((ret = sky_frame_parse_extension_headers(frame, &parsed, extension_length)) < 0)
		return ret;

	// Check the authentication/HMAC if the virtual channel necessitates it.
	if ((ret = sky_hmac_check_authentication(self, frame, &parsed)) < 0)
		return ret;

#ifdef SKY_USE_TDD_MAC
	// Update MAC/TDD state, and check for MAC/TDD handshake extension
	sky_rx_process_ext_mac_control(self, frame->rx_time_ticks, &parsed);
#endif

	// Increment VC RX frame count when the frame has been "accepted".
	self->diag->vc_stats[vc].rx_frames++;

	// Pass the parsed frame to be processed.
	return sky_vc_process_frame(self->virtual_channels[vc], &parsed, sky_get_tick_time());
}



static void sky_rx_process_ext_mac_control(SkyHandle self, int rx_time_ticks, SkyParsedFrame* parsed)
{
	// Check if the frame has a MAC/TDD extension
	const ExtTDDControl *tdd_ext = parsed->mac_tdd;
	if (tdd_ext == NULL)
		return;

	// No unauthenticated MAC updates and frame is not authenticated.
	// (Commented out, since the attack this protects against also doubles as simple ewar blocking attack)
	//if (self->conf->mac.unauthenticated_mac_updates == 0 && parsed->hdr.flag_authenticated == 0)
	//	return;

	// Get window and remaining time
	uint16_t w = sky_ntoh16(tdd_ext->window);
	uint16_t r = sky_ntoh16(tdd_ext->remaining);

	// Print debug info
	SKY_PRINTF(SKY_DIAG_MAC | SKY_DIAG_DEBUG, "MAC Updated: Window length %d, window remaining %d\n", w, r);

	// Update MAC system's belief of current status of windowing.
	mac_update_belief(self->mac, sky_get_tick_time(), rx_time_ticks, w, r);
}
