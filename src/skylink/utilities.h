//
// Created by elmore on 25.10.2021.
//

#ifndef SKYLINK_CMAKE_UTILITIES_H
#define SKYLINK_CMAKE_UTILITIES_H

#include <stdint.h>

#include <time.h>
#include "platform.h"

uint16_t sky_hton16(uint16_t vh);

uint16_t sky_ntoh16(uint16_t vn);


uint32_t sky_hton32(uint32_t vh);

uint32_t sky_ntoh32(uint32_t vn);


int32_t get_time_ms();

int positive_modulo(int x, int m);


// radio =====================================================================================================================
SkyRadio* new_radio();

void set_radio_rx(SkyRadio* radio);

void set_radio_tx(SkyRadio* radio);

void radio_transmit(SkyRadio* radio, void* data, int length);

void radio_receive(SkyRadio* radio, void* data, int* length);
// radio =====================================================================================================================

#endif //SKYLINK_CMAKE_UTILITIES_H
