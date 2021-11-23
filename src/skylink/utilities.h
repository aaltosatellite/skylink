//
// Created by elmore on 25.10.2021.
//

#ifndef SKYLINK_CMAKE_UTILITIES_H
#define SKYLINK_CMAKE_UTILITIES_H

#include <stdint.h>
#include <time.h>

#define MOD_TIME_MS		16777216




uint16_t sky_hton16(uint16_t vh);

uint16_t sky_ntoh16(uint16_t vn);

uint32_t sky_hton32(uint32_t vh);

uint32_t sky_ntoh32(uint32_t vn);

int32_t sky_ntohi32(int32_t vn);

int32_t sky_htoni32(int32_t vn);


// == unix ==============================================
int32_t get_time_ms();
void* instr_malloc(size_t n);
void report_allocation();
// == unix ==============================================


int32_t wrap_time_ms(int32_t time_ms);

int positive_modulo(int x, int m);

int x_in_u8_array(uint8_t x, const uint8_t* array, int length);

int x_in_u16_array(uint16_t x, const uint16_t* array, int length);

#endif //SKYLINK_CMAKE_UTILITIES_H
