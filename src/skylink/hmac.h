#ifndef __SKYLINK_HMAC_H__
#define __SKYLINK_HMAC_H__

#include <stdint.h>
#include "skylink.h"
#include "conf.h"
#include "../ext/cifra/hmac.h"
#include "../ext/cifra/sha2.h"


#define SKY_HMAC_CTX_SIZE  (sizeof(cf_hmac_ctx))
#define HMAC_CYCLE_LENGTH	65000
#define HMAC_NO_SEQUENCE	65010


/* HMAC runtime state */
struct sky_hmac {
	uint8_t* key;
	int32_t key_len;
	int32_t sequence_tx[SKY_NUM_VIRTUAL_CHANNELS];
	int32_t sequence_rx[SKY_NUM_VIRTUAL_CHANNELS];
	uint8_t vc_enforcement_need[SKY_NUM_VIRTUAL_CHANNELS];
	void* ctx;
};
typedef struct sky_hmac SkyHMAC;





/* Allocate and initialize HMAC state instance */
SkyHMAC* new_hmac_instance(HMACConfig* config);

/* Free HMAC resources */
void destroy_hmac(SkyHMAC* hmac);


/* Get next sequence number from transmit counter and advance it by one (and wrap modulo cycle) */
int32_t sky_hmac_get_next_hmac_tx_sequence_and_advance(SkyHandle self, uint8_t vc);


/* Authenticate a frame. */
int sky_hmac_extend_with_authentication(SkyHandle self, SkyRadioFrame* frame);


/* Check the frame authentication and sequence number if required for the virtual channel.
 * Also, corrects sequence number field endianess and removes the HMAC extension from the frame if provided. */
int sky_hmac_check_authentication(SkyHandle self, SkyRadioFrame* frame);

/* Positive modulo by max hmac sequence */
int32_t wrap_hmac_sequence(int32_t sequence);

/*
 * Load HMAC sequence numbers from given array.
 * Size of the array is 2 * SKY_NUM_VIRTUAL_CHANNELS.
 */
int sky_hmac_load_sequences(SkyHandle self, const int32_t* sequences);

/*
 * Dump HMAC sequence numbers to given array.
 * Size of the array is 2 * SKY_NUM_VIRTUAL_CHANNELS.
 */
int sky_hmac_dump_sequences(SkyHandle self, int32_t* sequences);


/* Marks a vc number as requiring hmac-sequence reset. This is used after a peer attempts authentication with too big sequence jump. */
//int sky_hmac_mark_vc_for_enforcement(SkyHandle self, uint8_t vc);


/* The obvious inverse of above function. */
//int sky_hmac_clear_vc_of_enforcement(SkyHandle self, uint8_t vc);



#endif /* __SKYLINK_HMAC_H__ */
