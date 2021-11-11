//
// Created by elmore on 25.10.2021.
//

#include "skylink/utilities.h"


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

int positive_modulo(int x, int m){
	return ((x % m) + m) % m;
}


int x_in_u8_array(uint8_t x, uint8_t* array, int length){
	for (int i = 0; i < length; ++i) {
		if(array[i] == x){
			return i;
		}
	}
	return -1;
}




// UNIX ================================================================================================================
#ifdef __unix__

#include <stdlib.h>



int32_t get_time_ms(){
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	uint64_t ts = t.tv_sec*1000;
	ts += t.tv_nsec/1000000;
	return (int32_t) (ts & 0x7FFFFFFF);
}



// UNIX ================================================================================================================
#else
// ARM =================================================================================================================
//todo: implement FreeRTOS get_time_ms()
#endif
// ARM =================================================================================================================
