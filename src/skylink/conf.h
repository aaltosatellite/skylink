#ifndef __SKYLINK_CONF_H__
#define __SKYLINK_CONF_H__

#include <stdint.h>


typedef struct {

	/* Enable CCSDS randomizer/scrambler */
	uint8_t enable_scrambler;

	/* Enable CCSDS Reed-Solomon */
	uint8_t enable_rs;

	uint32_t authenticate_tx;

} SkyPHYConfig_t;


typedef struct {

	/* MAC-mode: 0 = no-TDD, 1 = TDD*/
	unsigned int mac_mode;

	/* Minimum number of slots inside a window */
	uint32_t min_slots;

	/* Maximum number of slots inside a window */
	uint32_t max_slots;

	/* Minimum time between windows size adjustments. */
	uint32_t windows_adjust_interval;

	/* Delay between communication direction (uplink/downlink) changes. */
	uint32_t switching_delay;


} SkyMACConfig_t;



typedef struct {

	/* Is authentication required for the channel */
	uint8_t require_authentication;

	/* Last sequence number */
	uint8_t last_sequence;

} SkyVCConfig_t;



typedef struct {

	uint8_t window_size;

} SkyARQConfig_t;




#endif /* __SKYLINK_CONF_H__ */
