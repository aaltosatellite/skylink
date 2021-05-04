#ifndef __MODEM_H__
#define __MODEM_H__


void modem_init();
void modem_wait_for_sync();
int modem_tx();
int modem_rx();

int tick();

#endif /* __MODEM_H__ */
