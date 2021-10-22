/*
 * Skylink Protocol: ARQ sublayer
 */

/*
3-octet reverse channel:
  Octet 0
   7-6:  Receiver status
          00=reserved
          01=normal operation
          10=receiver in reset state,
          11=receiver is confused
   5-0:  Latest contiguously received sequence number
  Octet 1,2
   Bitmap of successfully received frames in window

1-octet header:
   7-6:  Frame type or command.
          00=reserved
          01=normal frame from reliable stream
          10=reliable stream protocol reset command
          11=reserved
   5-0:  Sequence number
*/

#include "skylink/arq.h"
#include "skylink/diag.h"


#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>

//#include "platform/wrapmalloc.h"



/* -------------------------------------
 * Common things for sender and receiver
 * ------------------------------------- */

typedef uint8_t seq_t;
typedef uint16_t bitmap_t;
typedef uint8_t p_timer_t;
#define WINDOW_SIZE 0x10
#define WINDOW_MASK 0x0F
#define SEQ_MASK 0x3F

#define RETRANSMIT_TIMEOUT 10
#define FIXED_SDU_LEN

typedef enum { RX_STATUS_RESERVED=0, RX_STATUS_NORMAL=1, RX_STATUS_RESET=2, RX_STATUS_WTF=3, RX_STATUS_UNKNOWN=4 } rx_status_t;
static const char *rx_status_str[] = { "---", "OK ", "RST", "WTF", "???" };

static inline bool getbit(bitmap_t bitmap, seq_t window_offset)
{
	return bitmap & (1 << (16 - window_offset)) ? 1 : 0;
}

static inline bitmap_t setbit(bitmap_t bitmap, seq_t window_offset, bool value)
{
	if(value)
		return bitmap |  (1 << (16 - window_offset));
	else
		return bitmap & ~(1 << (16 - window_offset));
}


// To help with printing diagnostics
static void bitmap_to_str(bitmap_t b, char *str)
{
	unsigned i;
	for (i = 0; i < 16; i++) {
		*str++ = (b & 0x8000) ? '#' : '.';
		b <<= 1;
	}
	*str = '\0';
}

/* ---------------------------------------
 * Sender related structures and functions
 * --------------------------------------- */

enum tx_sdu_state { SDU_FREE, SDU_TRANSMITTED_ONCE, SDU_RETRANSMITTED, SDU_ACKED };

// Struct to store an SDU in transmit buffer
struct tx_sdu {
	enum tx_sdu_state state;
	seq_t seq;
	p_timer_t last_tx_time;
	size_t length;
	uint8_t data[AP_ARQ_SDU_LEN];
};


// State of transmitting end of an ARQ process
struct ap_arqtx {
	/* Received reverse channel: */
	rx_status_t rx_status;
	seq_t rx_acked;
	bitmap_t rx_bitmap;

	/* State */
	p_timer_t time_now;
	seq_t tx_window_first, tx_window_last;
	unsigned wtf;
	struct tx_sdu buf[WINDOW_SIZE];
};



int ap_arqtx_reset(struct ap_arqtx *self, const SkyARQConfig_t *conf)
{
	(void)conf; // not used now
	int i;
	self->rx_acked = 0;
	self->rx_bitmap = 0;
	self->rx_status = RX_STATUS_UNKNOWN;
	self->tx_window_first = 0;
	self->tx_window_last = 0;
	self->wtf = 0;
	for (i=0; i<WINDOW_SIZE; i++)
		self->buf[i].state = SDU_FREE;

	SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: reset transmitter\n");
	return 0;
}


static int find_next_sdu_to_transmit(struct ap_arqtx *, const ap_arq_sdu_tx_cb cb, void *const cb_arg);


