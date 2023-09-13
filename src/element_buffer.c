#include "skylink/element_buffer.h"
#include "skylink/utilities.h"

#include "sky_platform.h"

#include <string.h> // memcpy


static int32_t min_i32(int32_t a, int32_t b){
	if (a > b){
		return b;
	}
	return a;
}

/*
static int32_t max_i32(int32_t a, int32_t b){
	if (a < b){
		return b;
	}
	return a;
}
*/

static BufferElement as_element(void* mem)
{
	BufferElement element;
	element.previous = (sky_element_idx_t*)mem;
	element.next = (sky_element_idx_t*)(mem + sizeof(sky_element_idx_t));
	element.data = mem + sizeof(sky_element_idx_t) + sizeof(sky_element_idx_t);
	return element;
}

static sky_element_idx_t wrap_element(SkyElementBuffer* buffer, int32_t idx)
{
	int32_t m = (int32_t) buffer->element_count;
	int32_t r = positive_modulo(idx, m);
	//int32_t r = ((idx % m) + m) % m;
	return (sky_element_idx_t) r;
}

static BufferElement element_i(SkyElementBuffer* elementBuffer, sky_element_idx_t i)
{
	return as_element(elementBuffer->pool + i * elementBuffer->element_size);
}

static int element_is_free(BufferElement element)
{
	if (*element.previous == EB_NULL_IDX && *element.next == EB_NULL_IDX)
		return 1;
	return 0;
}

static int element_is_first(SkyElementBuffer* buffer, BufferElement element)
{
	if (*element.previous == EB_END_IDX && *element.next == EB_END_IDX)
		return 1;
	if (*element.previous == EB_END_IDX && *element.next < buffer->element_count)
		return 1;
	return 0;
}

static int element_is_last(SkyElementBuffer* buffer, BufferElement element)
{
	if (*element.next == EB_END_IDX && *element.previous == EB_END_IDX)
		return 1;
	if (*element.next == EB_END_IDX && *element.previous < buffer->element_count)
		return 1;
	return 0;
}

static int element_is_in_chain(SkyElementBuffer* buffer, BufferElement element)
{
	if (*element.previous == EB_END_IDX && *element.next == EB_END_IDX)
		return 1;
	if (*element.previous < buffer->element_count && *element.next < buffer->element_count)
		return 1;
	if (*element.previous == EB_END_IDX && *element.next < buffer->element_count)
		return 1;
	if (*element.previous < buffer->element_count && *element.next == EB_END_IDX)
		return 1;
	return 0;
}

static void wipe_element(BufferElement element)
{
	*element.previous = EB_NULL_IDX;
	*element.next = EB_NULL_IDX;
}

static int element_buffer_get_next_free(SkyElementBuffer* buffer, sky_element_idx_t start_idx)
{
	for (sky_element_idx_t i = 0; i < buffer->element_count; ++i) {
		sky_element_idx_t idx = wrap_element(buffer, start_idx + i);
		BufferElement element = element_i(buffer, idx);
		if (element_is_free(element))
			return idx;
	}
	return -1;
}

static int element_buffer_get_n_free(SkyElementBuffer* buffer, int32_t n, sky_element_idx_t start_idx, sky_element_idx_t* tgt)
{
	int idx0 = element_buffer_get_next_free(buffer, start_idx);
	if(idx0 < 0)
		return -1;

	tgt[0] = (sky_element_idx_t)idx0;
	for (int i = 1; i < n; ++i) {
		int idx = element_buffer_get_next_free(buffer, wrap_element(buffer, tgt[i - 1] + 1));
		if (idx == idx0)
			return -1;
		if(idx < 0)
			return -1;
		tgt[i] = (sky_element_idx_t)idx;
	}
	return 0;
}

