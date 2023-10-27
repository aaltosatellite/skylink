#ifndef __SKYLINK_HMAC_H__
#define __SKYLINK_HMAC_H__


#include "skylink/skylink.h"
#include "skylink/conf.h"


/* HMAC trailer length */
#define SKY_HMAC_LENGTH                 4 // bytes


#if 1
/* HMAC runtime state */
struct sky_hmac {
	// TODO: Own key for each VC and direction
	uint8_t* key;
	int32_t key_len;
	uint32_t nonce_seed;

	// Next transmitted sequence number
	uint16_t sequence_tx[SKY_NUM_VIRTUAL_CHANNELS];

	// Next expected received sequence number
	uint16_t sequence_rx[SKY_NUM_VIRTUAL_CHANNELS];

	// Flag to indicate need to transmit HMAC reset extension
	uint8_t vc_enforcement_need[SKY_NUM_VIRTUAL_CHANNELS]; // TODO: bit field?

	// Pointer to hash function's context object
	void* ctx;
};
#else

/* HMAC runtime state */
struct sky_hmac
{
	// TODO: Own key for each VC and direction
	uint32_t nonce_seed;
	uint8_t** key_list;

	struct {
		int32_t sequence_tx;
		int32_t sequence_rx;
		uint8_t vc_enforcement_need;
	} vc[SKY_NUM_VIRTUAL_CHANNELS];

	void *ctx;
};

#endif

/* Allocate and initialize HMAC state instance */
SkyHMAC *sky_hmac_create(SkyHMACConfig *config);

/* Free HMAC resources */
void sky_hmac_destroy(SkyHMAC *hmac);

/* Get next sequence number from transmit counter and advance it by one. Sequence number naturally wraps around due to uint16 overflow. */
int32_t sky_hmac_get_next_tx_sequence(SkyHandle self, unsigned int vc);

/* Add authenticate trailer to a transmit frame. */
int sky_hmac_extend_with_authentication(SkyHandle self, SkyTransmitFrame* tx_frame);


/* Check the frame authentication and sequence number if required for the virtual channel.
 * Also, corrects sequence number field endianess and removes the HMAC extension from the frame if provided.
 * HMAC trailer is removed from the end of the frame.
 */
int sky_hmac_check_authentication(SkyHandle self, const SkyRadioFrame *frame, SkyParsedFrame* parsed);

/*
 * Load HMAC sequence numbers from given array.
 * Size of the array is 2 * SKY_NUM_VIRTUAL_CHANNELS.
 */
void sky_hmac_load_sequences(SkyHandle self, const uint16_t* sequences);

/*
 * Dump HMAC sequence numbers to given array.
 * Size of the array is 2 * SKY_NUM_VIRTUAL_CHANNELS.
 */
void sky_hmac_dump_sequences(SkyHandle self, uint16_t* sequences);


/* Marks a vc number as requiring hmac-sequence reset. This is used after a peer attempts authentication with too big sequence jump. */
//int sky_hmac_mark_vc_for_enforcement(SkyHandle self, uint8_t vc);


/* The obvious inverse of above function. */
//int sky_hmac_clear_vc_of_enforcement(SkyHandle self, uint8_t vc);



#endif /* __SKYLINK_HMAC_H__ */
