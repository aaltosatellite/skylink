#ifndef __SKYLINK_ELEMENT_BUFFER_H__
#define __SKYLINK_ELEMENT_BUFFER_H__

#include "skylink/skylink.h"




#define EB_MAX_ELEMENT_COUNT        (65530)
#define EB_END_IDX                  (EB_MAX_ELEMENT_COUNT + 1)
#define EB_NULL_IDX                 (EB_MAX_ELEMENT_COUNT + 2)
#define EB_LEN_BYTES                ((int)sizeof(sky_element_length_t))


struct sky_element_buffer_s
{
	/* Memory pool used to store elements */
	void* pool;

	/* */
	sky_element_idx_t last_write_index;

	/* Number of free elements */
	int32_t free_elements;

	/* */
	sky_element_idx_t element_count;

	/* */
	int32_t element_size;

	/* */
	int32_t element_usable_space;
};

typedef struct
{
	sky_element_idx_t* previous;
	sky_element_idx_t* next;
	void* data;
} __attribute__((__packed__)) BufferElement;

/*
 * Create a new element buffer
 *
 * Args:
 *     buffer: Element buffer to be destroyed
 */
SkyElementBuffer* sky_element_buffer_create(int32_t element_size, int32_t element_count);

/*
 * Destroy element buffer
 *
 * Args:
 *     buffer: Element buffer to be destroyed
 */
void sky_element_buffer_destroy(SkyElementBuffer* buffer);

/*
 * Erase all data in buffer and marks all elements free.
 *
 * Args:
 *     buffer: Element buffer
 */
void sky_element_buffer_wipe(SkyElementBuffer* buffer);

/*
 * Get the number of elements required for storing 'length' bytes.
 *
 * Args:
 *     buffer: Element buffer
 *     length:
 */
int sky_element_buffer_element_requirement_for(SkyElementBuffer* buffer, int32_t length);

/*
 * Returns the length of data in index 'idx'. Or negative error if no such data exists.
 *
 * Args:
 *     buffer: Element buffer
 *     idx:
 */
int sky_element_buffer_get_data_length(SkyElementBuffer* buffer, sky_element_idx_t idx);

/*
 * Store bytes pointed by 'data' to the buffer, and returns the index address, or negative error if no space.
 *
 * Args:
 *     buffer: Element buffer
 *     data:
 *     length:
 */
int sky_element_buffer_store(SkyElementBuffer* buffer, const uint8_t* data, sky_element_length_t length);

/*
 * Reads a payload from address index 'idx' that was previously returned by store function.
 * Or returns error if there is no payload or it is too long.
 */
int sky_element_buffer_read(SkyElementBuffer* buffer, uint8_t* target, sky_element_idx_t idx, int32_t max_len);

/*
 * Delete a payload at index 'idx'
 *
 * Args:
 *     buffer: Element buffer
 *     idx: Index of the
 */
int sky_element_buffer_delete(SkyElementBuffer* buffer, sky_element_idx_t idx);

/*
 * Utility function for external calculations7
 *
 * Args:
 *     element_size:
 *     length:
 */
int sky_element_buffer_element_requirement(int32_t element_size, int32_t length);


/* Test functions. */
int sky_element_buffer_valid_chain(SkyElementBuffer* buffer, sky_element_idx_t idx);


int sky_element_buffer_entire_buffer_is_ok(SkyElementBuffer* buffer);



#endif //__SKYLINK_ELEMENT_BUFFER_H__