static int chain_is_ok_backwards(SkyElementBuffer* buffer, sky_element_idx_t idx)
{
	BufferElement el = element_i(buffer, idx);
	while (1) {
		if(!element_is_in_chain(buffer, el))
			return 0;
		if(element_is_first(buffer, el))
			return 1;
		sky_element_idx_t idx_old = idx;
		idx = *el.previous;
		el = element_i(buffer, idx);
		if(*el.next != idx_old)
			return 0;
	}
}

static int chain_is_ok_forward(SkyElementBuffer* buffer, sky_element_idx_t idx)
{
	BufferElement el = element_i(buffer, idx);
	while (1) {
		//
		if (!element_is_in_chain(buffer, el))
			return 0;
		if (element_is_last(buffer, el))
			return 1;

		sky_element_idx_t idx_old = idx;
		idx = *el.next;
		el = element_i(buffer, idx);
		if(*el.previous != idx_old)
			return 0;
	}
}


int sky_element_buffer_element_requirement(int32_t element_size, int32_t length)
{
	int32_t usable_size = element_size - (int32_t)(2 * sizeof(sky_element_idx_t));
	int32_t n = (length + EB_LEN_BYTES + usable_size - 1) / usable_size;
	return n;
}


// ==== PUBLIC FUNCTIONS ===============================================================================================

SkyElementBuffer* sky_element_buffer_create(int32_t element_size, int32_t element_count)
{
	SKY_ASSERT(element_count <= EB_MAX_ELEMENT_COUNT);

	SkyElementBuffer* buffer = SKY_MALLOC(sizeof(SkyElementBuffer));
	SKY_ASSERT(buffer != NULL);

	uint8_t* pool = SKY_MALLOC(element_count * element_size);
	SKY_ASSERT(buffer != NULL);

	buffer->pool = pool;
	buffer->last_write_index = 0;
	buffer->free_elements = element_count;
	buffer->element_size = element_size;
	buffer->element_count = element_count;
	buffer->element_usable_space = element_size - (int32_t)(2 * sizeof(sky_element_idx_t));

	sky_element_buffer_wipe(buffer);

	return buffer;
}


void sky_element_buffer_destroy(SkyElementBuffer* buffer)
{
	SKY_FREE(buffer->pool);
	SKY_FREE(buffer);
}


void sky_element_buffer_wipe(SkyElementBuffer* buffer)
{
	for (sky_element_idx_t i = 0; i < buffer->element_count; ++i) {
		BufferElement el = element_i(buffer, i);
		wipe_element(el);
	}
	buffer->last_write_index = 0;
	buffer->free_elements = buffer->element_count;
}


int sky_element_buffer_element_requirement_for(SkyElementBuffer* buffer, int32_t length)
{
	int32_t n = (length + EB_LEN_BYTES + buffer->element_usable_space - 1) / buffer->element_usable_space;
	return n;
}


int sky_element_buffer_store(SkyElementBuffer* buffer, const uint8_t* data, sky_element_length_t length)
{
	// Calculate number of elements required.
	int32_t n_required = sky_element_buffer_element_requirement_for(buffer, length); // A fast ceil-division.
	if (n_required > buffer->free_elements)
		return SKY_RET_EBUFFER_NO_SPACE;

	if (n_required == 0)
		n_required++;

	// Acquire the elements.
	sky_element_idx_t indexes[42]; //todo parametrize this maximum element count used.
	int r = element_buffer_get_n_free(buffer, n_required, buffer->last_write_index, indexes);
	if (r < 0)
		return SKY_RET_EBUFFER_NO_SPACE; // Exit if not enough space

	// Write the metadata to the first element
	BufferElement el0 = element_i(buffer, indexes[0]);
	*el0.previous = EB_END_IDX;
	memcpy(el0.data, &length, sizeof(sky_element_length_t));

	// Copy (USABLE_SPACE - 4) bytes to the first element.
	int32_t to_copy = min_i32(length, buffer->element_usable_space - EB_LEN_BYTES);
	memcpy(el0.data + EB_LEN_BYTES, data, to_copy);
	*el0.next = EB_END_IDX;
	int32_t data_cursor = to_copy;
	BufferElement el = el0;
	for (int i = 1; i < n_required; ++i) {
		*el.next = indexes[i];
		el = element_i(buffer, indexes[i]);
		*el.next = EB_END_IDX;
		*el.previous = indexes[i-1];

		to_copy = min_i32(buffer->element_usable_space, length - data_cursor);
		memcpy(el.data, data+data_cursor, to_copy);
		data_cursor += to_copy;
	}

	buffer->last_write_index = indexes[n_required-1];
	buffer->free_elements -= n_required;
	return indexes[0];
}


