#include "skylink/element_buffer.h"
#include "skylink/utilities.h"

#include "sky_platform.h"

#include <string.h> // memcpy


/*
Implementation for the element buffer which is used for storing the raw data of packets.

The element buffer is a buffer of elements. Each element consists of a header and a data section.
The header consists of two pointers, one to the previous element and one to the next element.
The data section is a byte array of size (element_size - 2 * sizeof(sky_element_idx_t)).

The data is stored in chains of elements. The elements in the chain are linked together by the pointers in the header. 
The first element in the chain has a pointer to the previous element set to EB_END_IDX and the last element in the chain has a pointer to the next element set to EB_END_IDX.
The pointers in the header of the other elements in the chain point to the previous and next elements in the chain.

The first two bytes of the data section of the first element in the chain is used for storing the length of the data in the chain.
*/




//The minimum of two 32 bit integers.
static int32_t min_i32(int32_t a, int32_t b){
	if (a > b){
		return b;
	}
	return a;
}

/*
//The maximum of two 32 bit integers.
static int32_t max_i32(int32_t a, int32_t b){
	if (a < b){
		return b;
	}
	return a;
}
*/

/*
Returns the parameter mem as a BufferElement. The parameter mem is assumed to have its first two bytes as the previous pointer, the next two bytes as the address for the next pointer.
This is followed by the data for the element.
This function is used by element_i.
*/
static BufferElement as_element(void* mem)
{

	//Initialize the return value.
	BufferElement element;

	//Set the pointers to the data.
	element.previous = (sky_element_idx_t*)mem;
	element.next = (sky_element_idx_t*)(mem + sizeof(sky_element_idx_t));
	element.data = mem + sizeof(sky_element_idx_t) + sizeof(sky_element_idx_t);

	//Return the element.
	return element;
}

//Takes index idx and returns a corresponding valid index in the range [0, buffer->element_count).
static sky_element_idx_t wrap_element(SkyElementBuffer* buffer, int32_t idx)
{
	int32_t m = (int32_t) buffer->element_count;
	int32_t r = positive_modulo(idx, m);
	//int32_t r = ((idx % m) + m) % m;
	return (sky_element_idx_t) r;
}

//Returns the ith element in the buffer.
static BufferElement element_i(SkyElementBuffer* elementBuffer, sky_element_idx_t i)
{
	//Give as_element the address of the ith element in the buffer and return the element.
	return as_element(elementBuffer->pool + i * elementBuffer->element_size);
}

//Returns whether the element is free.
static int element_is_free(BufferElement element)
{
	//Check if the previous and next pointers are equal to EB_NULL_IDX.
	if (*element.previous == EB_NULL_IDX && *element.next == EB_NULL_IDX)
		return 1;

	//Element is not free. Return 0.
	return 0;
}

//Returns whether the element is the first in a chain.
static int element_is_first(SkyElementBuffer* buffer, BufferElement element)
{

	//Case 1: The element is the only element in the chain.
	if (*element.previous == EB_END_IDX && *element.next == EB_END_IDX)
		return 1;

	//Case 2: The element is the first element in a chain with more than one element.
	if (*element.previous == EB_END_IDX && *element.next < buffer->element_count)
		return 1;

	//Element is not the first element in a chain. Return 0.
	return 0;
}

//Returns whether the element is the last in a chain.
static int element_is_last(SkyElementBuffer* buffer, BufferElement element)
{

	//Case 1: The element is the only element in the chain.
	if (*element.next == EB_END_IDX && *element.previous == EB_END_IDX)
		return 1;


	//Case 2: The element is the last element in a chain with more than one element.
	if (*element.next == EB_END_IDX && *element.previous < buffer->element_count)
		return 1;


	//Element is not the last element in a chain. Return 0.
	return 0;
}

//Returns whether the element is in a chain.
static int element_is_in_chain(SkyElementBuffer* buffer, BufferElement element)
{

	//Case 1: The element i theonly element in the chain.
	if (*element.previous == EB_END_IDX && *element.next == EB_END_IDX)
		return 1;
	

	//Case 2: The element is in the middle of a chain. (Both previous and next pointers are valid and less than the element count.)
	if (*element.previous < buffer->element_count && *element.next < buffer->element_count)
		return 1;

	//Case 3: The element is the first element in a chain. (The previous pointer is EB_END_IDX and the next pointer is valid.)
	if (*element.previous == EB_END_IDX && *element.next < buffer->element_count)
		return 1;

	//Case 4: The element is the last element in a chain. (The next pointer is EB_END_IDX and the previous pointer is valid.)
	if (*element.previous < buffer->element_count && *element.next == EB_END_IDX)
		return 1;


	//Element is not in a chain. Return 0.
	return 0;
}

