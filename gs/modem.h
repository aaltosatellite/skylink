#ifndef __MODEM_H__
#define __MODEM_H__

/*
 * Initialize modem (interface)
 */
void modem_init();
void modem_wait_for_sync();

/*
 * Transmit a frame.
 */
int modem_tx(SkyRadioFrame* frame, timestamp_t t);

/*
 * Receive a frame
 */
int modem_rx(SkyRadioFrame* frame, int flags);

int modem_tx_active();
int tick();

#endif /* __MODEM_H__ */
