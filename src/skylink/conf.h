#ifndef __SKYLINK_CONF_H__
#define __SKYLINK_CONF_H__

#include <stdint.h>
#include "skylink.h"

//HMAC
#define SKY_HMAC_LENGTH 				8

//FLAGS
#define SKY_FLAG_AUTHENTICATED 			0b00001
#define SKY_FLAG_ARQ_ON 				0b00010
#define SKY_FLAG_HAS_PAYLOAD 			0b00100

//Number of frames justified to send only due to protocol control reasons (in absence of payloads).
#define UTILITY_FRAMES_PER_WINDOW		3

//Number of bytes in frame identity field
#define SKY_IDENTITY_LEN				5





typedef struct {
	/* Enable CCSDS randomizer/scrambler */
	uint8_t enable_scrambler;

	/* Enable CCSDS Reed-Solomon */
	uint8_t enable_rs;

	uint32_t authenticate_tx;
} SkyPHYConfig;



typedef struct {
	/* Default send window size for both me and peer. */
	int32_t default_window_length;
	int32_t maximum_window_length;
	int32_t minimum_window_length;

	/* Default time gap size between windows.
	 * Gap is fully utilized when two peers cannot hear each other.
	 * Too small gap value will make it difficult for the peers to avoid speaking over each other by chance.
	 * Too large value will decrease error and packet loss resistance of ARQ: In cases of packet loss,
	 * the peers will take long time to send anything, and will ARQ timeout happens too easily.*/
	int32_t default_gap_length;

	/* Default tail end time of the cycle. */
	int32_t default_tail_length;

	/* Boolean toggle for wether an unauthenticated frame can update MAC state belief.
	 * Enabling this will allow a continuous stream of unauthenticated frames to essentially block transmission:
	 * a so called 'shut-up-attack' */
	uint8_t unauthenticated_mac_updates;

	int32_t shift_threshold_ms;

} SkyMACConfig;



typedef struct {
	int element_size;

	int rcv_ring_len;
	int horizon_width;

	int send_ring_len;

	uint8_t require_authentication;
} SkyVCConfig;



typedef struct {
	int32_t key_length;

	uint8_t key[16];

	/* Length of the sequence cycle */
	//int32_t cycle_length;

	/* Maximum jump in sequence allowed */
	int32_t maximum_jump;

} HMACConfig;





/*
 * Protocol configuration struct.
 *
 * Some of the parameters can be changed while the link is running.
 * Where feasible, sublayer implementations should read their parameters
 * directly from here, allowing configuration changes.
 */
struct sky_conf {
	SkyPHYConfig phy;
	SkyMACConfig mac;
	HMACConfig	hmac;
	SkyVCConfig vc[SKY_NUM_VIRTUAL_CHANNELS];
	uint8_t identity[SKY_IDENTITY_LEN];
	int32_t arq_timeout_ms;
};
typedef struct sky_conf SkyConfig;

#endif /* __SKYLINK_CONF_H__ */
