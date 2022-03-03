#ifndef __SKYLINK_CONF_H__
#define __SKYLINK_CONF_H__

#include "skylink.h"
#include "skylink/frame.h"


// HMAC
#define SKY_HMAC_LENGTH                 8 // bytes




/*
 * Physical level configuration
 */
typedef struct {
	/* Enable CCSDS randomizer/scrambler */
	uint8_t enable_scrambler;

	/* Enable CCSDS Reed-Solomon */
	uint8_t enable_rs;

} SkyPHYConfig;



typedef struct {
	/* Default send window size for both me and peer. */
	int32_t default_window_length_ticks;
	int32_t maximum_window_length_ticks;
	int32_t minimum_window_length_ticks;

	/* Default time gap size between windows.
	 * Gap is fully utilized when two peers cannot hear each other.
	 * Too small gap value will make it difficult for the peers to avoid speaking over each other by chance.
	 * Too large value will decrease error and packet loss resistance of ARQ: In cases of packet loss,
	 * the peers will take long time to send anything, and will ARQ timeout happens too easily.*/
	int32_t gap_constant_ticks;

	/* Default tail end time of the cycle. */
	int32_t tail_constant_ticks;

	/* Boolean toggle for wether an unauthenticated frame can update MAC state belief.
	 * Enabling this will allow a continuous stream of unauthenticated frames to essentially block transmission:
	 * a so called 'shut-up-attack' */
	uint8_t unauthenticated_mac_updates;

	int32_t shift_threshold_ticks;

	int16_t window_adjust_increment_ticks;

	int8_t adjustment_period;

	/* After this many ticks of not getting mac state update, the link is considered silent. */
	int32_t idle_timeout_ticks;

	int8_t idle_frames_per_window;
} SkyMACConfig;



/* Virtual channel authentication option flags */
#define SKY_VC_FLAG_AUTHENTICATE_TX          (0b001)
#define SKY_VC_FLAG_REQUIRE_AUTHENTICATION   (0b010)
#define SKY_VC_FLAG_REQUIRE_SEQUENCE         (0b100)

/*
 * Per virtual channel configurations
 */
typedef struct {

	/* Size of single element in the element buffer*/
	int element_size;

	/**/
	int rcv_ring_len;

	/* */
	int horizon_width;

	/* Length of the ARQ sequence ring.  */
	int send_ring_len;

	/* Is authentication code (HMAC) required for the virtual channel */
	uint8_t require_authentication;
} SkyVCConfig;


/*
 * HMAC configurations
 */
typedef struct {
	/* Length of the authentication key */
	int32_t key_length;

	/* Authentication key */
	uint8_t key[16];

	/* Length of the sequence cycle */
	//int32_t cycle_length;

	/* Maximum allowed forward jump in sequence count */
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
	int32_t arq_timeout_ticks;
	int8_t arq_idle_frames_per_window;
};
typedef struct sky_conf SkyConfig;

#endif /* __SKYLINK_CONF_H__ */