int ap_arqtx_tx(struct ap_arqtx *state, const SkyARQConfig_t *conf, uint8_t *data, int maxlength, const ap_arq_sdu_tx_cb cb, void *const cb_arg)
{
	if(maxlength < 1 + AP_ARQ_SDU_LEN)
		return -1;

	state->time_now++;

	if (state->rx_status == RX_STATUS_UNKNOWN ||
	    state->rx_status == RX_STATUS_WTF ||
	    state->wtf > 0)
	{
		/* Receiver state unknown. Request protocol reset. */
		ap_arqtx_reset(state, conf);
		data[0] = 0x80;
		memset(data + 1, 0, AP_ARQ_SDU_LEN); // TODO variable length PDUs
		return 1 + AP_ARQ_SDU_LEN; // TODO variable length PDUs
	}

	int ret;
	ret = find_next_sdu_to_transmit(state, cb, cb_arg);
	if(ret >= 0) {
		seq_t seq = ret;
		struct tx_sdu *sdu = &state->buf[seq & WINDOW_MASK];
		if(sdu->seq != seq) {
			++state->wtf;
			SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: WTF: Wrong sequence number in transmit buffer (%d != %d)\n", sdu->seq, seq);
		}

		data[0] = 0x40 | (sdu->seq & SEQ_MASK);
		memcpy(data + 1, sdu->data, AP_ARQ_SDU_LEN);
		return 1 + AP_ARQ_SDU_LEN;
	} else {
		return -1;
	}
}


int ap_arqtx_rx_ack(struct ap_arqtx *state, const SkyARQConfig_t *conf, const uint8_t *data, int length)
{
	if (length != 3)
		return -1;
	/* Parse reverse channel */
	uint8_t d = data[0];
	state->rx_status = d >> 6;
	state->rx_acked = d & SEQ_MASK;
	state->rx_bitmap = ((bitmap_t)data[1] << 8) | data[2];
	return 0;
}


struct ap_arqtx *ap_arqtx_init(const SkyARQConfig_t *conf)
{
	struct ap_arqtx *self;
	self = calloc(1, sizeof(*self));
	ap_arqtx_reset(self, conf);
	return self;
}


static int get_sdu(struct ap_arqtx *self, uint8_t *data, size_t maxlength, const ap_arq_sdu_tx_cb cb, void *const cb_arg)
{
	int r = cb(cb_arg, data, maxlength);
	/* Again, this could be a flag for MUX instead.
	 * Or even better, implement variable length SDUs. */
#ifdef FIXED_SDU_LEN
	/* If MUX returns 0-size data, we don't need to transmit.
	 * (Yes this would also belong in MUX, nasty leakage of abstraction!) */
	if (r <= 0)
		return -1;
	if ((size_t)r < maxlength)
		memset(data + r, 0, maxlength - r);
#endif
	return r;
}


/* Decide which SDU to transmit next.
 *
 * This is where a lot of complex logic got concentrated.
 *
 * Return value is the sequence number to transmit next,
 *  -1 if nothing to transmit.
 */
