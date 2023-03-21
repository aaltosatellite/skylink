#ifndef __SKYLINK_UTILITIES_H__
#define __SKYLINK_UTILITIES_H__

#include <stdint.h>
#include "sky_platform.h"


#define MOD_TIME_TICKS		16777216

#define MAX(x,y)		(x > y ? x : y)
#define MIN(x,y)		(x < y ? x : y)

// GENERAL PURPOSE =====================================================================================================
uint16_t sky_hton16(uint16_t vh);

uint16_t sky_ntoh16(uint16_t vn);

uint32_t sky_hton32(uint32_t vh);

uint32_t sky_ntoh32(uint32_t vn);

int32_t sky_ntohi32(int32_t vn);

int32_t sky_htoni32(int32_t vn);

int32_t positive_modulo(int32_t x, int32_t m);

int x_in_u8_array(uint8_t x, const uint8_t* array, int length);

int x_in_u16_array(uint16_t x, const uint16_t* array, int length);

int32_t wrap_time_ticks(tick_t time_ticks);
// GENERAL PURPOSE =====================================================================================================


// GLOBAL TIME =====================================================================================================
int sky_tick(tick_t time_in_ticks);
tick_t sky_get_tick_time();
// GLOBAL TIME =====================================================================================================




#endif //__SKYLINK_UTILITIES_H__
