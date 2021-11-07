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

void sleep_ms(int64_t ms);

void sleep_us(int64_t us);

int positive_modulo(int x, int m);

int x_in_u8_array(uint8_t x, uint8_t* array, int length);


#endif //SKYLINK_CMAKE_UTILITIES_H
