#ifndef __VC_INTERFACE_H__
#define __VC_INTERFACE_H__

#include <stdbool.h>

#include "skylink/platform.h"

/*
 * List of control message command/response types
 */
#define VC_CTRL_TRANSMIT_VC0        0
#define VC_CTRL_TRANSMIT_VC1        1
#define VC_CTRL_TRANSMIT_VC2        2
#define VC_CTRL_TRANSMIT_VC3        3

#define VC_CTRL_RECEIVE_VC0         4
#define VC_CTRL_RECEIVE_VC1         5
#define VC_CTRL_RECEIVE_VC2         6
#define VC_CTRL_RECEIVE_VC3         7


#define VC_CTRL_GET_STATE           10
#define VC_CTRL_STATE_RSP           11
#define VC_CTRL_FLUSH_BUFFERS       12

#define VC_CTRL_GET_STATS           13
#define VC_CTRL_STATS_RSP           14
#define VC_CTRL_CLEAR_STATS         15

#define VC_CTRL_SET_CONFIG          16
#define VC_CTRL_GET_CONFIG          17
#define VC_CTRL_CONFIG_RSP          18

#define VC_CTRL_ARQ_CONNECT         20
#define VC_CTRL_ARQ_DISCONNECT      21
#define VC_CTRL_ARQ_TIMEOUT         22



int vc_init(unsigned int base, bool use_push_pull);

int vc_check_arq_states();

int vc_check_sys_to_rf();

int vc_check_rf_to_sys();



#endif /* __VC_INTERFACE_H__ */