//Wipes the element. Sets the previous and next pointers to EB_NULL_IDX.
static void wipe_element(BufferElement element)
{
	*element.previous = EB_NULL_IDX;
	*element.next = EB_NULL_IDX;
}

//Returns the index of the next free element in the buffer.
static int element_buffer_get_next_free(SkyElementBuffer* buffer, sky_element_idx_t start_idx)
{

	//Loop through all elements in the buffer and return the index of the first free element.
	for (sky_element_idx_t i = 0; i < buffer->element_count; ++i) {

		//Get the index of the element and the element at that index.
		sky_element_idx_t idx = wrap_element(buffer, start_idx + i);
		BufferElement element = element_i(buffer, idx);

		//Return the index of the element if it is free.
		if (element_is_free(element))
			return idx;
	}
	//No free element was found, return -1.
	return -1;
}

//Returns the indexes of the next n free elements in the buffer.
static int element_buffer_get_n_free(SkyElementBuffer* buffer, int32_t n, sky_element_idx_t start_idx, sky_element_idx_t* tgt)
{
	//Get the first free element in the buffer. Return -1 if no free element was found.
	int idx0 = element_buffer_get_next_free(buffer, start_idx);
	if (idx0 < 0)
		return -1;

	//Set the first index in the target array to the index of the first free element.
	tgt[0] = (sky_element_idx_t)idx0;

	//Loop through the rest of the elements and set the indexes in the target array to the indexes of the next free elements.
	for (int i = 1; i < n; ++i) {

		//Get the index of the next free element and return -1 if no free element was found or if the index is equal to the index of the first free element.
		int idx = element_buffer_get_next_free(buffer, wrap_element(buffer, tgt[i - 1] + 1));
		if (idx == idx0)
			return -1;
		if(idx < 0)
			return -1;

		//Set the index in the target array to the index of the next free element.
		tgt[i] = (sky_element_idx_t)idx;
	}

	//Return 0 if the indexes of the next n free elements were found.
	return 0;
}

//Returns whether the chain is ok backwards. This means that all elements in the chain have a valid previous pointer until the first element in the chain is reached.
static int chain_is_ok_backwards(SkyElementBuffer* buffer, sky_element_idx_t idx)
{

	//Get the element at the given index.
	BufferElement el = element_i(buffer, idx);

	//Loop through the chain backwards until the first element in the chain is reached or an invalid element is found.
	while (1){

		//Check if the element is in a chain and return 0 if it is not.
		if (!element_is_in_chain(buffer, el))
			return 0;

		//Check if the element is the first element in the chain and return 1 if it is.
		if (element_is_first(buffer, el))
			return 1;

		//Get the index of the previous element and get the element at that index.
		sky_element_idx_t idx_old = idx;
		idx = *el.previous;
		el = element_i(buffer, idx);

		//Check that the next pointer of the new element is equal to the index of the previous element.
		if (*el.next != idx_old)
			return 0;
	}
}

//Returns whether the chain is ok forwards. This means that all elements in the chain have a valid next pointer until the last element in the chain is reached.
static int chain_is_ok_forward(SkyElementBuffer* buffer, sky_element_idx_t idx)
{

	//Get the element at the given index.
	BufferElement el = element_i(buffer, idx);

	//Loop through the chain forwards until the last element in the chain is reached or an invalid element is found.
	while (1){

		//Check if the element is in a chain and return 0 if it is not.
		if (!element_is_in_chain(buffer, el))
			return 0;

		//Check if the element is the last element in the chain and return 1 if it is.
		if (element_is_last(buffer, el))
			return 1;

		//Get the index of the next element and get the element at that index.
		sky_element_idx_t idx_old = idx;
		idx = *el.next;
		el = element_i(buffer, idx);

		//Check that the previous pointer of the new element is equal to the index of the previous element.
		if (*el.previous != idx_old)
			return 0;
	}
}


/*
Returns the number of elements required for storing 'length' bytes.
This differs from sky_element_buffer_element_requirement_for, because it uses a value for element size instead of a buffer given as a parameter.
*/
int sky_element_buffer_element_requirement(int32_t element_size, int32_t length)
{
	int32_t usable_size = element_size - (int32_t)(2 * sizeof(sky_element_idx_t));
	int32_t n = (length + EB_LEN_BYTES + usable_size - 1) / usable_size;
	return n;
}


// ==== PUBLIC FUNCTIONS ===============================================================================================

