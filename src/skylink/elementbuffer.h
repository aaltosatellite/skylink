//
// Created by elmore on 8.10.2021.
//

#ifndef SKYLINK_SLOTBUFFER_H
#define SKYLINK_SLOTBUFFER_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>



typedef uint16_t idx_t;
typedef uint16_t pl_len_t;

#define EB_MAX_ELEMENT_COUNT 		65530
#define EB_END_IDX					(EB_MAX_ELEMENT_COUNT + 1)
#define EB_NULL_IDX					(EB_MAX_ELEMENT_COUNT + 2)
#define EB_LEN_BYTES				((int)sizeof(pl_len_t))

struct slot_buffer_s {
	void* pool;
	idx_t last_write_index;
	int32_t free_elements;
	idx_t element_count;
	int32_t element_size;
	int32_t element_usable_space;
};
typedef struct slot_buffer_s ElementBuffer;

struct buffer_element_s {
	idx_t* previous;
	idx_t* next;
	void* data;
};
typedef struct buffer_element_s BufferElement;


/* The obvious */
ElementBuffer* new_element_buffer(int32_t element_size, int32_t element_count);
void destroy_element_buffer(ElementBuffer* buffer);

/* Erases all data in buffer and marks all elements free. */
void wipe_element_buffer(ElementBuffer* buffer);

/* Returns the number of elements required for storing 'length' bytes */
int element_buffer_element_requirement_for(ElementBuffer* buffer, int32_t length);

/* Returns the length of data in index 'idx'. Or negative error if no such data exists. */
int element_buffer_get_data_length(ElementBuffer* buffer, idx_t idx);

/* Stores bytes pointed by 'data' to the buffer, and returns the index address, or negative error if no space. */
int element_buffer_store(ElementBuffer* buffer, uint8_t* data, pl_len_t length);

/* Reads a payload from address index 'idx' that was previously returned by store function.
 * Or returns error if there is no payload or it is too long */
int element_buffer_read(ElementBuffer* buffer, uint8_t* target, idx_t idx, int32_t max_len);

/* Deletes a payload at index 'idx' */
int element_buffer_delete(ElementBuffer* buffer, idx_t idx);



/* Test functions. */
int element_buffer_valid_chain(ElementBuffer* buffer, idx_t idx);
int element_buffer_entire_buffer_is_ok(ElementBuffer* buffer);



#endif //SKYLINK_SLOTBUFFER_H
