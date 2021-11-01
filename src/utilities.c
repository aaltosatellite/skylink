//
// Created by elmore on 25.10.2021.
//

#include "utilities.h"


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


#ifdef __unix__

#include <stdlib.h>
#include <string.h>

int32_t get_time_ms(){
	struct timespec t;
	clock_gettime(CLOCK_REALTIME, &t);
	uint64_t ts = t.tv_sec*1000;
	ts += t.tv_nsec/1000000;
	return (int32_t) (ts & 0x7FFFFFFF);
}



// radio =====================================================================================================================
SkyRadio* new_radio(){
	SkyRadio* radio = malloc(sizeof(SkyRadio));
	radio->received = 0;
	radio->transmitted = 0;
	radio->mode = RADIO_OFF;
	radio->packets_transmitted_this_cycle = 0;
	return radio;
}


void set_radio_rx(SkyRadio* radio){
	radio->mode = RADIO_RX;
}

void set_radio_tx(SkyRadio* radio){
	radio->mode = RADIO_TX;
	radio->packets_transmitted_this_cycle = 0;
}


void radio_transmit(SkyRadio* radio, void* data, int length){
	memcpy(radio->transmission_buffer, data, length);
	radio->packets_transmitted_this_cycle++;
}

void radio_receive(SkyRadio* radio, void* data, int* length){
	memcpy(data, radio->reception_buffer, radio->received);
	*length = radio->received;
	radio->received = 0;
}
// radio =====================================================================================================================




#else
//todo: implement FreeRTOS get_time_ms()
#endif

int positive_modulo(int x, int m){
	return ((x % m) + m) % m;
}









