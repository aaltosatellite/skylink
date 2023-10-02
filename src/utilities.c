
#include "skylink/utilities.h"
#include "skylink/skylink.h"
#include "skylink/conf.h"

#include "skylink/diag.h"
#include "skylink/mac.h"
#include "skylink/reliable_vc.h"
#include "skylink/hmac.h"

#include "sky_platform.h"

#include <string.h> // memset, memcpy

// Create new Skylink protocol instance based on the configuration struct.
SkyHandle sky_create(SkyConfig *config)
{

	// Sanity check identity length
	if (config->identity_len < 3 || config->identity_len > 7) // TODO: better place?
		return 0;

	// Sanity check ARQ parameters and set to default if invalid value in config. (TODO: find a better place)
	SkyARQConfig *arq_conf = &config->arq;
	if (arq_conf->timeout_ticks < 1000 || arq_conf->timeout_ticks > 30000)
		arq_conf->timeout_ticks = 10000;
	if (arq_conf->idle_frame_threshold < 100 || arq_conf->idle_frame_threshold > 10000)
		arq_conf->idle_frame_threshold = 1000;
	if (arq_conf->idle_frames_per_window < 1 || arq_conf->idle_frames_per_window > 4)
		arq_conf->idle_frames_per_window = 1;


	//Allocate memory for Skylink instance and set it to zero.
	SkyHandle handle = SKY_MALLOC(sizeof(struct sky_all));
	SKY_ASSERT(handle != NULL);
	memset(handle, 0, sizeof(struct sky_all));

	// Copy configuration to Skylink instance.
	handle->conf = config;

	// Create diagnostics instance.
	handle->diag = sky_diag_create();
	SKY_ASSERT(handle != NULL);

	// Create TDD/MAC instance.
	handle->mac = sky_mac_create(&config->mac);
	SKY_ASSERT(handle->mac != NULL);

	// Create HMAC instance.
	handle->hmac = sky_hmac_create(&config->hmac);
	SKY_ASSERT(handle->hmac != NULL);

	// Create virtual channels.
	for (unsigned int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i)
	{
		handle->virtual_channels[i] = sky_vc_create(&config->vc[i]);
		SKY_ASSERT(handle->hmac != NULL);
	}

	// Return created instance
	return handle;
}

// Destroy Skylink instance and free all of its memory.
void sky_destroy(SkyHandle handle)
{
	sky_diag_destroy(handle->diag);
	sky_mac_destroy(handle->mac);
	sky_hmac_destroy(handle->hmac);
	for (unsigned int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i)
		sky_vc_destroy(handle->virtual_channels[i]);
	SKY_FREE(handle);
}


// GENERAL PURPOSE =====================================================================================================
/*
*	Functions for swapping endian ordering if needed.
*/

// 16-bit host to network byte order conversion.
uint16_t sky_hton16(uint16_t vh) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return vh;
#else
	return (((vh & 0xff00) >> 8) | ((vh & 0x00ff) << 8));
#endif
}

// 16-bit network to host byte order conversion.
inline uint16_t __attribute__ ((__const__)) sky_ntoh16(uint16_t vn) {
	return sky_hton16(vn);
}

// 32-bit host to network byte order conversion.
inline uint32_t __attribute__ ((__const__)) sky_hton32(uint32_t vh) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return vh;
#else
	return (((vh & 0xff000000) >> 24) | ((vh & 0x000000ff) << 24) |
			((vh & 0x0000ff00) <<  8) | ((vh & 0x00ff0000) >>  8));
#endif
}

// 32-bit network to host byte order conversion.
inline uint32_t __attribute__ ((__const__)) sky_ntoh32(uint32_t vn) {
	return sky_hton32(vn);
}

// 32-bit signed integer host to network byte order conversion.
inline int32_t __attribute__ ((__const__)) sky_htoni32(int32_t vh) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return vh;
#else
	return (int32_t) (((vh & 0xff000000) >> 24) | ((vh & 0x000000ff) << 24) | ((vh & 0x0000ff00) <<  8) | ((vh & 0x00ff0000) >>  8));
#endif
}

// 32-bit signed integer network to host byte order conversion.
inline int32_t __attribute__ ((__const__)) sky_ntohi32(int32_t vn) {
	return sky_htoni32(vn);
}




/*
 *	Positive modulo function.
 *	This is needed because C's modulo operator is not the same as the mathematical modulo operator, but rather the remainder operator.
 */
int32_t positive_modulo(int32_t x, int32_t m){
	// x is large compared to m.
	if((abs(x) > (m*12))){
		return ((x % m) + m) % m;
	}
	// Optimizations without need for division since UHF processor doesn't have division instruction.
	while(x < 0){
		x = x + m;
	}
	while(x >= m){
		x = x - m;
	}
	//SKY_ASSERT(x == (((x % m) + m) % m) );
	return x;
}



// Positive modulo for time ticks. Allows time to wrap around back to zero.
int32_t wrap_time_ticks(sky_tick_t time_ticks){
	return positive_modulo(time_ticks, MOD_TIME_TICKS);
}


// GENERAL PURPOSE =====================================================================================================




// GLOBAL TIME =====================================================================================================

sky_tick_t _global_ticks_now = 0;

// Set current time in ticks.
int sky_tick(sky_tick_t time_in_ticks)
{
	// Check if time is going to change.
	int ret = 0;
	if(time_in_ticks != _global_ticks_now){
		ret = 1;
	}
	// Set _global_ticks_now to time_in_ticks
	_global_ticks_now = positive_modulo(time_in_ticks, MOD_TIME_TICKS);
	return ret;
}

// Get current time in ticks.
sky_tick_t sky_get_tick_time()
{
	return _global_ticks_now;
}

// GLOBAL TIME =====================================================================================================

