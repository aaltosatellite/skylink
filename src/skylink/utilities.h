#ifndef __SKYLINK_UTILITIES_H__
#define __SKYLINK_UTILITIES_H__

#include <stdint.h>
#include "sky_platform.h"


#define MOD_TIME_TICKS		16777216

// Max between two values x and y.
#define MAX(x,y)		(x > y ? x : y)

// Min between two values x and y.
#define MIN(x,y)		(x < y ? x : y)

// GENERAL PURPOSE =====================================================================================================

// Host to network byte order conversion for 16 bit values.
uint16_t sky_hton16(uint16_t vh);

// Network to host byte order conversion for 16 bit values.
uint16_t sky_ntoh16(uint16_t vn);

// Host to network byte order conversion for 32 bit values.
uint32_t sky_hton32(uint32_t vh);

// Network to host byte order conversion for 32 bit values.
uint32_t sky_ntoh32(uint32_t vn);

// Network to host byte order conversion for 32 bit signed integer values.
int32_t sky_ntohi32(int32_t vn);

// Host to network byte order conversion for 32 bit signed integer values.
int32_t sky_htoni32(int32_t vn);

/*
*   Positive modulo function.
*   This is needed because C's modulo operator is not the same as the mathematical modulo operator, but rather the remainder operator.
*/
int32_t positive_modulo(int32_t x, int32_t m);

// Linear search for a value in an array of uint8_t.
int x_in_u8_array(uint8_t x, const uint8_t* array, int length);

// Linear search for a value in an array of uint16_t.
int x_in_u16_array(uint16_t x, const uint16_t* array, int length);

// Function that allows to wrap a time value to the MOD_TIME_TICKS value.
int32_t wrap_time_ticks(sky_tick_t time_ticks);
// GENERAL PURPOSE =====================================================================================================


// GLOBAL TIME =====================================================================================================

/* Set current time in ticks */
int sky_tick(sky_tick_t time_in_ticks);

// Get current time in ticks.
sky_tick_t sky_get_tick_time();
// GLOBAL TIME =====================================================================================================




#endif //__SKYLINK_UTILITIES_H__