int sky_element_buffer_get_data_length(SkyElementBuffer* buffer, sky_element_idx_t idx)
{
	BufferElement el = element_i(buffer, idx);
	if (!element_is_first(buffer, el))
		return SKY_RET_EBUFFER_INVALID_INDEX;

	int32_t len = *(sky_element_length_t*)(el.data);
	return len;
}


int sky_element_buffer_read(SkyElementBuffer* buffer, uint8_t* target, sky_element_idx_t idx, int32_t max_len)
{
	BufferElement el = element_i(buffer, idx);
	if (!element_is_first(buffer, el))
		return SKY_RET_EBUFFER_INVALID_INDEX;

	// check if target buffer is long enough.
	int32_t element_length = *(sky_element_length_t*)(el.data);
	if (element_length > max_len)
		return SKY_RET_EBUFFER_TOO_LONG_PAYLOAD;

	// Read from first element
	int32_t to_read = min_i32(buffer->element_usable_space - EB_LEN_BYTES, element_length);
	memcpy(target, el.data + EB_LEN_BYTES, to_read);

	// Read from successive elements
	BufferElement previous_el = el;
	int32_t cursor = to_read;
	int32_t n_elements = sky_element_buffer_element_requirement_for(buffer, element_length);
	for (int i = 1; i < n_elements; ++i) {
		el = element_i(buffer, *previous_el.next);
		if (element_is_free(el))
			return SKY_RET_EBUFFER_CHAIN_CORRUPTED;

		to_read = min_i32(buffer->element_usable_space, element_length - cursor);
		memcpy(target+cursor, el.data, to_read);
		cursor += to_read;
		previous_el = el;
	}

	if (!element_is_last(buffer, el))
		return SKY_RET_EBUFFER_CHAIN_CORRUPTED;

	return element_length;
}



int sky_element_buffer_delete(SkyElementBuffer* buffer, sky_element_idx_t idx)
{
	BufferElement el = element_i(buffer, idx);
	if (!element_is_first(buffer, el))
		return SKY_RET_EBUFFER_INVALID_INDEX;

	while (1) {
		sky_element_idx_t next = *el.next;
		if(element_is_last(buffer, el)){
			wipe_element(el);
			buffer->free_elements++;
			break;
		}
		wipe_element(el);
		buffer->free_elements++;
		el = element_i(buffer, next);
	}

	return 0;
}


int sky_element_buffer_valid_chain(SkyElementBuffer* buffer, sky_element_idx_t idx)
{
	BufferElement el = element_i(buffer, idx);
	if (!element_is_in_chain(buffer, el))
		return 0;
	int a = chain_is_ok_backwards(buffer, idx);
	int b = chain_is_ok_forward(buffer, idx);
	return a && b;
}


int sky_element_buffer_entire_buffer_is_ok(SkyElementBuffer* buffer)
{
	int counted_free = 0;
	for (sky_element_idx_t i = 0; i < buffer->element_count; ++i) {
		BufferElement el = element_i(buffer, i);
		if(element_is_free(el)){
			counted_free++;
			continue;
		}
		if(sky_element_buffer_valid_chain(buffer, i)){
			continue;
		}
		return 0;
	}
	if(counted_free != buffer->free_elements){
		return 0;
	}
	return 1;
}

// ==== PUBLIC FUNCTIONS ===============================================================================================
