//
// Created by Admin loop_status 06.09.2018.
//
#ifndef CRC16_H_CRC16
#define CRC16_H_CRC16

#include <inttypes.h>
#include <stdint.h>
#define CRC16BASE 0xFFFF
/*
 * other popular bases:
 * 0x1D0F
 */





uint16_t crc16(const uint8_t* data_p, uint32_t length);

uint16_t crc16_rebase(const uint8_t* data_p, uint32_t length, uint16_t base);

uint32_t crc32(const void *data, uint32_t n_bytes);

uint32_t crc32_rebase(const void *data, uint32_t n_bytes, uint32_t crc0);



#endif //CRC16_H_CRC16

