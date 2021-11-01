#ifndef __SKYLINK_HMAC_H__
#define __SKYLINK_HMAC_H__

#include <stdint.h>
#include "skylink.h"
#include "../ext/cifra/hmac.h"
#include "../ext/cifra/sha2.h"


#define SKY_HMAC_CTX_SIZE  (sizeof(cf_hmac_ctx))
#define HMAC_CYCLE_LENGTH	65000
#define HMAC_NO_SEQUENCE	65010


/* Positive modulo by max hmac sequence */
int32_t wrap_hmac_sequence(int32_t sequence);

/* Allocate and initialize HMAC state instance */
SkyHMAC* new_hmac_instance(HMACConfig* config);


/* Free HMAC resources */
void destroy_hmac(SkyHMAC* hmac);


/* Get next sequence number from transmit counter and advance it by one (and wrap modulo cycle) */
int sky_hmac_get_next_hmac_tx_sequence_and_advance(SkyHandle self, uint8_t vc);


/* Authenticate a frame. */
int sky_hmac_extend_with_authentication(SkyHandle self, SkyRadioFrame* frame);


/* Check the frame authentication. */
int sky_hmac_check_authentication(SkyHandle self, SkyRadioFrame* frame);


/* Return booelan (0/1) wether the virtual channel requires authentication */
int sky_hmac_vc_demands_auth(SkyHandle self, uint8_t vc);


/* Marks a vc number as requiring hmac-sequence reset. This is used after a peer attempts authentication with too big sequence jump. */
//int sky_hmac_mark_vc_for_enforcement(SkyHandle self, uint8_t vc);


/* The obvious inverse of above function. */
//int sky_hmac_clear_vc_of_enforcement(SkyHandle self, uint8_t vc);



#endif /* __SKYLINK_HMAC_H__ */
