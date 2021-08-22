#include "skylink/skylink.h"
#include "skylink/diag.h"
#include "skylink/fec.h"
#include "skylink/hmac.h"
#include "skylink/endian.h"
#include "skylink_platform.h"

#include <stdlib.h>
#include <string.h>



unsigned int sky_diag_mask = SKY_DIAG_LINK_STATE | SKY_DIAG_BUG | SKY_DIAG_FRAMES | SKY_DIAG_DEBUG | SKY_DIAG_INFO;

/** Initialization */
SkyHandle_t sky_init(SkyHandle_t self, SkyConfig_t *conf)
{
	SKY_ASSERT(conf);

	if (self == NULL)
		self = SKY_MALLOC(sizeof(*self));

	self->conf = conf;
	self->diag = SKY_CALLOC(1, sizeof(*self->diag));

	//self->mac = sky_mac_init(self);

	// ARQs
	//self->arqtx[0] = ap_arqtx_init(&conf->arqtx_conf);
	//self->arqrx[0] = ap_arqrx_init(&conf->arqrx_conf);

	return self;
}


int sky_rx(SkyHandle_t self, SkyRadioFrame_t *frame)
{
	SKY_ASSERT(self && frame);
	int ret;

	// Decode FEC
	if ((ret = sky_fec_decode(frame, self->diag)) < 0)
		return ret;

	// Check authentication code if the frame claims it is authenticated.
	if (frame->hdr.flags & SKY_FRAME_AUTHENTICATED) {
		if ((ret = sky_hmac_check_authentication(self, frame)) < 0)
			return ret;
	}

	//ap_mac_rx(self, frame);
	sky_buf_write(self->rxbuf[0], frame->raw, frame->length, BUF_FIRST_SEG | BUF_LAST_SEG);

	return 0;
}


int sky_rx_raw(SkyHandle_t self, SkyRadioFrame_t *frame)
{
	SKY_ASSERT(self && frame);

	// Read Golay decoded len
	uint32_t coded_len = (frame->raw[0] << 16) | (frame->raw[1] << 8) | frame->raw[2];

	int ret = decode_golay24(&coded_len);
	if (ret < 0) {
		// TODO: log the number of corrected bits?
		self->diag->rx_fec_fail++;
		SKY_PRINTF(SKY_DIAG_FRAMES, "%10u: Golay failed %d\n", get_timestamp(), ret);
		return SKY_RET_GOLAY_FAILED;
	}

	if ((coded_len & 0xF00) != (SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED))
		return -1;

	frame->length = coded_len & SKY_GOLAY_PAYLOAD_LENGTH_MASK;

	// Remove the length header from the rest of the data
	for (unsigned int i = 0; i < frame->length; i++)
		frame->raw[i] = frame->raw[i + 3];

	return sky_rx(self, frame);
}


int sky_tx(SkyHandle_t self, SkyRadioFrame_t *frame, timestamp_t current_time)
{
	SKY_ASSERT(self && frame);

	int loc = 0;

	// Reserve space for the MAC header
	if (0)
		loc += 2;

	if (0 && ap_mac_tx(self, frame, current_time) < 0)
		return -1;


	int flags;

	int ret = sky_buf_read(self->txbuf[0], &frame->payload[loc], 200, &flags);
	if (ret < 0)
		return -1;


	frame->length = ret;

	frame->hdr.flags = 0;
	frame->hdr.apid = 0;

	frame->phy.length = 0;

	/*
	 * Authenticate the frame
	 */
	sky_hmac_authenticate(self, frame);

	/*
	 * Apply Forward Error Correction (FEC) coding
	 */
	sky_fec_encode(frame);

	++self->diag->tx_frames;

	return 0;
}



int sky_tx_raw(SkyHandle_t ap, SkyRadioFrame_t *frame, timestamp_t current_time) {

	int ret = sky_tx(ap, frame, current_time);
	if (ret != 0)
		return ret;

	/* Move the data by 3 bytes to make room for the PHY header */
	for (unsigned int i = frame->length; i != 0; i--)
		frame->raw[i + 3] = frame->raw[i];

	/*
	 * Decode length field.
	 * Remark: Return code ignored. It's always 0.
	 */
	uint32_t phy_header = frame->length | SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED;
	encode_golay24(&phy_header);

	frame->raw[0] = 0xff & (phy_header >> 16);
	frame->raw[1] = 0xff & (phy_header >> 8);
	frame->raw[2] = 0xff & (phy_header >> 0);

	return SKY_RET_OK;
}