//Creates a new element buffer and returns a pointer to it. 
SkyElementBuffer* sky_element_buffer_create(int32_t element_size, int32_t element_count)
{
	//Make sure that the element count for the buffer is not larger than the maximum allowed. (65530)
	SKY_ASSERT(element_count <= EB_MAX_ELEMENT_COUNT);

	//Allocate memory for the buffer and assert that it was succesfully allocated. 
	SkyElementBuffer* buffer = SKY_MALLOC(sizeof(SkyElementBuffer));
	SKY_ASSERT(buffer != NULL);

	//Allocate memory for the pool and assert that it was succesfully allocated.
	uint8_t* pool = SKY_MALLOC(element_count * element_size);
	SKY_ASSERT(buffer != NULL);

	//Initialize the buffer.
	buffer->pool = pool;
	buffer->last_write_index = 0;
	buffer->free_elements = element_count;
	buffer->element_size = element_size;
	buffer->element_count = element_count;
	buffer->element_usable_space = element_size - (int32_t)(2 * sizeof(sky_element_idx_t));

	//Erase all data in the buffer and mark all elements as free.
	sky_element_buffer_wipe(buffer);

	return buffer;
}


//Destroys the given element buffer.
void sky_element_buffer_destroy(SkyElementBuffer* buffer)
{
	//Free the pool and the buffer.
	SKY_FREE(buffer->pool);
	SKY_FREE(buffer);
}


//Erases all data in the buffer and marks all elements free.
void sky_element_buffer_wipe(SkyElementBuffer* buffer)
{
	//Loop through all elements in the buffer and wipe them. Wiping means setting the previous and next pointers to EB_NULL_IDX.
	for (sky_element_idx_t i = 0; i < buffer->element_count; ++i) {
		BufferElement el = element_i(buffer, i);
		wipe_element(el);
	}
	//Reset the last write index and the free elements count to their initial values.
	buffer->last_write_index = 0;
	buffer->free_elements = buffer->element_count;
}

/*
Returns the number of elements required for storing 'length' bytes.
This differs from sky_element_buffer_element_requirement, because it uses a buffer given as a parameter instead of a value for element size.
*/
int sky_element_buffer_element_requirement_for(SkyElementBuffer* buffer, int32_t length)
{
	int32_t n = (length + EB_LEN_BYTES + buffer->element_usable_space - 1) / buffer->element_usable_space;
	return n;
}

//Store data to the buffer with the given length. Returns the index of the first element in the chain, or negative error if no space.
int sky_element_buffer_store(SkyElementBuffer* buffer, const uint8_t* data, sky_element_length_t length)
{
	// Calculate number of elements required.
	int32_t n_required = sky_element_buffer_element_requirement_for(buffer, length); // A fast ceil-division.

	//check if there is enough space.
	if (n_required > buffer->free_elements)
		return SKY_RET_EBUFFER_NO_SPACE;


	//make sure that there is at least one element required.
	if (n_required == 0)
		n_required++;

	// Acquire the elements.
	sky_element_idx_t indexes[42]; //todo parametrize this maximum element count used.

	//Store the indexes of the elements next 'n_required' free elements in the indexes array.
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

	//copy the rest of the data to the successive elements.
	for (int i = 1; i < n_required; ++i) {

		//Set the next pointer of the previous element to the index of the current element.
		*el.next = indexes[i];

		//Get the current element.
		el = element_i(buffer, indexes[i]);

		//Initialize the element. Next is set to end index, but is overwritten if this is not the last element.
		*el.next = EB_END_IDX;
		*el.previous = indexes[i-1];

		//Copy the data to the element. Check if the data is longer than the usable space of the element.
		to_copy = min_i32(buffer->element_usable_space, length - data_cursor);
		memcpy(el.data, data+data_cursor, to_copy);

		//Update the data cursor by adding the number of bytes copied.
		data_cursor += to_copy;
	}

	//update the buffer metadata by setting the last write index and subtracting the number of used elements from the free elements count.
	buffer->last_write_index = indexes[n_required-1];
	buffer->free_elements -= n_required;

	//Return the index of the first element added.
	return indexes[0];
}


//Returns the length of the data stored in the chain starting with the given index. The length of the data is stored in the beginning of the first element in the chain.
int sky_element_buffer_get_data_length(SkyElementBuffer* buffer, sky_element_idx_t idx)
{
	//Get the element at the given index.
	BufferElement el = element_i(buffer, idx);

	//Check if the element is the first element in a chain.
	if (!element_is_first(buffer, el))
		return SKY_RET_EBUFFER_INVALID_INDEX;

	//Get the length of the data from the first element and return it.
	int32_t len = *(sky_element_length_t*)(el.data);
	return len;
}


