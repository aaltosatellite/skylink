
#ifndef __SKYLINK_ELEMENT_BUFFER_H__
#define __SKYLINK_ELEMENT_BUFFER_H__

#include "skylink/skylink.h"


typedef uint16_t idx_t;
typedef uint16_t pl_len_t;


#define EB_MAX_ELEMENT_COUNT 		65530
#define EB_END_IDX					(EB_MAX_ELEMENT_COUNT + 1)
#define EB_NULL_IDX					(EB_MAX_ELEMENT_COUNT + 2)
#define EB_LEN_BYTES				((int)sizeof(pl_len_t))




struct sky_element_buffer_s
{
	void* pool;
	idx_t last_write_index;
	int32_t free_elements;
	idx_t element_count;
	int32_t element_size;
	int32_t element_usable_space;
};

typedef struct
{
	idx_t* previous;
	idx_t* next;
	void* data;
} __attribute__((__packed__)) BufferElement;


/* The obvious */
SkyElementBuffer* sky_element_buffer_create(int32_t element_size, int32_t element_count);
void sky_element_buffer_destroy(SkyElementBuffer* buffer);

/* Erases all data in buffer and marks all elements free. */
void sky_element_buffer_wipe(SkyElementBuffer* buffer);

/* Returns the number of elements required for storing 'length' bytes */
int sky_element_buffer_element_requirement_for(SkyElementBuffer* buffer, int32_t length);

/* Returns the length of data in index 'idx'. Or negative error if no such data exists. */
int sky_element_buffer_get_data_length(SkyElementBuffer* buffer, idx_t idx);

/* Stores bytes pointed by 'data' to the buffer, and returns the index address, or negative error if no space. */
int sky_element_buffer_store(SkyElementBuffer* buffer, const uint8_t* data, pl_len_t length);

/* Reads a payload from address index 'idx' that was previously returned by store function.
 * Or returns error if there is no payload or it is too long */
int sky_element_buffer_read(SkyElementBuffer* buffer, uint8_t* target, idx_t idx, int32_t max_len);

/* Deletes a payload at index 'idx' */
int sky_element_buffer_delete(SkyElementBuffer* buffer, idx_t idx);

/* Utility function for external calculations */
int sky_element_buffer_element_requirement(int32_t element_size, int32_t length);


/* Test functions. */
int sky_element_buffer_valid_chain(SkyElementBuffer* buffer, idx_t idx);
int sky_element_buffer_entire_buffer_is_ok(SkyElementBuffer* buffer);



#endif //__SKYLINK_ELEMENT_BUFFER_H__
