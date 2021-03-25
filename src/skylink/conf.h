#ifndef __SKYLINK_CONF_H__
#define __SKYLINK_CONF_H__

#include <stdint.h>


typedef struct {

	/* Minimum number of slots inside a window */
	uint32_t min_slots;

	/* Maximum number of slots inside a window */
	uint32_t max_slots;

	/* Minimum time between windows size adjustmends. */
	uint32_t windows_adjust_interval;

	/* Delay between communication direction (uplink/downlink) changes. */
	uint32_t switching_delay;

} SkyMACConfig_t;


typedef struct {

    /* Enable CCSDS randomizer/scrambler */
    uint8_t enable_scrambler;

    /* Enable CCSDS Reed-Solomon */
	uint8_t enable_rs;

} SkyPHYConfig_t;




#endif /* __SKYLINK_CONF_H__ */
