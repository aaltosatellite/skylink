#ifndef __SUO_H__
#define __SUO_H__

#include <stdint.h>

#include "../skylink/skylink.h"


#define RADIOFRAME_MAXLEN    256

/*
 * Suo message types
 */
#define SUO_FRAME_RECEIVE     0x00
#define SUO_FRAME_TRANSMIT    0x01
#define SUO_FRAME_TIMING      0x02
#define SUO_FRAME_CONTROL     0x03

#define SUO_FLAG_  1

struct suohdr {

};

struct suoframe {
	uint32_t id, flags;
	uint64_t time;
	uint32_t metadata[11];
	uint32_t len;
	uint8_t data[RADIOFRAME_MAXLEN];
};

struct suotiming {
	uint32_t id, flags;
	uint64_t time;
};


#endif /* __SUO_H__ */
