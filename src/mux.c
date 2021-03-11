/*
 * Skylink Protocol: Multiplexing sublayer
 *
 * TODO: implement fragmentation and everything
 */

#include "skylink/skylink.h"
#include "skylink/diag.h"
#include <assert.h>

#define N_VC 15

/*
 * Originally, the plan was to implement packet fragmentation and reassembly
 * as a part of the MUX implementation and store the fragments here.
 * Fragmentation is now, however, done in the packet buffers, so there's no
 * state to store here. Removing the whole MUX state struct was then
 * considered, but it's now kept here as a placeholder, since implementing
 * prioritization and scheduling of virtual channels might need storage of
 * some state at least at the transmitting end.
 */
struct ap_mux {

};



/* Struct to pass more parameters to that stupid callback wrapper below */
struct arqmux_cb_arg {
	struct ap_all *ap;
	struct ap_mux_conf *umux_conf;
};

/* Now that the ARQ data structure has been "cleaned up" to not contain things
 * not really related to actual ARQ process state, it seems that the mess was
 * just moved to a different place: now we need to pass more things in every
 * function call, and to keep them somehow in control, end up needing these
 * stupid callback wrapper functions. :|
 * (Is this why closures exist in higher level languages?)
 */
static int arqmux_rx_cb(void *arg, const uint8_t *data, int datalen)
{
	struct arqmux_cb_arg *p = arg;
	return ap_mux_rx(p->ap, p->umux_conf, data, datalen);
}


static int arqmux_tx_cb(void *arg, uint8_t *data, int maxlen)
{
	struct arqmux_cb_arg *p = arg;
	return ap_mux_tx(p->ap, p->umux_conf, data, maxlen);
}


static int fragment_rx(struct ap_all *ap, const uint8_t *data, int fraglen, uint8_t header)
{
	SKY_ASSERT(ap && data);
	int vcn = header >> 3; // channel number
	/*
	 * VC 0: zero padding
	 * VC 1-3: unbuffered channels (e.g. beacon, packet repeater)
	 * VC 4-7: buffered packet streams 0-3
	 * VC 8-11: ARQ processes 0-3
	 * VC 12-15: ARQ processes 0-3 reverse channels (TODO)
	 */
	if (vcn >= 4 && vcn <= 7) {
		return ap_buf_write(ap->rxbuf[vcn-4], data, fraglen, (3 & (header >> 1)) ^ 3);
	}
	else if (vcn >= 8 && vcn < 8 + AP_N_ARQS) {
		const int arqn = vcn - 8;
		return ap_arqrx_rx(ap->arqrx[arqn], &ap->conf->arqrx_conf, data, fraglen,
			arqmux_rx_cb, &(struct arqmux_cb_arg){
			ap, &ap->conf->umux_conf[arqn] });
	}
	else if (vcn >= 12 && vcn < 12 + AP_N_ARQS) {
		const int arqn = vcn - 12;
		return ap_arqtx_rx_ack(ap->arqtx[arqn], &ap->conf->arqtx_conf, data, fraglen);
	}
	else return 0;
}


static int fragment_tx(struct ap_all *ap, const struct ap_mux_conf *conf, uint8_t *data, int maxlen, int vcn, uint8_t *header)
{
	unsigned flags;
	int ret = -1;
	if ((conf->enabled_channels & (1 << vcn)) == 0)
		return -1;
	else if (vcn >= 4 && vcn <= 7) {
		ret = ap_buf_read(ap->txbuf[vcn - 4], data, maxlen, &flags);
		*header = ((3 & flags) ^ 3) << 1;
	}
	else if (vcn >= 8 && vcn < 8 + AP_N_ARQS) {
		const int arqn = vcn - 8;
		ret = ap_arqtx_tx(ap->arqtx[arqn], &ap->conf->arqtx_conf, data, maxlen,
			arqmux_tx_cb, &(struct arqmux_cb_arg){
			ap, &ap->conf->umux_conf[arqn] });
		*header = 0;
	}
	else if (vcn >= 12 && vcn < 12 + AP_N_ARQS) {
		const int arqn = vcn - 12;
		ret = ap_arqrx_tx_ack(ap->arqrx[arqn], &ap->conf->arqrx_conf, data, maxlen);
		*header = 0;
	}
	return ret;
}


int ap_mux_rx(struct ap_all *ap, const struct ap_mux_conf *conf, const uint8_t *data, int datalen)
{
	/* Parse a MUX PDU */
	int p;
	for (p = 0; p < datalen;) {
		uint8_t header = data[p];
		++p;
		int fraglen;
		if (header & 1) {
			/* Next byte indicates fragment length */
			if (p >= datalen)
				break;
			fraglen = data[p];
			++p;
			if (p + fraglen > datalen)
				break;
		} else {
			/* Fragment continues until end of PDU.
			 * TODO: maximum length for each channel also */
			fraglen = datalen - p;
		}
		fragment_rx(ap, data + p, fraglen, header);
		p += fraglen;
	}
	return 0;
}


int ap_mux_tx(struct ap_all *ap, const struct ap_mux_conf *conf, uint8_t *data, int maxlen)
{
	int p;
	int vci = 0;
	char no_data[N_VC] = { 0 };
	int tries = 0;
	for (p = 0; p < maxlen - 2;) {
		if (no_data[vci]) {
			/* ugly and messy logic, try to redo it somehow someday... */
			++tries;
			if (tries >= N_VC)
				break;
		} else {
			unsigned vcn = vci + 1;
			uint8_t header = 0;
			int rr = fragment_tx(ap, conf, data + p + 2, maxlen - p - 2, vcn, &header);
			if (rr >= 0) {
				// add header
				data[p] = header | (vcn << 3) | 1;
				data[p+1] = rr;
				p += 2 + rr;
			} else {
				no_data[vci] = 1;
			}
		}
		assert(p <= maxlen); // for development/debugging

		/* TODO: more proper prioritization, etc */
		vci = (vci + 1) % N_VC;
	}
	return p;
}


int ap_mux_reset(struct ap_mux *self)
{
	return 0;
}
