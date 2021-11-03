#ifndef __SKYLINK_CONF_H__
#define __SKYLINK_CONF_H__

#include <stdint.h>


//HMAC
#define SKY_HMAC_LENGTH 				8

//FLAGS
#define SKY_FLAG_AUTHENTICATED 			0b00001
#define SKY_FLAG_ARQ_ON 				0b00010
#define SKY_FLAG_HAS_PAYLOAD 			0b00100

//Physical layer radio frame structure.
#define SKY_NUM_VIRTUAL_CHANNELS  		4
#define SKY_FRAME_MAX_LEN       		0x100
#define SKY_IDENTITY_LEN				5



typedef struct {
	/* Enable CCSDS randomizer/scrambler */
	uint8_t enable_scrambler;

	/* Enable CCSDS Reed-Solomon */
	uint8_t enable_rs;

	uint32_t authenticate_tx;

} SkyPHYConfig;



typedef struct {
	/* Default send window size for both me and peer */
	int32_t default_window_length;

	/* Default gap size between windows. */
	int32_t default_gap_length;

	/* Default tail end time of the cycle */
	int32_t default_tail_length;

	int32_t maximum_window_size;

	int32_t minimum_window_size;

	int32_t maximum_gap_size;

	int32_t minimum_gap_size;

	/* Boolean toggle for wether an unauthenticated frame can update MAC state belief.
	 * Enabling this will allow a continuous stream of unauthenticated frames to essentially block transmission. */
	uint8_t unauthenticated_mac_updates;

} SkyMACConfig;



typedef struct {
	int element_size;
	int element_count;

	int rcv_ring_len;
	int initial_rcv_sequence;
	int horizon_width;

	int send_ring_len;
	int initial_send_sequence;
	int n_recall;

} SkyArrayConfig;



typedef struct {
	int32_t key_length;

	uint8_t key[16];

	uint16_t magic_sequence;

	/* Length of the sequence cycle */
	//int32_t cycle_length;

	/* Maximum jump in sequence allowed */
	int32_t maximum_jump;

} HMACConfig;



typedef struct {
	/* Is authentication required for the channel */
	uint8_t require_authentication;

	uint8_t arq_on;

} SkyVCConfig;



#endif /* __SKYLINK_CONF_H__ */
