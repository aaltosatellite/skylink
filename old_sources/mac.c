/*
 * Skylink Protocol: Medium Access Control layer
 *
 * Handles time division duplexing.
 *
 * It has a bit complex logic and one could try to write it
 * more clearly some day, but it seems to work...
 *
 * TODO: Make TDD timing configurable at runtime.
 * Check the logic to make sure timing works right (i.e. without collisions)
 * in all cases.
 */

#include "skylink/skylink.h"
#include "skylink/diag.h"
//#include "platform/wrapmalloc.h"
#include <string.h>

#define TDD_PERIOD (TIMESTAMP_MS*1000)
#define TDD_M2S (TIMESTAMP_MS*500)
#define TDD_FRAME (TIMESTAMP_MS*230)
#define TDD_FRAME_PREPARATION_TIME (TIMESTAMP_MS*5)

#define TDD_SYNC_TIMEOUT_CYCLES 3


// Valid values for the header octet
//#define MAC_HEADER_RESERVED  0x40
#define MAC_HEADER_S2M       0x56
#define MAC_HEADER_M2S_FIRST 0x69
#define MAC_HEADER_M2S_OTHER 0x7F


// State of the MAC/TDD sublayer
typedef struct ap_mac {

	/* Time when the TDD cycle/window started */
	timestamp_t tdd_cycle_start;

	/* */
	timestamp_t tdd_next_frame;

	unsigned tdd_cycles_since_sync, tdd_frame_number;

	/* Time when the TDD windows size was adjusted last time. */
	timestamp_t last_window_adjust;
} SkyMAC_t;




int eval_window_adjustment_logic(int self, timestamp_t now) {

	//if (all->mac->last_window_adjust > )

}

/*

static int sky_mac_postpone_tx(timestamp_t now, unsigned int duration) {
	timestamp new_cycle_start = now + tdd_cycle_start;
	if (new_cycle_start > tdd_cycle_start)
		ap->mac->tdd_cycle_start = new_cycle_start;
}

void sky_carrier_sensed(timestamp_t now) {
	sky_mac_postpone_tx(now, ap)
}
*/



int ap_mac_rx(struct ap_all *ap, SkyRadioFrame_t *frame)
{
	SKY_ASSERT(ap && frame);

#if 0
	ap_mux_rx(ap, &ap->conf->lmux_conf, frame->raw+1, frame->length-1);


	struct ap_mac *self = ap->mac;
	char is_tdd_slave = ap->conf->tdd_slave;
	if (frame->length < 1)
		return -1;

	/* Check protocol identifier and direction */
	char valid = 0;
	switch (frame->data[0]) {
	case MAC_HEADER_S2M:
		if (!is_tdd_slave)
			valid = 1;
		break;
	case MAC_HEADER_M2S_FIRST:
		if (is_tdd_slave) {
			valid = 1;
			// Synchronize:
			self->tdd_cycle_start = frame->timestamp;
			self->tdd_next_frame = self->tdd_cycle_start + TDD_M2S - TDD_FRAME_PREPARATION_TIME;
			self->tdd_frame_number = 0;
			self->tdd_cycles_since_sync = 0;
		}
		break;
	case MAC_HEADER_M2S_OTHER:
		if (is_tdd_slave)
			valid = 1;
		break;
	default:
		// Invalid header
		break;
	};

#endif


	return 0;
}


int ap_mac_tx(SkyHandle_t *self, SkyRadioFrame_t *frame, timestamp_t current_time)
{
	SKY_ASSERT(self && frame);

	SkyMAC_t* mac = self->mac;

	struct ap_mac *self = self->mac;
	char is_tdd_slave = self->conf->tdd_slave;
	char transmit_ok = 1;
	int retval = -1;
	timediff_t td;

#if 0
	td = current_time - mac->tdd_cycle_start;
	if (td >= TDD_PERIOD) {
		//SKY_PRINTF(SKY_DIAG_DEBUG, "TDD: %10u: Wrapped TDD timer\n", (unsigned)current_time);
		self->tdd_cycle_start += TDD_PERIOD;

		if (is_tdd_slave)
			self->tdd_next_frame = mac->tdd_cycle_start + TDD_M2S - TDD_FRAME_PREPARATION_TIME;
		else
			self->tdd_next_frame = mac->tdd_cycle_start;

		self->tdd_frame_number = 0;
		++self->tdd_cycles_since_sync;
	}

	/* Don't transmit if transmit slot
	 * would end during the next frame */
	td = self->tdd_next_frame - self->tdd_cycle_start;
	if (!is_tdd_slave) {
		if (td >= TDD_M2S - TDD_FRAME)
			transmit_ok = 0;
	} else {
		if (td >= TDD_PERIOD - TDD_FRAME - TDD_FRAME_PREPARATION_TIME)
			transmit_ok = 0;
	}
#endif


	td = current_time - mac->tdd_next_frame;
	if (transmit_ok && td >= 0) {
		//SKY_PRINTF(SKY_DIAG_DEBUG, "TDD: %10u: Frame transmit time (td=%d)\n", current_time, td);

		/* Time to transmit a frame. Slave doesn't transmit anything
		 * if it hasn't been synchronized recently enough. */
		if (!is_tdd_slave ||
		    self->tdd_cycles_since_sync < TDD_SYNC_TIMEOUT_CYCLES)
		{

			int ret;

			// BLAH ugly call again
			ret = -1;// ap_mux_tx(ap, &ap->conf->lmux_conf, frame->raw+1, frame->length-1);
			if (ret >= 0) {
				/* Yes, there is a frame to transmit.
				 * Add header octet. */
				uint8_t header;
				if (is_tdd_slave)
					header = MAC_HEADER_S2M;
				else if (self->tdd_frame_number == 0)
					header = MAC_HEADER_M2S_FIRST;
				else
					header = MAC_HEADER_M2S_OTHER;
				frame->raw[0] = header;
				frame->length = ret+1;
				retval = 1;
			}
		}

		/* Transmit timestamp is not currently supported.
		 * When used, it should be set a bit in future
		 * since generating the frame takes some time. */
		frame->timestamp = mac->tdd_next_frame + TDD_FRAME_PREPARATION_TIME;

		self->tdd_next_frame += TDD_FRAME;
		++self->tdd_frame_number;
	}
	return retval;
}


SkyMAC_t* sky_mac_init(SkyHandle_t *self/*, const struct ap_conf *conf*/)
{
	SKY_ASSERT(self);

	SkyMACConfig_t *conf = self->conf;
	SkyMAC_t* mac = calloc(1, sizeof(SkyMAC_t));

	/* Trick the timer to wrap once on next call to l1_tx */
	mac->tdd_cycle_start = conf->initial_time - TDD_PERIOD;
	mac->tdd_cycles_since_sync = 1000;

	return mac;
}
