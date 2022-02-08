//
// Created by elmore on 25.10.2021.
//

#include <stdio.h>
#include <stdlib.h>
#include "skylink/platform.h"
#include "skylink/utilities.h"
#define DEBUG
#ifdef DEBUG
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





int positive_modulo_x(int32_t x, int32_t m){
	return ((x % m) + m) % m;
}



int32_t positive_modulo_true(int32_t x, int32_t m){
	//return (((x % m) + m) % m);
	if((abs(x) > (m*12))){
		//printf("slw.");
		return ((x % m) + m) % m;
	}
	while(x < 0){
		x = x + m;
	}
	while(x >= m){
		x = x - m;
	}
#ifdef DEBUG
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

int32_t wrap_time_ms(int32_t time_ms){
	return positive_modulo_true(time_ms, MOD_TIME_MS);
}


// GENERAL PURPOSE =====================================================================================================




// GLOBAL TIME =====================================================================================================
timestamp_t _global_time_now_ms = 0;

int sky_tick(timestamp_t time_ms){
	int ret = 0;
	if(time_ms != _global_time_now_ms){
		ret = 1;
	}
	_global_time_now_ms = MAX(0, _global_time_now_ms + 1);
	return ret;
}

timestamp_t sky_get_tick_time(){
	return _global_time_now_ms;
}
// GLOBAL TIME =====================================================================================================





// UNIX ================================================================================================================
#ifdef __unix__
#include <stdlib.h>
#include <stdio.h>
static size_t allocated = 0;
static int allocations = 0;



void* instr_malloc(size_t n){
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
