//
// Created by elmore on 7.10.2021.
//

#include "skylink/elementbuffer.h"
#include "skylink/platform.h"
#include "skylink/utilities.h"



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

static BufferElement as_element(void* mem){
	BufferElement element;
	element.previous = (idx_t*)mem;
	element.next = (idx_t*)(mem + sizeof(idx_t));
	element.data = mem + sizeof(idx_t) + sizeof(idx_t);
	return element;
}

static idx_t wrap_element(ElementBuffer* buffer, int32_t idx){
	int32_t m = (int32_t) buffer->element_count;
	int32_t r = ((idx % m) + m) % m;
	return (idx_t) r;
}

static BufferElement element_i(ElementBuffer* elementBuffer, idx_t i){
	return as_element(elementBuffer->pool + i * elementBuffer->element_size);
}

static int element_is_free(BufferElement element){
	if((*element.previous == EB_NULL_IDX) && (*element.next == EB_NULL_IDX)){
		return 1;
	}
	return 0;
}

static int element_is_first(ElementBuffer* buffer, BufferElement element){
	if((*element.previous == EB_END_IDX) && (*element.next == EB_END_IDX)){
		return 1;
	}
	if((*element.previous == EB_END_IDX) && (*element.next < buffer->element_count )){
		return 1;
	}
	return 0;
}

static int element_is_last(ElementBuffer* buffer, BufferElement element){
	if((*element.next == EB_END_IDX) && (*element.previous == EB_END_IDX)){
		return 1;
	}
	if((*element.next == EB_END_IDX) && (*element.previous < buffer->element_count )){
		return 1;
	}
	return 0;
}

static int element_is_in_chain(ElementBuffer* buffer, BufferElement element){
	if((*element.previous == EB_END_IDX) && (*element.next == EB_END_IDX) ){
		return 1;
	}
	if( (*element.previous < buffer->element_count) && (*element.next < buffer->element_count) ){
		return 1;
	}
	if((*element.previous == EB_END_IDX) && (*element.next < buffer->element_count) ){
		return 1;
	}
	if( (*element.previous < buffer->element_count) && (*element.next == EB_END_IDX) ){
		return 1;
	}
	return 0;
}

static void wipe_element(BufferElement element){
	*element.previous = EB_NULL_IDX;
	*element.next = EB_NULL_IDX;
}

static int element_buffer_get_next_free(ElementBuffer* buffer, idx_t start_idx){
	for (idx_t i = 0; i < buffer->element_count; ++i) {
		idx_t idx = wrap_element(buffer, start_idx + i);
		BufferElement element = element_i(buffer, idx);
		if(element_is_free(element)){
			return idx;
		}
	}
	return -1;
}

static int element_buffer_get_n_free(ElementBuffer* buffer, int32_t n, idx_t start_idx, idx_t* tgt){
	int idx0 = element_buffer_get_next_free(buffer, start_idx);
	if(idx0 < 0){
		return -1;
	}
	tgt[0] = (idx_t)idx0;
	for (int i = 1; i < n; ++i) {
		int idx = element_buffer_get_next_free(buffer, wrap_element(buffer, tgt[i - 1] + 1));
		if (idx == idx0){
			return -1;
		}
		if(idx < 0){
			return -1;
		}
		tgt[i] = (idx_t)idx;
	}
	return 0;
}

static int chain_is_ok_backwards(ElementBuffer* buffer, idx_t idx){
	BufferElement el = element_i(buffer, idx);
	while (1){
		if(!element_is_in_chain(buffer, el)){
			return 0;
		}
		if(element_is_first(buffer, el)){
			return 1;
		}
		idx_t idx_old = idx;
		idx = *el.previous;
		el = element_i(buffer, idx);
		if(*el.next != idx_old){
			return 0;
		}
	}
}

static int chain_is_ok_forward(ElementBuffer* buffer, idx_t idx){
	BufferElement el = element_i(buffer, idx);
	while (1){
		if(!element_is_in_chain(buffer, el)){
			return 0;
		}
		if(element_is_last(buffer, el)){
			return 1;
		}
		idx_t idx_old = idx;
		idx = *el.next;
		el = element_i(buffer, idx);
		if(*el.previous != idx_old){
			return 0;
		}
	}
}


int element_buffer_element_requirement(int32_t element_size, int32_t length){
	int32_t usable_size = element_size - (int32_t)(2 * sizeof(idx_t));
	int32_t n = (length + EB_LEN_BYTES + usable_size - 1) / usable_size;
	return n;
}