static int find_next_sdu_to_transmit(struct ap_arqtx *self, const ap_arq_sdu_tx_cb cb, void *const cb_arg)
{
	unsigned i;
	p_timer_t time_now = self->time_now;
	seq_t seq_to_transmit;
	const bitmap_t rx_bitmap = self->rx_bitmap;
	const seq_t rx_acked = self->rx_acked;

	seq_t tx_window_first = self->tx_window_first, tx_window_last = self->tx_window_last;


	/* Free transmit window up to latest contiguously received SDU */
	unsigned n_acked = (rx_acked+1 - tx_window_first) & SEQ_MASK;
	if(n_acked >= WINDOW_SIZE) {
		/* Should not happen in normal operation.
		 * Probably receiver and transmitter states are not synchronized. */
		n_acked = WINDOW_SIZE; // what should actually be done?
		++self->wtf;
		SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: WTF: Acked more than WINDOW_SIZE.\n");
	}

	for(i=0; i<n_acked; i++) {
		seq_t seq = (tx_window_first + i) & SEQ_MASK;
		struct tx_sdu *sdu = &self->buf[seq & WINDOW_MASK];
		sdu->state = SDU_FREE;
	}

	tx_window_first = rx_acked+1;



	/* Find the latest successfully received SDU,
	 * indicated by the last 1 in the bitmap. */
	seq_t rx_latest = rx_acked;
	for(i=WINDOW_SIZE-1; i >= 1; i--) {
		if(getbit(rx_bitmap, i)) {
			rx_latest = (rx_acked + i) & SEQ_MASK;
			break;
		}
	}
	SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: Receive window as seen by transmitter: %2d...%2d, %04x, %d\n", rx_acked, rx_latest, rx_bitmap, self->rx_status);

	seq_t rx_window_fullness = (rx_latest - rx_acked) & SEQ_MASK;

	/* Check if SDUs in between have been lost, starting from the earliest one */
	for(i=1; i < rx_window_fullness; i++) {
		seq_t seq = (rx_acked + i) & SEQ_MASK;
		struct tx_sdu *sdu = &self->buf[seq & WINDOW_MASK];

		if (getbit(rx_bitmap, i) == 0) {
			/* If the SDU has been transmitted only once, retransmit immediately.
			 * Otherwise retransmit if its timer has expired. */
			switch(sdu->state) {
			case SDU_TRANSMITTED_ONCE:
				sdu->state = SDU_RETRANSMITTED;
				sdu->last_tx_time = time_now;
				seq_to_transmit = seq;
				goto seq_chosen;

			case SDU_RETRANSMITTED: {
				p_timer_t timediff = time_now - sdu->last_tx_time;
				if(timediff >= RETRANSMIT_TIMEOUT) {
					SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: Timeout retransmit %02d after %d\n", seq, timediff);
					sdu->last_tx_time = time_now;
					seq_to_transmit = seq;
					goto seq_chosen;
				}
				/* Else, go find the next one to retransmit */
				break;}

			default:
				/* Should not happen in normal operation.
				 * Probably receiver and transmitter states are not synchronized. */
				++self->wtf;
				SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: WTF: Tried to retransmit a freed SDU.\n");
			}
		} else {
			sdu->state = SDU_ACKED;
		}
	}

	/* If the code gets here, nothing was found to retransmit.
	 * If there is space in window, get the next SDU. */

	seq_t tx_window_fullness = (tx_window_last+1 - tx_window_first) & SEQ_MASK;

	if(tx_window_fullness < WINDOW_SIZE /*-1*/) { // is -1 needed?
		/* Get the next sequence number after end of the current window */
		seq_t seq = (tx_window_last + 1) & SEQ_MASK;
		struct tx_sdu *sdu = &self->buf[seq & WINDOW_MASK];

		if(sdu->state != SDU_FREE) {
			SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: TX buffer wasn't free. Bug?\n");
		}

		int ret = get_sdu(self, sdu->data, AP_ARQ_SDU_LEN, cb, cb_arg);
		if(ret >= 0) {
			sdu->state = SDU_TRANSMITTED_ONCE;
			sdu->seq = seq;
			sdu->last_tx_time = time_now;
			tx_window_last = seq;
			seq_to_transmit = seq;
			goto seq_chosen;
		} else {
			/* Check for timeout of packets in TX window also here.
			 * Otherwise it may get stuck never retransmitting the last
			 * lost packets until more packets have been sent.
			 * Yes, the function starts to get even more complex now.
			 * There's starting to be duplicated code,
			 * but the alternative would have been another goto
			 * and some additional variables.
			 *
			 * TODO (maybe):
			 * Maybe next step would be rewriting the whole function
			 * in some different way.
			 * The whole timeout system could also be somehow different,
			 * since now packets can still get delayed quite much in
			 * situations where retransmission happens due to a timeout.
			 * Or is the timeout just too long?
			 */
			for (i = 0; i < tx_window_fullness; i++) {
				seq_t seq = (tx_window_first + i) & SEQ_MASK;
				struct tx_sdu *sdu = &self->buf[seq & WINDOW_MASK];
				// not sure if this if is necessary
				if (sdu->state == SDU_TRANSMITTED_ONCE) {
					p_timer_t age = time_now - sdu->last_tx_time;
					if (age >= RETRANSMIT_TIMEOUT) {
						SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: Timeout retransmit hack\n");
						sdu->last_tx_time = time_now;
						seq_to_transmit = seq;
						goto seq_chosen;
					}
				}
			}
			goto nothing_to_transmit;
		}
	} else {
		SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: Full transmit window\n");
		/* Window was full, indicating high packet loss.
		 * Retransmit the oldest packet to avoid getting stuck here. */
		p_timer_t oldest_age = 0;
		seq_t oldest_seq = 0;
		/* Hmm.. Why have I replaced WINDOW_SIZE with 8?
		 * Can't remember... And should it start from 0 and not 1? */
		for(i = 1; i < /*WINDOW_SIZE*/ 8; i++) {
			seq_t seq = (tx_window_first + i) & SEQ_MASK;
			struct tx_sdu *sdu = &self->buf[seq & WINDOW_MASK];

			p_timer_t age = time_now - sdu->last_tx_time;
			if(age > oldest_age) {
				oldest_age = age;
				oldest_seq = seq;
			}
		}
		if(oldest_age > 0) {
			struct tx_sdu *sdu = &self->buf[oldest_seq & WINDOW_MASK];
			sdu->state = SDU_RETRANSMITTED;
			sdu->last_tx_time = time_now;
			seq_to_transmit = oldest_seq;
			goto seq_chosen;
		}

		goto nothing_to_transmit;
	}

	SKY_PRINTF(SKY_DIAG_DEBUG, "Hmm, should not get here\n");


	int retval;

nothing_to_transmit:
	retval = -1;
	goto done;

seq_chosen:
	retval = seq_to_transmit;

done:
	self->tx_window_first = tx_window_first;
	self->tx_window_last  = tx_window_last;
	return retval;
}





