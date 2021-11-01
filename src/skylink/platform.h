#ifndef __SKY_PLATFORM_H__
#define __SKY_PLATFORM_H__

#include <stdint.h>



#if defined(__unix__) // UNIX/POSIX
#include <time.h>

/* Timestamps in microsecond, 32 bits. (wraps around every 4295 seconds) */
typedef uint32_t timestamp_t;
typedef int32_t timediff_t;
typedef int32_t time_ms_t;

timestamp_t get_timestamp();

#define TIMESTAMP_MS ((timestamp_t)1000)

#define SKY_MALLOC malloc
#define SKY_CALLOC calloc
#define SKY_FREE free

#define RADIO_OFF	0
#define RADIO_RX	1
#define RADIO_TX	2

struct skylink_radio_t {
	uint8_t transmission_buffer[500];
	uint8_t reception_buffer[500];
	int transmitted;
	int received;
	uint8_t mode;
	int packets_transmitted_this_cycle;
};
typedef struct skylink_radio_t SkyRadio;



#else //FreeRTOS


/* Timestamps in microsecond, 32 bits (wraps around every 4295 seconds) */
typedef uint32_t timestamp_t;
typedef int32_t timediff_t;

timestamp_t get_timestamp();

#define TIMESTAMP_MS ((timestamp_t)1000)

#define SKY_MALLOC pvMalloc
#define SKY_CALLOC pvMalloc
#define SKY_FREE pvFree


#endif



#endif /* __SKY_PLATFORM_H__ */
