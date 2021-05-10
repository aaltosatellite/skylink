/*
 * Packet buffer with segmentation support.
 * The implementation is thread safe in the sense that
 * reader and writer can be in different threads.
 * There should, however, be only one thread writing
 * and one thread reading at a time.
 */

#include "skylink/buf.h"
#include "skylink/diag.h"
//#include "platform/wrapmalloc.h"

struct ap_buf {
	unsigned size;
	/* p_write is used internally by the writer to store
	 * where it is going. With fragmented packets, it may
	 * point to the middle of a packet.
	 *
	 * remaining is used internally by the reader
	 * when reading a packet in fragments.
	 *
	 * p_write2 always points to the byte after the end of a complete
	 * packet, i.e. the place where the next complete packet will be
	 * written. This means p_write2 is advanced only after all fragments
	 * of a packet have been received.
	 *
	 * p_write2 and p_read are used to communicate between
	 * reader and writer. They are volatile for thread safety.
	 *
	 * Note that the implementation assumes that reading and writing
	 * an unsigned int is atomic, which should be true on ARM when
	 * the variable is aligned. Don't use this code on 8-bit
	 * architectures where this probably isn't true.
	 * */
	unsigned p_write, remaining;
	volatile unsigned p_read, p_write2;
	uint8_t data[];
};


/* Return the next index with wrapping */
static inline unsigned next_wrap(unsigned p, unsigned size)
{
	if (p + 1 >= size) return 0;
	else return p + 1;
}


int sky_buf_write(struct ap_buf *self, const uint8_t *data, unsigned datalen, unsigned flags)
{
	SKY_ASSERT(self);
	/* TODO: check if there is space in buffer.
	 * If it runs out of space in the middle of receiving
	 * a segmented packet, the ongoing packet should be discarded
	 * by moving p_write back to p_write2.
	 */
	unsigned p_read = self->p_read;
	unsigned p_write = self->p_write, size = self->size;
	unsigned p_write2 = self->p_write2;

	if (p_write == p_write2) {
		/* If p_write and p_write2 are equal, this should be
		 * the first segment. If it is, start by writing
		 * the length word. If it's not the first segment,
		 * part of the data is missing or an overrun has
		 * happened, so discard the data. */
		if (flags & BUF_FIRST_SEG) {
			self->data[p_write] = datalen >> 8;
			p_write = next_wrap(p_write, size);
			self->data[p_write] = datalen & 0xFF;
			p_write = next_wrap(p_write, size);
		} else {
			return -2;
		}
	} else {
		/* Otherwise, update the length word */
		unsigned lw = (self->data[p_write2] << 8) | self->data[p_write2+1];
		lw += datalen;
		// TODO: handle length word overflow?
		self->data[p_write2] = lw >> 8;
		self->data[p_write2+1] = lw;
	}
	// TODO: convert this loop to more efficient memcpys with wrapping logic
	unsigned i;
	for (i=0; i<datalen; ++i) {
		unsigned p_next = next_wrap(p_write, size);
		if (p_next == p_read) {
			/* Buffer overrun. Abort packet. */
			self->p_write = self->p_write2;
			return -1;
		} else {
			self->data[p_write] = data[i];
			p_write = p_next;
		}
	}
	self->p_write = p_write;
	if (flags & BUF_LAST_SEG) {
		self->p_write2 = p_write;
	}
	return 0;
}


int sky_buf_read(struct ap_buf *self, uint8_t *data, unsigned maxlen, unsigned *flags)
{
	SKY_ASSERT(self && data && flags);

	unsigned p_read = self->p_read, size = self->size;
	unsigned p_write2 = self->p_write2;
	unsigned remaining = self->remaining;
	unsigned ret_flags = 0;

	if (p_read == p_write2) {
		/* Buffer is empty, return no data */
		return -1;
	}

	if (remaining == 0) {
		/* Start of new packet, read length */
		remaining = self->data[p_read] << 8;
		p_read = next_wrap(p_read, size);
		remaining |= self->data[p_read];
		p_read = next_wrap(p_read, size);
		ret_flags |= BUF_FIRST_SEG;
	}

	unsigned i;
	for (i=0; i < maxlen && i < remaining; ++i) {
		data[i] = self->data[p_read];
		p_read = next_wrap(p_read, size);
	}
	self->p_read = p_read;
	// at this point, i indicates the number of bytes read
	remaining -= i;
	self->remaining = remaining;
	if (remaining == 0) {
		/* Last segment, packet completely read */
		ret_flags |= BUF_LAST_SEG;
	}
	*flags = ret_flags;
	return i;
}

int sky_buf_peek(struct ap_buf *self) {
	return 0;
}

unsigned sky_buf_space(struct ap_buf *self)
{
	SKY_ASSERT(self);
	unsigned size = self->size;
	unsigned p_read = self->p_read, p_write = self->p_write;
	return (size * 2 + p_read - p_write - 1) % size;
}


unsigned sky_buf_fullness(struct ap_buf *self)
{
	SKY_ASSERT(self);
	unsigned size = self->size;
	unsigned p_read = self->p_read, p_write2 = self->p_write2;
	return (size + p_write2 - p_read) % size;
}

int sky_buf_flush(SkyBuffer_t *self) {
	// TODO
	return 0;
}

struct ap_buf *sky_buf_init(unsigned size)
{
	struct ap_buf *self;
	self = malloc(sizeof(struct ap_buf) + size);
	SKY_ASSERT(self);
	if (self == NULL)
		return NULL;
	self->size = size;
	self->p_read = self->p_write = self->remaining = self->p_write2 = 0;
	return self;
}