int ap_arqtx_print(struct ap_arqtx *state)
{
	char str[18];
	unsigned wf = state->tx_window_first, wl = state->tx_window_last;

	unsigned i, w = wf;
	for (i = 0; i < 17; i++) {
		if (w == ((wl + 1) & SEQ_MASK))
			break;
		str[i] = ".1R#"[state->buf[w & WINDOW_MASK].state];
		w = (w + 1) & SEQ_MASK;
	}
	for (;i < 17; i++)
		str[i] = ' ';
	str[i] = '\0';
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Window %2u to %2u: %s \n", wf, wl, str);

	bitmap_to_str(state->rx_bitmap, str);
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "Latest ACK   %2u: %s  %s \n", state->rx_acked, str, rx_status_str[state->rx_status]);
	return 0;
}








/* -----------------------------------------
 * Receiver related structures and functions
 * ----------------------------------------- */


enum rx_sdu_state { RXSDU_FREE, RXSDU_RECEIVED };

struct rx_sdu {
	enum rx_sdu_state state;
	seq_t seq;
	size_t length;
	uint8_t data[AP_ARQ_SDU_LEN];
};


struct ap_arqrx {
	rx_status_t status;
	seq_t acked;
	bitmap_t bitmap;
	unsigned wtf;

	struct rx_sdu buf[WINDOW_SIZE];
};


int ap_arqrx_reset(struct ap_arqrx *self, const SkyARQConfig_t *conf)
{
	int i;
	self->status = RX_STATUS_RESET;
	self->acked = 0;
	self->bitmap = 0;
	self->wtf = 0;
	for (i=0; i<WINDOW_SIZE; i++)
		self->buf[i].state = RXSDU_FREE;

	SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: reset receiver\n");
	return 0;
}


static int sr_rx(struct ap_arqrx *self, const uint8_t *data, size_t length, seq_t rxseq, const ap_arq_sdu_rx_cb cb, void *const cb_arg);

