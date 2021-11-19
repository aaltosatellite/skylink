#ifndef __VC_INTERFACE_H__
#define __VC_INTERFACE_H__

#include <stdbool.h>


int vc_init(unsigned int base, bool use_push_pull);

void vc_check();


#endif /* __VC_INTERFACE_H__ */
