#ifndef __SKYLINK_CONF_H__
#define __SKYLINK_CONF_H__

#include "skylink/skylink.h"
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


/* TDD MAC configuration struct */
typedef struct {

	/* Transmission window size limits for both me and peer. */
	int32_t maximum_window_length_ticks;
	int32_t minimum_window_length_ticks;

	/* Default time gap size between windows.
	 * Gap is fully utilized when two peers cannot hear each other.
	 * Too small gap value will make it difficult for the peers to avoid speaking over each other by chance.
	 * Too large value will decrease error and packet loss resistance of ARQ: In cases of packet loss,
	 * the peers will take long time to send anything, and will ARQ timeout happens too easily.*/
	int32_t gap_constant_ticks;

	/* Minimum time */
	int32_t tail_constant_ticks; // TODO: Rename switch_delay_ticks.

	/* Deprecated configuration value for shifting window to avoid MAC collision. Instead, adjust gap length. */
	int32_t shift_threshold_ticks; //TODO: Remove in the future.

	/* After this many ticks of not getting MAC state updates, the link is considered idle. */
	int32_t idle_timeout_ticks;

	/* Increment size of single windows size adjustment in tick */
	int16_t window_adjust_increment_ticks;

	/* Fallback by this many ticks when frame carrier is sensed */
	int16_t carrier_sense_ticks;

	/* Boolean toggle for wether an unauthenticated frame can update MAC state belief.
	 * Enabling this will allow a continuous stream of unauthenticated frames to essentially block transmission:
	 * a so called 'shut-up-attack' */
	uint8_t unauthenticated_mac_updates;

	/* How often window length can be adjusted (incremented or decremented). Count in windows. */
	int8_t window_adjustment_period;

	/* If window has less frames than the count, an idle frames are generated to fill the window. */
	uint8_t idle_frames_per_window;

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

	/* Size of the ring buffer for reception */
	int rcv_ring_len;

	/* How many payloads VC can receive before the next in line. */
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

	/* Maximum allowed forward jump in sequence count */
	int32_t maximum_jump;

	/* Authentication key */
	uint8_t key[32];

} SkyHMACConfig;


/*
 * ARQ configurations
 */
typedef struct
{
	/* If continuous packet transmission or reception has not advanced in arq_timeout_ticks time, arq drops off. */
	int32_t timeout_ticks;

	/* If continuous packet transmission or reception has not advanced in idle_frame_threshold time,
	 * frames are being created even if there are no payloads. This is to keep status fresh. */
	int32_t idle_frame_threshold;

	/* How many idle frames are created per window per virtual channel where ARQ is active. */
	int8_t idle_frames_per_window;

} SkyARQConfig;


/*
 * Protocol configuration struct.
 *
 * Some of the parameters can be changed while the link is running.
 * Where feasible, sublayer implementations should read their parameters
 * directly from here, allowing configuration changes.
 */
struct sky_conf {
	SkyPHYConfig    phy;
	SkyMACConfig    mac;
	SkyHMACConfig	hmac;
	SkyVCConfig     vc[SKY_NUM_VIRTUAL_CHANNELS];
	SkyARQConfig    arq;
	uint8_t identity[SKY_IDENTITY_LEN];
};
typedef struct sky_conf SkyConfig;

#endif /* __SKYLINK_CONF_H__ */
