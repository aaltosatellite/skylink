#include "skylink.h"
#include "diag.h"
#include "fec.h"
#include "hmac.h"
#include "mac_2.h"
#include "endian.h"
#include "platform.h"
#include "arq_ring.h"
#include "skypacket.h"

#include <stdlib.h>
#include <string.h>



unsigned int sky_diag_mask = SKY_DIAG_LINK_STATE | SKY_DIAG_BUG | SKY_DIAG_FRAMES | SKY_DIAG_DEBUG | SKY_DIAG_INFO;

/** Initialization */
SkyHandle sky_init(SkyHandle self, SkyConfig_t *conf)
{
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


int sky_rx(SkyHandle self, SkyRadioFrame *frame)
{
	int ret;

	// Decode packet
	if((ret = decode_skylink_packet(frame)) < 0){
		return ret;
	}

	// Check authentication code if the frame claims it is authenticated.
	if (sky_hmac_frame_claims_authenticated(frame)) {
		if ((ret = sky_hmac_check_authentication(self, frame)) < 0)
			return ret;
	}

	// If the virtual channel necessitates auth, return error.
	if(sky_hmac_vc_demands_auth(self, frame->vc) && (!(frame->flags & SKY_FLAG_FRAME_AUTHENTICATED))){
		return SKY_RET_AUTH_MISSING;
	}

	// Update MAC status
	if((frame->flags & SKY_FLAG_FRAME_AUTHENTICATED) || self->conf.mac.unauth_mac_updates){
		mac_update_belief(self->mac, &self->conf->mac, frame->timestamp_ms, frame->mac_length, frame->mac_left);
	}

	int r = -1;
	if(! (self->conf.vc[frame.vc].arq_on)){
		r = skyArray_push_rx_packet_monotonic(self->arrayBuffers[frame.vc], frame->payload, frame->payload_length);
	}

	if(self->conf.vc[frame.vc].arq_on){
		if(frame->arq_sequence == 0){
			return SKY_RET_NO_MAC_SEQUENCE;
		}
		r = skyArray_push_rx_packet(self->arrayBuffers[frame.vc], frame->payload, frame->payload_length, frame->arq_sequence);
	}

	//todo: log behavior based on r.
	return 0;
}


int sky_rx_raw(SkyHandle self, SkyRadioFrame *frame)
{
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


	// Decode FEC
	if ((ret = sky_fec_decode(frame, self->diag)) < 0)
		return ret;

	return sky_rx(self, frame);
}


int tx_loop(SkyHandle self, SkyRadioFrame *frame, int32_t now_ms){
	if(mac_own_window_remaining(self->mac, now_ms) < 0){
		return 0;
	}
	for (int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i) {
		int vc = self->conf.vc.priority_order[i];
		SkyArqRing* arqRing = self->arrayBuffers[vc];
		if(skyArray_count_packets_to_tx(array) == 0){ //todo: or, there might be extensions to send.
			continue;
		}
		int seq = 0;
		int leng = skyArray_get_legth_of_next_tx_payload(arqRing);
		skyArray_read_packet_for_tx(array, frame->raw+SKY_FRAME_MAX_LEN-leng, &seq);
		//todo: add extensions
		encode_skylink_packet(self, frame);
		//todo: physical send.
	}


}



int sky_tx(SkyHandle self, SkyRadioFrame *frame, timestamp_t current_time)
{
	int loc = 0;

	// Reserve space for the MAC header
	if (0)
		loc += 2;

	//if (0 && ap_mac_tx(self, frame, current_time) < 0)
	//	return -1;


	int flags;

	//int ret = sky_buf_read(self->txbuf[0], &frame->payload[loc], 200, &flags);
	//if (ret < 0)
	//	return -1;


	//frame->length = ret;

	//frame->hdr.flags = 0;
	//frame->hdr.apid = 0;

	//frame->phy.length = 0;

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



int sky_tx_raw(SkyHandle ap, SkyRadioFrame *frame, timestamp_t current_time) {

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




int sky_set_config(SkyHandle self, unsigned int cfg, unsigned int val) {
	SKY_ASSERT(self)
	(void)self; (void)cfg; (void)val;
	return -1;
}


int sky_get_config(SkyHandle self, unsigned int cfg, unsigned int* val) {
	SKY_ASSERT(self)
	(void)self; (void)cfg; (void)val;
	return -1;
}



int sky_print_diag(SkyHandle self)
{
	SKY_ASSERT(self)

	unsigned i;
	const SkyDiagnostics_t *diag = self->diag;
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "\033[H\033[2J")
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Received frames: %5u total, %5u OK, %5u failed. FEC corrected octets %5u/%u\n",
		diag->rx_frames, diag->rx_fec_ok, diag->rx_fec_fail, diag->rx_fec_errs, diag->rx_fec_octs)
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Transmit frames: %5u\n",
		diag->tx_frames)

	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Buffer fullness RX: ")
	//for (i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++)
		//SKY_PRINTF(SKY_DIAG_LINK_STATE, "%4u ", ap_buf_fullness(self->rxbuf[i]))
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "TX: ")
	//for (i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; i++)
	//	SKY_PRINTF(SKY_DIAG_LINK_STATE, "%4u ", ap_buf_fullness(self->txbuf[i]))
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "\n")

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

		SKY_PRINTF(SKY_DIAG_DEBUG, "%s ", str)

		data++;
		data_len--;
	}
	SKY_PRINTF(SKY_DIAG_DEBUG, "\n")
}


int sky_clear_stats(SkyHandle self) {
	memset(self->diag, 0, sizeof(SkyDiagnostics_t));
	return 0;
}


int sky_get_buffer_status(SkyHandle self, SkyBufferState_t* state) {
	SKY_ASSERT(self && state)

	for (int vc = 0; vc < SKY_NUM_VIRTUAL_CHANNELS; vc++) {
		state->state[vc] = 0;
		//state->rx_free[vc] = sky_buf_fullness(self->rxbuf[vc]);
		//state->tx_avail[vc] = sky_buf_space(self->rxbuf[vc]);
	}

	return 0;
}


int sky_flush_buffers(SkyHandle self) {
	SKY_ASSERT(self)
	for (int vc = 0; vc < 4; vc++) {
		//sky_buf_flush(self->rxbuf[vc]);
		//sky_buf_flush(self->txbuf[vc]);
	}
}


