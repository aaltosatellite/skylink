#include "skylink/skylink.h"
#include "skylink/diag.h"
#include "skylink/fec.h"

#include "skylink_platform.h"

#include <string.h>



unsigned int sky_diag_mask = SKY_DIAG_LINK_STATE | SKY_DIAG_BUG;

/** Initialization */
SkyHandle_t sky_init(SkyHandle_t self, struct ap_conf *conf)
{
	SKY_ASSERT(conf);

	if (self == NULL)
		self = SKY_MALLOC(sizeof(*self));

	self->conf = conf;
	self->diag = SKY_CALLOC(1, sizeof(*self->diag));

	self->mac = ap_mac_init(self);

	// ARQs
	//self->arqtx[0] = ap_arqtx_init(&conf->arqtx_conf);
	//self->arqrx[0] = ap_arqrx_init(&conf->arqrx_conf);


	return self;
}


int sky_rx(SkyHandle_t ap, struct radioframe *frame)
{
	SKY_ASSERT(ap && frame);

	int ret = sky_fec_decode(frame, ap->diag);
	if (ret >= 0)
		return ap_mac_rx(ap, frame);
	else
		return -1;
}


int sky_rx_raw(SkyHandle_t ap, SkyRadioFrame_t *frame)
{
	SKY_ASSERT(ap && frame);

	// Read golay decoded len
	uint32_t coded_len = (frame->raw[0] << 16) | (frame->raw[1] << 16) | frame->raw[2];

	int ret = decode_golay24(&coded_len);
	if (ret < 0) {
		// TODO: log the number of corrected bits?
		ap->diag->rx_fec_fail++;
		return SKY_RET_GOLAY_FAILED;
	}

	frame->length = coded_len & SKY_GOLAY_PAYLOAD_LENGTH_MASK;

	// Remove the length header from the rest of the data
	for (unsigned int i = 0; i < frame->length; i++)
		frame->raw[i] = frame->raw[i + 3];

	return sky_rx(ap, frame);
}


int sky_tx(SkyHandle_t self, SkyRadioFrame_t *frame, timestamp_t current_time)
{
	SKY_ASSERT(ap && frame);

	frame->length = 223; // TODO: check if this is used anywhere and place the constant somewhere
	if (ap_mac_tx(self, frame, current_time) < 0)
		return -1;

	if (1 /*ap->conf->authenticate_tx*/) {
		sky_authenticate(self, frame);
	}


	++self->diag->tx_frames;
	sky_fec_encode(frame);

	/*
	 * Decode length field.
	 * Remark: Return code ignored. It's always 0.
	 */
	uint32_t phy_header = frame->length | SKY_GOLAY_RS_ENABLED | SKY_GOLAY_RANDOMIZER_ENABLED;
	encode_golay24(&phy_header);

	//frame->

	return 0;
}



int sky_init_hmac(SkyHandle_t self, const char* key, unsigned int key_len) {

	if (self->hmac_ctx == NULL)
		self->hmac_ctx = SKY_MALLOC(SKY_HMAC_CTX_SIZE);

	cf_hmac_init(self->hmac_ctx, &cf_sha256, key, key_len);
}


int sky_authenticate(SkyHandle_t self, SkyRadioFrame_t* frame) {

	uint8_t full_hash[32];
	cf_hmac_update(self->hmac_ctx, frame->raw, frame->length);
	cf_hmac_finish(self->hmac_ctx, full_hash);

	// Copy truncated hash to the end of the frame
	memcpy(&frame->raw[frame->length], full_hash, SKY_HMAC_LENGTH);
	return SKY_RET_OK;
}




int sky_check_authentication(SkyHandle_t self, SkyRadioFrame_t* frame) {

	if (frame->length < SKY_HMAC_LENGTH)
		return -1; // SKY_RET_INVALID_FRAME_LENGTH


	// Check if the sequence number is something we are expecting
	if (0) {
		// TODO:
	}

	uint8_t calculated_hash[32];
	cf_hmac_update(self->hmac_ctx, frame->raw, frame->length - SKY_HMAC_LENGTH);
	cf_hmac_finish(self->hmac_ctx, calculated_hash);

	uint8_t *frame_hash = &frame->raw[frame->length - SKY_HMAC_LENGTH];

	if (memcmp(frame_hash, calculated_hash, SKY_HMAC_LENGTH) != 0) {
		/*

		 */
		return -1;
		// FAIL
	}
}



int sky_vc_write(SkyHandle_t self, SkyVirtualChannel_t vc, const uint8_t *data, unsigned datalen, unsigned flags) {
	//return sky_buf_write(ap1->txbuf[(cmd - CMD_WRITE_VC0)/2], data+1, len-1, BUF_FIRST_SEG|BUF_LAST_SEG);
	return 0;
}



int sky_print_diag(SkyHandle_t self)
{
	SKY_ASSERT(self);

	unsigned i;
	const struct ap_diag *diag = self->diag;
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "\033[H\033[2J");
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Received frames: %5u total, %5u OK, %5u failed. FEC corrected octets %5u/%u\n",
		diag->rx_frames, diag->rx_fec_ok, diag->rx_fec_fail, diag->rx_fec_errs, diag->rx_fec_octs);
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Transmit frames: %5u\n",
		diag->tx_frames);

	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Buffer fullness RX: ");
	for (i = 0; i < AP_RX_BUFS; i++)
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "%4u ", ap_buf_fullness(self->rxbuf[i]));
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "TX: ");
	for (i = 0; i < AP_TX_BUFS; i++)
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "%4u ", ap_buf_fullness(self->txbuf[i]));
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "\n");
	for (i = 0; i < AP_N_ARQS; i++) {
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "ARQ %u RX:\n", i);
		ap_arqrx_print(self->arqrx[i]);
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "\n");
	}
	for (i = 0; i < AP_N_ARQS; i++) {
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "ARQ %u TX:\n", i);
		ap_arqtx_print(self->arqtx[i]);
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "\n");
	}
	return 0;
}
