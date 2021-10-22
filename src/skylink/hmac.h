#ifndef __SKYLINK_HMAC_H__
#define __SKYLINK_HMAC_H__

#include "skylink/skylink.h"

#include <stdint.h>





/*
 * Initialize HMAC
 */
int sky_hmac_init(SkyHandle_t self, const uint8_t* key, unsigned int key_len);

/*
 * Return boolean as to if a frame claims it is authenticated.
 */
int sky_hmac_frame_claims_authenticated(SkyRadioFrame* frame);

/*
 * Authenticate a frame.
 */
int sky_hmac_authenticate(SkyHandle_t self, SkyRadioFrame* frame);

/*
 * Check the frame authentication.
 */
int sky_hmac_check_authentication(SkyHandle_t self, SkyRadioFrame* frame);


int sky_hmac_vc_demands_auth(SkyHandle_t self, uint8_t vc);


#endif /* __SKYLINK_HMAC_H__ */
