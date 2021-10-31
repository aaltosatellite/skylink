//
// Created by elmore on 25.10.2021.
//

#ifndef SKYLINK_CMAKE_UTILITIES_H
#define SKYLINK_CMAKE_UTILITIES_H

#include <stdint.h>
#include <time.h>

uint16_t sky_hton16(uint16_t vh);

uint16_t sky_ntoh16(uint16_t vn);


uint32_t sky_hton32(uint32_t vh);

uint32_t sky_ntoh32(uint32_t vn);


int32_t get_time_ms();

int positive_modulo(int x, int m);

#endif //SKYLINK_CMAKE_UTILITIES_H