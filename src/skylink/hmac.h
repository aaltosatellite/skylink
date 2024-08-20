#ifndef __SKYLINK_HMAC_H__
#define __SKYLINK_HMAC_H__


#include "skylink/skylink.h"
#include "skylink/conf.h"


/* HMAC trailer length */
#define SKY_HMAC_LENGTH                 (4) // bytes


typedef struct {
	const uint8_t* key;
	const unsigned int len;
} SkyHMACKey;


/* Per virtual channel HMAC state */
typedef struct
{
	/* Flag to indicate need to transmit HMAC reset extension */
	uint8_t send_sequence_reset;

	/* Current transmitting sequence number */
	int32_t sequence_tx;

	/* Current receiving sequence number */
	int32_t sequence_rx;
} SkyHMACVChannel;


/* HMAC runtime state */
struct sky_hmac
{
	// HMAC keys
	const SkyHMACKey *keys;
	unsigned int num_keys;

	/* Array of per virtual channel HMAC states */
	SkyHMACVChannel vc[SKY_NUM_VIRTUAL_CHANNELS];

	uint32_t nonce_seed;

	/* Internal Blake3 context object */
	void *ctx;
};


/* Allocate and initialize HMAC state instance */
SkyHMAC *sky_hmac_create(SkyHMACConfig *config);

/* Free HMAC resources */
void sky_hmac_destroy(SkyHMAC *hmac);

/* Set HMAC keys */
void sky_hmac_set_keys(SkyHandle self, const SkyHMACKey *keys, unsigned int count);

/* Get next sequence number from transmit counter and advance it by one. Sequence number naturally wraps around due to uint16 overflow. */
int32_t sky_hmac_get_next_tx_sequence(SkyHandle self, unsigned int vc);

/*
 * Add authenticate trailer to a transmit frame.
 */
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


#endif /* __SKYLINK_HMAC_H__ */