//Reads the data from the chain starting with the given index to the target buffer. Returns the number of bytes read, or negative error if the index is invalid or the target buffer is too small.
int sky_element_buffer_read(SkyElementBuffer* buffer, uint8_t* target, sky_element_idx_t idx, unsigned int max_len)
{

	//Get the element at the given index.
	BufferElement el = element_i(buffer, idx);

	//check if the element is the first element in a chain.
	if(!element_is_first(buffer, el))
		return SKY_RET_EBUFFER_INVALID_INDEX;

	// Check if target buffer is long enough and store the length of the data in the variable leng.
	int32_t element_length = *(sky_element_length_t*)(el.data);
	if (element_length > max_len)
		return SKY_RET_EBUFFER_TOO_LONG_PAYLOAD;

	// Read from first element, if the data is longer than the usable space of the element, read all of the data from the element.
	int32_t to_read = min_i32(buffer->element_usable_space - EB_LEN_BYTES, element_length);
	memcpy(target, el.data + EB_LEN_BYTES, to_read);

	// Set the element that was read to be the previous element. Initialize the cursor to the number of bytes read and get the amount of elements required for storing the data.
	BufferElement previous_el = el;
	int32_t cursor = to_read;
	int32_t n_elements = sky_element_buffer_element_requirement_for(buffer, element_length);

	// Loop through the rest of the elements in the chain and read the data from them.
	for (int i = 1; i < n_elements; ++i) {

		// Get the next element and make sure that the chain is intact.
		el = element_i(buffer, *previous_el.next);
		if (element_is_free(el))
			return SKY_RET_EBUFFER_CHAIN_CORRUPTED;

		//Check how much data to read from the element and read it. If the element is the last element in the chain there might be less data to read than the usable space of the element.
		to_read = min_i32(buffer->element_usable_space, element_length - cursor);
		memcpy(target+cursor, el.data, to_read);

		//Update the cursor and the previous element.
		cursor += to_read;
		previous_el = el;
	}

	//Check that the final element that was read is the last element in the chain.
	if (!element_is_last(buffer, el))
		return SKY_RET_EBUFFER_CHAIN_CORRUPTED;

	//Return the number of bytes read.
	return element_length;
}



//Delete a data chain starting with the given index. Returns negative error if the index is invalid.
int sky_element_buffer_delete(SkyElementBuffer* buffer, sky_element_idx_t idx)
{
	//Get the element at the given index.
	BufferElement el = element_i(buffer, idx);

	//Check if the element is the first element in a chain.
	if (!element_is_first(buffer, el))
		return SKY_RET_EBUFFER_INVALID_INDEX;

	//Loop through elements until the end of the chain is reached.
	while(1){

		//Set a variable for the next element and check if the element is the last element in the chain.
		sky_element_idx_t next = *el.next;
		if(element_is_last(buffer, el)){
			//Wipe the element and increment the free elements count.
			wipe_element(el);
			buffer->free_elements++;
			break;
		}

		//Wipe the element and increment the free elements count.
		wipe_element(el);
		buffer->free_elements++;

		//Get the next element.
		el = element_i(buffer, next);
	}

	//Return 0 if the chain was deleted succesfully.
	return 0;
}



//Returns whether the chain starting with the given index is valid. This is a test function.
int sky_element_buffer_valid_chain(SkyElementBuffer* buffer, sky_element_idx_t idx)
{
	BufferElement el = element_i(buffer, idx);
	if (!element_is_in_chain(buffer, el))
		return 0;
	int a = chain_is_ok_backwards(buffer, idx);
	int b = chain_is_ok_forward(buffer, idx);
	return a && b;
}



/*
Returns whether the buffer is ok. This means that all elements are either free or in a valid chain.
This is a test function.
*/
int sky_element_buffer_entire_buffer_is_ok(SkyElementBuffer* buffer)
{
	//Initialize the number of counted free elements as 0.
	int counted_free = 0;

	//Loop through all elements in the buffer.
	for (sky_element_idx_t i = 0; i < buffer->element_count; ++i) {

		//Get the element at the current index.
		BufferElement el = element_i(buffer, i);

		//If the element is free, increment the counted free elements and continue to the next element.
		if(element_is_free(el)){
			counted_free++;
			continue;
		}

		//If the element is not free, check if it is in a valid chain.
		if(sky_element_buffer_valid_chain(buffer, i)){
			continue;
		}

		//If the element is not free and not in a valid chain, return 0.
		return 0;
	}

	//Make sure that the counted free elements is equal to the free elements count in the buffer.
	if(counted_free != buffer->free_elements){
		return 0;
	}

	//Return 1 if the buffer is ok.
	return 1;
}

// ==== PUBLIC FUNCTIONS ===============================================================================================
