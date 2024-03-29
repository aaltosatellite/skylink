#ifndef __SKYLINK_CONF_H__
#define __SKYLINK_CONF_H__

#include "skylink/skylink.h"
#include "skylink/frame.h"


#define SKY_USE_TDD_MAC



/* TDD MAC configuration struct */
typedef struct {

	/* Transmission window size limits for both me and peer. */
	int32_t maximum_window_length_ticks;
	int32_t minimum_window_length_ticks;

	/* Default time gap size between windows.
	 * Gap is fully utilized when two peers cannot hear each other.
	 * Too small of a gap value will make it difficult for the peers to avoid speaking over each other by chance.
	 * Too large of a value will decrease error and packet loss resistance of ARQ: In cases of packet loss,
	 * the peers will take a long time to send anything, and ARQ timeouts will happen too easily.*/
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

	/* Boolean toggle for whether an unauthenticated frame can update MAC state belief.
	 * Enabling this will allow a continuous stream of unauthenticated frames to essentially block transmission:
	 * a so called 'shut-up-attack' */
	uint8_t unauthenticated_mac_updates;

	/* How often window length can be adjusted (incremented or decremented). Count in windows. */
	int8_t window_adjustment_period;

	/* If window has less frames than the count, idle frames are generated to fill the window. */
	uint8_t idle_frames_per_window;

} SkyMACConfig;



/* Virtual channel authentication option flags */
#define SKY_CONFIG_FLAG_AUTHENTICATE_TX          (0b0001)
#define SKY_CONFIG_FLAG_REQUIRE_AUTHENTICATION   (0b0010)
#define SKY_CONFIG_FLAG_REQUIRE_SEQUENCE         (0b0100)
#define SKY_CONFIG_FLAG_USE_CRC32                (0b1000)

/*
 * Per virtual channel configurations
 */
typedef struct {
	/* Usable size of single element in the element buffer*/
	int usable_element_size;

	/* Size of the ring buffer for reception */
	int rcv_ring_len;

	/* How many payloads VC can receive before the next in line. */
	int horizon_width;

	/* Length of the ARQ sequence ring.  */
	int send_ring_len;

	/* Is authentication code (HMAC) required for the virtual channel */
	uint8_t require_authentication;

	//uint8_t tx_key, rx_key;

} SkyVCConfig;


/*
 * HMAC configurations
 */
typedef struct {

	/* Length of the authentication key */
	int32_t key_length;

	/* Maximum allowed forward jump in sequence count.
	 * Value should larger than expected number of lost frames aka ARQ window length.
	 * Sensible values are between 16 and 64. Recommended value is 24. */
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
	SkyMACConfig    mac; // MAC/TDD Configuration
	SkyHMACConfig	hmac; // HMAC Configuration
	SkyVCConfig     vc[SKY_NUM_VIRTUAL_CHANNELS]; // Virtual channel configurations
	SkyARQConfig    arq; // Automatic repeat request configurations
	uint8_t         identity[SKY_MAX_IDENTITY_LEN]; // Identity
	unsigned int    identity_len; // Length of identity in bytes
};
typedef struct sky_conf SkyConfig;

#endif /* __SKYLINK_CONF_H__ */
