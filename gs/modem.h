#ifndef __MODEM_H__
#define __MODEM_H__

#include "skylink/skylink.h"
#include "skylink/platform.h"
#include "../platforms/posix/timestamp.h"

/*
 * Initialize modem (interface)
 */
void modem_init();
void modem_wait_for_sync();

/*
 * Transmit a frame.
 *
 * Args:
 */
int modem_tx(SkyRadioFrame* frame, timestamp_t t);

/*
 * Wait for frames from the modem.
 * The function will block till a tick message or new incoming frame is received.
 *
 * Args:
 *    frame: Frame object in which the incoming frame will be stored.
 *    flags: Flags to control zmq receiving.
 *
 * Returns:
 *    1 if a frame was successfully received.
 *    0 if no new frames were available.
 *    <0 if an error occurred.
 */
int modem_rx(SkyRadioFrame* frame, int flags);

/*
 * Returns 1(true) if modem has sensed carrier and locked started receiving a frame.
 */
int modem_carrier_sensed();

/*
 * Returns 1 if the modem is currently transmitting.
 */
int modem_can_send();

/*
 * Returns 1 if a tick event has been received.
 */
int tick();

#endif /* __MODEM_H__ */
