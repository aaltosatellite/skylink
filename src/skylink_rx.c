#include "skylink/skylink.h"
#include "skylink/diag.h"
#include "skylink/fec.h"
#include "skylink/reliable_vc.h"
#include "skylink/frame.h"
#include "skylink/mac.h"
#include "skylink/hmac.h"
#include "skylink/utilities.h"

#include "ext/gr-satellites/golay24.h"

#include <string.h> // memcmp


static void sky_rx_process_ext_mac_control(SkyHandle self, int rx_time_ticks, SkyParsedFrame *parsed);

int sky_rx_with_golay(SkyHandle self, SkyRadioFrame* frame)
{
	// Read Golay coded length
	uint32_t coded_len = (frame->raw[0] << 16) | (frame->raw[1] << 8) | frame->raw[2];
	int ret = decode_golay24(&coded_len);
	if (ret < 0) {
		SKY_PRINTF(SKY_DIAG_FEC, COLOR_RED "Golay failed" COLOR_RESET "\n");
		self->diag->rx_fec_fail++;
		return SKY_RET_GOLAY_FAILED;
	}

	frame->length = (int32_t)coded_len & SKY_GOLAY_PAYLOAD_LENGTH_MASK;
	if (frame->length > sizeof(frame->raw))
		return SKY_RET_INVALID_ENCODED_LENGTH;

	// Remove the length header from the rest of the data
	for (unsigned int i = 0; i < frame->length; i++)
		frame->raw[i] = frame->raw[i + 3];

	return sky_rx_with_fec(self, frame);
}


int sky_rx_with_fec(SkyHandle self, SkyRadioFrame* frame)
{
	if (frame->length < SKY_FRAME_MIN_LEN + RS_PARITYS) {
		SKY_PRINTF(SKY_DIAG_FRAMES, COLOR_RED "Too short frame" COLOR_RESET "\n");
		return SKY_RET_INVALID_ENCODED_LENGTH;
	}

	// Decode FEC
	int ret = sky_fec_decode(frame, self->diag);
	if (ret < 0)
		return ret;

	return sky_rx(self, frame);
}

static int filter_by_identity(SkyHandle self, const uint8_t *identity, unsigned int identity_len)
{
	//
	if (memcmp(identity, self->conf->identity, identity_len) == 0 && self->conf->identity_len == identity_len)
		return 1;

	return 0;
}

int sky_rx(SkyHandle self, const SkyRadioFrame* frame)
{
	// Some error checks
	if(frame->length < SKY_FRAME_MIN_LEN){
		SKY_PRINTF(SKY_DIAG_FRAMES, COLOR_RED "Too short frame" COLOR_RESET "\n");
		return SKY_RET_INVALID_PLAIN_LENGTH;
	}

	self->diag->rx_frames++;
	self->diag->rx_bytes += frame->length;

	int ret;
	SkyParsedFrame parsed;
	memset(&parsed, 0, sizeof(SkyParsedFrame));

	// Validate protocol version
	const uint8_t version = frame->raw[0] & SKYLINK_FRAME_VERSION_BYTE;
	if (version != SKYLINK_FRAME_VERSION_BYTE)
		return SKY_RET_INVALID_VERSION;

	// Validate identity field
	parsed.identity = &frame->raw[1];
	parsed.identity_len = (frame->raw[0] & SKYLINK_FRAME_IDENTITY_MASK);
	if (parsed.identity_len == 0 || parsed.identity_len > SKY_MAX_IDENTITY_LEN)
		return SKY_RET_INVALID_VERSION;

	// Identity filtering
	if (filter_by_identity(self, parsed.identity, parsed.identity_len)) // TODO: Filtering callback function
		return SKY_RET_FILTERED_BY_IDENDITY;

	// Parse header
	const unsigned header_start = 1 + parsed.identity_len;
	memcpy(&parsed.hdr, &frame->raw[header_start], sizeof(SkyStaticHeader));
	const unsigned payload_start = header_start + sizeof(SkyStaticHeader) + parsed.hdr.extension_length;
	if (payload_start > frame->length)
		return SKY_RET_INVALID_EXT_LENGTH;
	const unsigned vc = parsed.hdr.vc;

#if SKY_NUM_VIRTUAL_CHANNELS < 4
	if (vc >= SKY_NUM_VIRTUAL_CHANNELS)
		return SKY_RET_INVALID_VC;
#endif

	// Extract the frame payload
	parsed.payload = &frame->raw[payload_start];
	if ((parsed.hdr.flags & SKY_FLAG_HAS_PAYLOAD) != 0)
		parsed.payload_len = frame->length - payload_start;
	else
		parsed.payload_len = 0; // Ignore frame payload if payload flag is not set.

	// Parse and validate all extension headers
	if ((ret = sky_frame_parse_extension_headers(frame, &parsed)) < 0)
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

	return sky_vc_process_frame(self->virtual_channels[vc], &parsed, sky_get_tick_time());
}



static void sky_rx_process_ext_mac_control(SkyHandle self, int rx_time_ticks, SkyParsedFrame* parsed)
{
	const SkyHeaderExtension *tdd_ext = parsed->mac_tdd;
	if (tdd_ext == NULL)
		return;

	if (self->conf->mac.unauthenticated_mac_updates == 0 && (parsed->hdr.flags & SKY_FLAG_AUTHENTICATED) == 0)
		return;

	uint16_t w = sky_ntoh16(tdd_ext->TDDControl.window);
	uint16_t r = sky_ntoh16(tdd_ext->TDDControl.remaining);
	SKY_PRINTF(SKY_DIAG_MAC | SKY_DIAG_DEBUG, "MAC Updated: Window length %d, window remaining %d\n", w, r);

	mac_update_belief(self->mac, sky_get_tick_time(), rx_time_ticks, w, r);
}
