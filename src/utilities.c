//
// Created by elmore on 25.10.2021.
//

#include "skylink/utilities.h"

// GENERAL PURPOSE =====================================================================================================
uint16_t sky_hton16(uint16_t vh) {
#ifndef __LITTLE_ENDIAN__
	return vh;
#else
	return (((vh & 0xff00) >> 8) | ((vh & 0x00ff) << 8));
#endif
}

inline uint16_t __attribute__ ((__const__)) sky_ntoh16(uint16_t vn) {
	return sky_hton16(vn);
}


inline uint32_t __attribute__ ((__const__)) sky_hton32(uint32_t vh) {
#ifndef __LITTLE_ENDIAN__
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
#ifndef __LITTLE_ENDIAN__
	return vh;
#else
	return (int32_t) (((vh & 0xff000000) >> 24) | ((vh & 0x000000ff) << 24) | ((vh & 0x0000ff00) <<  8) | ((vh & 0x00ff0000) >>  8));
#endif
}

inline int32_t __attribute__ ((__const__)) sky_ntohi32(int32_t vn) {
	return sky_htoni32(vn);
}



int positive_modulo(int x, int m){
	return ((x % m) + m) % m;
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
	return ((time_ms % MOD_TIME_MS) + MOD_TIME_MS) % MOD_TIME_MS;
}
// GENERAL PURPOSE =====================================================================================================





// UNIX ================================================================================================================
#ifdef __unix__
#include <stdlib.h>
#include <stdio.h>
static size_t allocated = 0;
static int allocations = 0;


int32_t get_time_ms(){
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	uint64_t ts = t.tv_sec*1000;
	ts += t.tv_nsec/1000000;
	ts = ts % MOD_TIME_MS;
	return (int32_t) ts;
}

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
//---
//todo: implement FreeRTOS get_time_ms()


#endif
// ARM =================================================================================================================
