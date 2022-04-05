//
// Created by elmore on 25.10.2021.
//

#include <stdio.h>
#include <stdlib.h>
#include "skylink/platform.h"
#include "skylink/utilities.h"

#ifdef SKY_DEBUG
#include <assert.h>
#endif


// GENERAL PURPOSE =====================================================================================================
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
#ifdef SKY_DEBUG
	assert(x == (((x % m) + m) % m) );
#endif
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

int32_t wrap_time_ticks(tick_t time_ticks){
	return positive_modulo(time_ticks, MOD_TIME_TICKS);
}


// GENERAL PURPOSE =====================================================================================================




// GLOBAL TIME =====================================================================================================
tick_t _global_ticks_now = 0;

int sky_tick(tick_t time_in_ticks){
	int ret = 0;
	if(time_in_ticks != _global_ticks_now){
		ret = 1;
	}
	//_global_ticks_now = time_in_ticks;
	_global_ticks_now = positive_modulo(time_in_ticks, MOD_TIME_TICKS);
	return ret;
}

tick_t sky_get_tick_time(){
	return _global_ticks_now;
}
// GLOBAL TIME =====================================================================================================





// UNIX ================================================================================================================
#ifdef __unix__
static size_t allocated = 0;
static int allocations = 0;



void* instrumented_malloc(size_t n){
	printf("  (allocating %ld)\n", n); fflush(stdout);
	allocated += n;
	allocations++;
	return malloc(n);
}

void report_allocation(){
	printf("=====================\n"); fflush(stdout);
	printf("%ld bytes allocated.\n", allocated); fflush(stdout);
	printf("%d allocations.\n", allocations); fflush(stdout);
	printf("=====================\n"); fflush(stdout);
}


#endif
// UNIX ================================================================================================================






// ARM =================================================================================================================
#ifdef __arm__

#endif
// ARM =================================================================================================================