int ap_arqrx_rx(struct ap_arqrx *state, const SkyARQConfig_t *conf, const uint8_t *data, int length, const ap_arq_sdu_rx_cb cb, void *const cb_arg)
{
	if(length != 1 + AP_ARQ_SDU_LEN)
		return 0;

	/* Parse header */
	uint8_t d = data[0];
	switch(d & 0xC0) {
	case 0x00: /* Reserved */
		break;
	case 0x40: /* Normal frame from reliable stream */
		sr_rx(state, data+1, length-1, d & SEQ_MASK, cb, cb_arg);
		break;
	case 0x80: /* Receiver reset command */
		ap_arqrx_reset(state, conf);
		break;
	default:   /* Reserved */
		break;
	}

	return 0;
}


static int sr_rx(struct ap_arqrx *self, const uint8_t *data, size_t length, seq_t rxseq, const ap_arq_sdu_rx_cb cb, void *const cb_arg)
{
	seq_t acked = self->acked;
	bitmap_t bitmap = self->bitmap;

	if (self->status == RX_STATUS_WTF) {
		SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: RX in WTF state, discarding\n");
		goto done;
	}

	seq_t seqdiff = (rxseq - acked) & SEQ_MASK;
	if(seqdiff <= 0 || seqdiff > WINDOW_SIZE) { // is this the right range?
		/* This may happen in normal operation if an old frame
		 * gets retransmitted, so don't go WTF. */
		/*++self->wtf;*/
		SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: RX not within window, discarding\n");
		goto done;
	}

	/* Put data in buffer and mark as received in bitmap */
	struct rx_sdu *sdu = &self->buf[rxseq & WINDOW_MASK];
	sdu->state = RXSDU_RECEIVED;
	sdu->seq = rxseq;
	sdu->length = length;
	memcpy(sdu->data, data, AP_ARQ_SDU_LEN);

	bitmap = setbit(bitmap, seqdiff, 1);

	/* Get all contiguous data from last acked one */
	unsigned i;
	for(i=1; i<WINDOW_SIZE; i++) {
		if(getbit(bitmap, 1)) {
			/* Yes: output it, free it and ACK it */
			seq_t seq = (acked + 1) & SEQ_MASK;
			struct rx_sdu *sdu = &self->buf[seq & WINDOW_MASK];

			if(sdu->seq != seq) {
				/* This somehow happens when receiver and transmitter
				 * are not synchronized.
				 * TODO: find out if this may happen in the case
				 * or is there an actual bug somewhere
				 */
				SKY_PRINTF(SKY_DIAG_DEBUG, "ARQ: WTF: Wrong sequence number in RX buffer.\n");
				++self->wtf;
			}

			cb(cb_arg, sdu->data, sdu->length);
			// TODO: maybe check whether callback accepted it, to implement flow control
			sdu->state = RXSDU_FREE;

			/* Increment acked by one and shift bitmap accordingly */
			acked = seq;
			bitmap <<= 1;
		} else {
			/* No more, stop here */
			break;
		}
	}


done:
	self->acked = acked;
	self->bitmap = bitmap;
	if (self->wtf > 0)
		self->status = RX_STATUS_WTF;
	else
		self->status = RX_STATUS_NORMAL;
	return 0;
}


int ap_arqrx_tx_ack(struct ap_arqrx *state, const SkyARQConfig_t *conf, uint8_t *data, int maxlen)
{
	data[0] = state->acked;
	data[1] = state->bitmap >> 8;
	data[2] = state->bitmap;
	return 3;
}


struct ap_arqrx *ap_arqrx_init(const SkyARQConfig_t *conf)
{
	struct ap_arqrx *self;
	self = calloc(1, sizeof(*self));
	ap_arqrx_reset(self, conf);
	return self;
}


int ap_arqrx_print(struct ap_arqrx *state)
{
	char str[17];
	bitmap_to_str(state->bitmap, str);
	SKY_PRINTF(SKY_DIAG_LINK_STATE, "RX Window    %2u: %s  %s \n", state->acked, str, rx_status_str[state->status]);
	return 0;
}