int sky_vc_write(SkyHandle_t self, SkyVirtualChannel_t vc, const uint8_t *data, unsigned datalen, unsigned flags) {
	//return sky_buf_write(ap1->txbuf[(cmd - CMD_WRITE_VC0)/2], data+1, len-1, BUF_FIRST_SEG|BUF_LAST_SEG);
	return 0;
}


int sky_set_config(SkyHandle_t self, unsigned int cfg, unsigned int val) {
	SKY_ASSERT(self);
	(void)self; (void)cfg; (void)val;
	return -1;
}


int sky_get_config(SkyHandle_t self, unsigned int cfg, unsigned int* val) {
	SKY_ASSERT(self);
	(void)self; (void)cfg; (void)val;
	return -1;
}



int sky_print_diag(SkyHandle_t self)
{
	SKY_ASSERT(self);

	unsigned i;
	const SkyDiagnostics_t *diag = self->diag;
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "\033[H\033[2J");
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Received frames: %5u total, %5u OK, %5u failed. FEC corrected octets %5u/%u\n",
		diag->rx_frames, diag->rx_fec_ok, diag->rx_fec_fail, diag->rx_fec_errs, diag->rx_fec_octs);
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Transmit frames: %5u\n",
		diag->tx_frames);

	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Buffer fullness RX: ");
	for (i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++)
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "%4u ", ap_buf_fullness(self->rxbuf[i]));
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "TX: ");
	for (i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++)
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "%4u ", ap_buf_fullness(self->txbuf[i]));
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "\n");

	/*for (i = 0; i < AP_N_ARQS; i++) {
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "ARQ %u RX:\n", i);
		ap_arqrx_print(self->arqrx[i]);
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "\n");
	}
	for (i = 0; i < AP_N_ARQS; i++) {
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "ARQ %u TX:\n", i);
		ap_arqtx_print(self->arqtx[i]);
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "\n");
	}*/

	return 0;
}


void sky_diag_dump_hex(uint8_t* data, unsigned int data_len) {

	char str[3];
	const char hex[17] = "0123456789ABCDEF";

	while (data_len) {

		str[0] = hex[0xF & (*data >> 4)];
		str[1] = hex[0xF & (*data >> 0)];
		str[2] = '\0';

		SKY_PRINTF(SKY_DIAG_DEBUG, "%s ", str);

		data++;
		data_len--;
	}
	SKY_PRINTF(SKY_DIAG_DEBUG, "\n");
}


int sky_clear_stats(SkyHandle_t self) {
	memset(self->diag, 0, sizeof(SkyDiagnostics_t));
	return 0;
}


int sky_get_buffer_status(SkyHandle_t self, SkyBufferState_t* state) {
	SKY_ASSERT(self && state);

	for (int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		state->state[vc] = 0;
		state->rx_free[vc] = sky_buf_fullness(self->rxbuf[vc]);
		state->tx_avail[vc] = sky_buf_space(self->rxbuf[vc]);
	}

	return 0;
}


int sky_flush_buffers(SkyHandle_t self) {
	SKY_ASSERT(self);
	for (int vc = 0; vc < 4; vc++) {
		sky_buf_flush(self->rxbuf[vc]);
		sky_buf_flush(self->txbuf[vc]);
	}
}



inline uint16_t __attribute__ ((__const__)) sky_hton16(uint16_t vh) {
#ifndef __LITTLE_ENDIAN__
	return vh;
#else
	return (((vh & 0xff00) >> 8) | ((vh & 0x00ff) << 8));
#endif
}

inline uint16_t __attribute__ ((__const__)) sky_ntoh16(uint16_t vn) {
	return sky_hton16(vn);
}


inline uint32_t __attribute__ ((__const__)) sky_hton32(uint32_t vh) {
#ifndef __LITTLE_ENDIAN__
	return vh;
#else
	return (((vh & 0xff000000) >> 24) | ((vh & 0x000000ff) << 24) |
			((vh & 0x0000ff00) <<  8) | ((vh & 0x00ff0000) >>  8));
#endif
}

inline uint32_t __attribute__ ((__const__)) sky_ntoh32(uint32_t vn) {
	return sky_hton32(vn);
}
