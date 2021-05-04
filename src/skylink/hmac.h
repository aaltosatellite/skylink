#ifndef __SKYLINK_HMAC_H__
#define __SKYLINK_HMAC_H__

#include "skylink/skylink.h"

#include <stdint.h>


/*
 */
#define SKY_HMAC_LENGTH 8


/*
 * HMAC runtime state
 */
struct sky_hmac {
	const uint8_t* key;
	unsigned int key_len;
	unsigned int seq_num[SKY_NUM_VIRTUAL_CHANNELS];
	void* ctx;
};


/*
 * Initialize HMAC
 */
int sky_hmac_init(SkyHandle_t self, const uint8_t* key, unsigned int key_len);

/*
 * Authenticate a frame.
 */
int sky_hmac_authenticate(SkyHandle_t self, SkyRadioFrame_t* frame);

/*
 * Check the frame authentication.
 */
int sky_hmac_check_authentication(SkyHandle_t self, SkyRadioFrame_t* frame);


#endif /* __SKYLINK_HMAC_H__ */
