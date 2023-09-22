
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

	if (config->identity_len < 3 || config->identity_len > 7) // TODO: better place?
		return 0;

	// Sanity check ARQ parameters (TODO: find a better place)
	SkyARQConfig *arq_conf = &config->arq;
	if (arq_conf->timeout_ticks < 1000 || arq_conf->timeout_ticks > 30000)
		arq_conf->timeout_ticks = 10000;
	if (arq_conf->idle_frame_threshold < 100 || arq_conf->idle_frame_threshold > 10000)
		arq_conf->idle_frame_threshold = 1000;
	if (arq_conf->idle_frames_per_window < 1 || arq_conf->idle_frames_per_window > 4)
		arq_conf->idle_frames_per_window = 1;


	SkyHandle handle = SKY_MALLOC(sizeof(struct sky_all));
	SKY_ASSERT(handle != NULL);
	memset(handle, 0, sizeof(struct sky_all));

	handle->conf = config;
	
	handle->diag = sky_diag_create();
	SKY_ASSERT(handle != NULL);
	
	handle->mac = sky_mac_create(&config->mac);
	SKY_ASSERT(handle->mac != NULL);

	handle->hmac = sky_hmac_create(&config->hmac);
	SKY_ASSERT(handle->hmac != NULL);


	for (unsigned int i = 0; i < SKY_NUM_VIRTUAL_CHANNELS; ++i)
	{
		handle->virtual_channels[i] = sky_vc_create(&config->vc[i]);
		SKY_ASSERT(handle->hmac != NULL);
	}

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
Functions for swapping endian ordering if needed.
*/
uint16_t sky_hton16(uint16_t vh) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return vh;
#else
	return (((vh & 0xff00) >> 8) | ((vh & 0x00ff) << 8));
#endif
}

inline uint16_t __attribute__ ((__const__)) sky_ntoh16(uint16_t vn) {
	return sky_hton16(vn);
}


inline uint32_t __attribute__ ((__const__)) sky_hton32(uint32_t vh) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return vh;
#else
	return (((vh & 0xff000000) >> 24) | ((vh & 0x000000ff) << 24) |
			((vh & 0x0000ff00) <<  8) | ((vh & 0x00ff0000) >>  8));
#endif
}

inline uint32_t __attribute__ ((__const__)) sky_ntoh32(uint32_t vn) {
	return sky_hton32(vn);
}

inline int32_t __attribute__ ((__const__)) sky_htoni32(int32_t vh) {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
	return vh;
#else
	return (int32_t) (((vh & 0xff000000) >> 24) | ((vh & 0x000000ff) << 24) | ((vh & 0x0000ff00) <<  8) | ((vh & 0x00ff0000) >>  8));
#endif
}

inline int32_t __attribute__ ((__const__)) sky_ntohi32(int32_t vn) {
	return sky_htoni32(vn);
}





int32_t positive_modulo(int32_t x, int32_t m){
	if((abs(x) > (m*12))){
		return ((x % m) + m) % m;
	}
	while(x < 0){
		x = x + m;
	}
	while(x >= m){
		x = x - m;
	}
	//SKY_ASSERT(x == (((x % m) + m) % m) );
	return x;
}






int x_in_u8_array(uint8_t x, const uint8_t* array, int length){
	for (int i = 0; i < length; ++i) {
		if(array[i] == x){
			return i;
		}
	}
	return -1;
}

int x_in_u16_array(uint16_t x, const uint16_t* array, int length){
	for (int i = 0; i < length; ++i) {
		if(array[i] == x){
			return i;
		}
	}
	return -1;
}

int32_t wrap_time_ticks(sky_tick_t time_ticks){
	return positive_modulo(time_ticks, MOD_TIME_TICKS);
}


// GENERAL PURPOSE =====================================================================================================




// GLOBAL TIME =====================================================================================================

sky_tick_t _global_ticks_now = 0;

int sky_tick(sky_tick_t time_in_ticks)
{
	int ret = 0;
	if(time_in_ticks != _global_ticks_now){
		ret = 1;
	}
	//_global_ticks_now = time_in_ticks;
	_global_ticks_now = positive_modulo(time_in_ticks, MOD_TIME_TICKS);
	return ret;
}

sky_tick_t sky_get_tick_time()
{
	return _global_ticks_now;
}

// GLOBAL TIME =====================================================================================================

