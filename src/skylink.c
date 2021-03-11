#include "skylink/skylink.h"
#include "skylink/diag.h"

#include "platform/wrapmalloc.h"


unsigned int sky_diag_mask = SKY_DIAG_LINK_STATE | SKY_DIAG_BUG;

/* Initialization */

struct ap_all* sky_init(struct ap_all *ap, struct ap_conf *conf)
{
	SKY_ASSERT(ap && conf);

	ap->conf = conf;
	ap->diag = calloc(1, sizeof(*ap->diag));

	ap->arqtx[0] = ap_arqtx_init(&conf->arqtx_conf);
	ap->arqrx[0] = ap_arqrx_init(&conf->arqrx_conf);

	ap->mac = ap_mac_init(ap);

	conf->umux_conf[0].enabled_channels = 0b000001110000;
	conf->lmux_conf.enabled_channels = 0b0001000110001110;

	return ap;
}


int sky_rx(struct ap_all *ap, struct radioframe *frame)
{
	SKY_ASSERT(ap && frame);

	int ret = sky_fec_decode(frame, ap->diag);
	if (ret >= 0)
		return ap_mac_rx(ap, frame);
	else
		return -1;
}


int sky_tx(struct ap_all *ap, struct radioframe *frame, timestamp_t current_time)
{
	SKY_ASSERT(ap && frame);

	frame->length = 223; // TODO: check if this is used anywhere and place the constant somewhere
	if (ap_mac_tx(ap, frame, current_time) >= 0) {
		++ap->diag->tx_frames;
		sky_fec_encode(frame);
		return 0;
	} else {
		return -1;
	}
}


int sky_print_diag(struct ap_all *ap)
{
	SKY_ASSERT(ap);

	unsigned i;
	const struct ap_diag *diag = ap->diag;
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "\033[H\033[2J");
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Received frames: %5u total, %5u OK, %5u failed. FEC corrected octets %5u/%u\n",
		diag->rx_frames, diag->rx_fec_ok, diag->rx_fec_fail, diag->rx_fec_errs, diag->rx_fec_octs);
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Transmit frames: %5u\n",
		diag->tx_frames);

	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Buffer fullness RX: ");
	for (i = 0; i < AP_RX_BUFS; i++)
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "%4u ", ap_buf_fullness(ap->rxbuf[i]));
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "TX: ");
	for (i = 0; i < AP_TX_BUFS; i++)
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "%4u ", ap_buf_fullness(ap->txbuf[i]));
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "\n");
	for (i = 0; i < AP_N_ARQS; i++) {
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "ARQ %u RX:\n", i);
		ap_arqrx_print(ap->arqrx[i]);
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "\n");
	}
	for (i = 0; i < AP_N_ARQS; i++) {
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "ARQ %u TX:\n", i);
		ap_arqtx_print(ap->arqtx[i]);
		SKY_PRINTF(SKY_DIAG_LINK_STATE, "\n");
	}
	return 0;
}
