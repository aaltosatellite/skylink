#ifndef __VC_INTERFACE_H__
#define __VC_INTERFACE_H__

#include <stdbool.h>


int vc_init(unsigned int base, bool use_push_pull);

int vc_check_arq_states();

int vc_check_incoming();

int vc_check_outcoming();



#endif /* __VC_INTERFACE_H__ */
