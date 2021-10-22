#ifndef __SKYLINK_BUF_H__
#define __SKYLINK_BUF_H__

/*
 * Packet buffer with segmentation support.
 *
 * The buffer implementation does not depend on other parts of the protocol
 * code, so it can be tested separately if desired.
 *
 * The implementation is thread safe in the sense that
 * reader and writer can be in different threads.
 * There should, however, be at most one thread writing
 * and one thread reading at a time.
 */

#include <stdint.h>

#define ap_buf_read sky_buf_read
#define ap_buf_write sky_buf_write
#define ap_buf_space sky_buf_space
#define ap_buf_fullness sky_buf_fullness
#define ap_buf_init sky_buf_init

// State of a packet buffer, including its contents
typedef struct ap_buf SkyBuffer_t;

// Flag: this is the first (or only) segment of a packet
#define BUF_FIRST_SEG 1U
// Flag: this is the last (or only) segment of a packet
#define BUF_LAST_SEG  2U


/*
 * Write a segment of a packet to the buffer.
 *
 * If the whole packet is written in this call,
 * flags shall be BUF_FIRST_SEG|BUF_LAST_SEG.
 * If this is the first segment of a segmented packet,
 * flags shall be BUF_FIRST_SEG.
 * If this is the last segment of a segmented packet,
 * flags shall be BUF_LAST_SEG.
 * If this is an intermediate segment,
 * flags shall be 0.
 */
int sky_buf_write(SkyBuffer_t *self, const uint8_t *data, unsigned datalen, unsigned flags);

/*
 * Read a segment of a packet from the buffer.
 *
 * Args:
 */
int sky_buf_read(SkyBuffer_t *self, uint8_t *data, unsigned maxlen, unsigned *flags);

/*
 * Return amount of free space available for writing
 */
unsigned sky_buf_space(SkyBuffer_t *self);

/*
 * Return amount of data available for reading
 */
unsigned sky_buf_fullness(SkyBuffer_t *self);

/*
 * Flush/empty the buffer
 */
int sky_buf_flush(SkyBuffer_t *self);

/*
 * Allocate and initialize a packet buffer.
 *
 * params:
 *    size: the amount of memory allocated for the contents.
 */
SkyBuffer_t* sky_buf_init(unsigned size);


#endif /* __SKYLINK_BUF_H__ */