// ==== PUBLIC FUNCTIONS ===============================================================================================
ElementBuffer* new_element_buffer(int32_t element_size, int32_t element_count){
	ElementBuffer* ebuffer = SKY_MALLOC(sizeof(ElementBuffer));
	uint8_t* pool = SKY_MALLOC(element_count * element_size);
	if(element_count > EB_MAX_ELEMENT_COUNT){
		return NULL;
	}
	ebuffer->pool = pool;
	ebuffer->last_write_index = 0;
	ebuffer->free_elements = element_count;
	ebuffer->element_size = element_size;
	ebuffer->element_count = element_count;
	ebuffer->element_usable_space = element_size - (int32_t)(2 * sizeof(idx_t));
	wipe_element_buffer(ebuffer);
	return ebuffer;
}


void destroy_element_buffer(ElementBuffer* buffer){
	SKY_FREE(buffer->pool);
	SKY_FREE(buffer);
}


void wipe_element_buffer(ElementBuffer* buffer){
	for (idx_t i = 0; i < buffer->element_count; ++i) {
		BufferElement el = element_i(buffer, i);
		wipe_element(el);
	}
	buffer->last_write_index = 0;
	buffer->free_elements = buffer->element_count;
}


int element_buffer_element_requirement_for(ElementBuffer* buffer, int32_t length){
	int32_t n = (length + EB_LEN_BYTES + buffer->element_usable_space - 1) / buffer->element_usable_space;
	return n;
}


int element_buffer_store(ElementBuffer* buffer, uint8_t* data, pl_len_t length){
	//calculate number of elements required.
	int32_t n_required = element_buffer_element_requirement_for(buffer, length); //A fast ceil-division.
	if(n_required > buffer->free_elements){
		return EBUFFER_RET_NO_SPACE;
	}
	if(n_required == 0){
		n_required++;
	}

	//acquire the elements.
	idx_t indexes[42]; //todo parametrize this maximum element count used.
	int r = element_buffer_get_n_free(buffer, n_required, buffer->last_write_index, indexes);
	if(r < 0){
		return EBUFFER_RET_NO_SPACE; //exit if not enough space
	}

	//write the metadata to the first element
	BufferElement el0 = element_i(buffer, indexes[0]);
	*el0.previous = EB_END_IDX;
	memcpy(el0.data, &length, sizeof(pl_len_t));

	//copy (USABLE_SPACE - 4) bytes to the first element.
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



int element_buffer_get_data_length(ElementBuffer* buffer, idx_t idx){
	BufferElement el = element_i(buffer, idx);
	if(!element_is_first(buffer, el)){
		return EBUFFER_RET_INVALID_INDEX;
	}
	int32_t len = *(pl_len_t*)(el.data);
	return len;
}



int element_buffer_read(ElementBuffer* buffer, uint8_t* target, idx_t idx, int32_t max_len){
	BufferElement el = element_i(buffer, idx);
	if(!element_is_first(buffer, el)){
		return EBUFFER_RET_INVALID_INDEX;
	}

	//check if target buffer is long enough.
	int32_t leng = *(pl_len_t*)(el.data);
	if(leng > max_len){
		return EBUFFER_RET_TOO_LONG_PAYLOAD;
	}

	//read from first element
	int32_t to_read = min_i32(buffer->element_usable_space - EB_LEN_BYTES, leng);
	memcpy(target, el.data + EB_LEN_BYTES, to_read);

	//read from successive elements
	BufferElement previous_el = el;
	int32_t cursor = to_read;
	int32_t n_elements = element_buffer_element_requirement_for(buffer, leng);
	for (int i = 1; i < n_elements; ++i) {
		el = element_i(buffer, *previous_el.next);
		if(element_is_free(el)){
			return EBUFFER_RET_CHAIN_CORRUPTED;
		}
		to_read = min_i32(buffer->element_usable_space, leng-cursor);
		memcpy(target+cursor, el.data, to_read);
		cursor += to_read;
		previous_el = el;
	}
	if(!element_is_last(buffer, el)){
		return EBUFFER_RET_CHAIN_CORRUPTED;
	}
	return leng;
}



int element_buffer_delete(ElementBuffer* buffer, idx_t idx){
	BufferElement el = element_i(buffer, idx);
	if(!element_is_first(buffer, el)){
		return EBUFFER_RET_INVALID_INDEX;
	}
	while(1){
		idx_t next = *el.next;
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



int element_buffer_valid_chain(ElementBuffer* buffer, idx_t idx){
	BufferElement el = element_i(buffer, idx);
	if(!element_is_in_chain(buffer, el)){
		return 0;
	}
	int a = chain_is_ok_backwards(buffer, idx);
	int b = chain_is_ok_forward(buffer, idx);
	return a && b;
}



int element_buffer_entire_buffer_is_ok(ElementBuffer* buffer){
	int counted_free = 0;
	for (idx_t i = 0; i < buffer->element_count; ++i) {
		BufferElement el = element_i(buffer, i);
		if(element_is_free(el)){
			counted_free++;
			continue;
		}
		if(element_buffer_valid_chain(buffer, i)){
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





