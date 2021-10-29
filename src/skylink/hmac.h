#ifndef __SKYLINK_HMAC_H__
#define __SKYLINK_HMAC_H__

#include <stdint.h>
#include "skylink.h"
#include "../ext/cifra/hmac.h"
#include "../ext/cifra/sha2.h"


#define SKY_HMAC_CTX_SIZE  (sizeof(cf_hmac_ctx))

/*
 * Allocate and initialize HMAC state instance
 */
SkyHMAC* new_hmac_instance(HMACConfig* config);


/*
 * Free HMAC resources
 */
void destroy_hmac(SkyHMAC* hmac);


/*
 * Get next sequence number from transmit counter and advance it by one (and wrap modulo cycle)
 */
int sky_hmac_get_next_hmac_tx_sequence_and_advance(SkyHandle self, uint8_t vc);


/*
 * Authenticate a frame.
 */
int sky_hmac_extend_with_authentication(SkyHandle self, SkyRadioFrame* frame);


/*
 * Check the frame authentication.
 */
int sky_hmac_check_authentication(SkyHandle self, SkyRadioFrame* frame);


/*
 * Return booelan (0/1) wether the virtual channel requires authentication
 */
int sky_hmac_vc_demands_auth(SkyHandle self, uint8_t vc);


#endif /* __SKYLINK_HMAC_H__ */
