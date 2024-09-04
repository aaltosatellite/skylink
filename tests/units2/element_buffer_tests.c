/*
 * Tests for the Element Buffer implementation
 */

#include "units.h"

#define ELEMENT_COUNT(byte_len, element_size) (unsigned int)ceil((double)(sizeof(sky_element_length_t) + (byte_len)) / (element_size))


// Helper function for checking a created element buffer
void check_element_buffer(SkyElementBuffer* buff, int usable_element_size, int element_count)
{
	ASSERT(buff != NULL, "Buffer is NULL");
	ASSERT(buff->element_size == usable_element_size+4, "Invalid element size. Expected: %d, got: %d",usable_element_size+4,buff->element_size);
	ASSERT(buff->element_count == element_count, "Invalid element count. Expected: %d, got: %d",element_count,buff->element_count);
	ASSERT(buff->element_usable_space == usable_element_size, "Invalid usable element size. Expected: %d, got: %d",usable_element_size,buff->element_usable_space);
	ASSERT(buff->free_elements == element_count, "Invalid free elements. Expected: %d, got: %d",element_count,buff->free_elements);
	ASSERT(buff->last_write_index == 0, "Invalid last write index. Expected: %d, got: %d",0,buff->last_write_index);
	ASSERT(buff->pool != NULL, "Pool is NULL");
}

// Helper function for shuffling an array.
void shuffle(int *array, size_t n)
{
	if (n < 2)
		return;
	for (size_t i = 0; i < n - 1; i++)
	{
		size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
		int t = array[j];
		array[j] = array[i];
		array[i] = t;
	}
}

// Test the element buffer creation function
TEST(create_element_buffer)
{
	SkyElementBuffer* buff =  sky_element_buffer_create(20, 100);
	check_element_buffer(buff, 20, 100);
	sky_element_buffer_destroy(buff);
}
// NOTE: There are no checks for creating element buffers with zero size or count, should this be checked???

/*
 * Test storing and reading data in the buffer
 */
TEST(store_and_read_data)
{
	// Create a buffer with 16 usable byte elements and 100 elements.
	SkyElementBuffer* buff =  sky_element_buffer_create(16,100);

	// Check the created buffer
	check_element_buffer(buff, 16, 100);

	// Create a buffer of data to store with size > 16 bytes. This should use multiple elements.
	uint8_t writein[20];
	fillrand(writein, sizeof(writein));
	sky_element_buffer_store(buff, writein, sizeof(writein));
	ASSERT(buff->free_elements == 98, "free_elements = %d", buff->free_elements);
	ASSERT(buff->last_write_index == 1, "last_write_index = %d", buff->last_write_index);

	// Go through the buffer and check the data is correct
	uint8_t readout[20];
	int read_len = sky_element_buffer_read(buff, readout, 0, sizeof(readout));
	ASSERT(read_len == 20, "read_len = %d", read_len);
	ASSERT_MEMORY(writein, readout, read_len);

	// Check that the data length function works as intended.
	ASSERT(sky_element_buffer_get_data_length(buff,0) == 20,"Invalid data length. Expected: %d, got: %d",20,sky_element_buffer_get_data_length(buff,0));

	sky_element_buffer_destroy(buff);
}


/*
 * Test the two functions for checking element requirements.
 */
TEST(element_requirements)
{
	return;
	// Element usable space is element size - 4 bytes.
	// Element buffer element requirements is element size + 4 bytes.
	ASSERT(sky_element_buffer_element_requirement(20,20) == 2, "Basic element requirement failed. Expected: %d, got: %d",2,sky_element_buffer_element_requirement(20,20));

	int n = 30000; // Smaller n for faster testing.
	reseed_random();
	// Randomly generate some element sizes and counts n times and check the requirements are correct.
	for (int i = 0; i < n; i++)
	{
		int element_size = (rand() % 10000) + 5; // Zero division error if element size is 4.
		int length = rand() % 10000;
		int usable = element_size - 4;
		int requirements = sky_element_buffer_element_requirement(element_size,length);
		ASSERT(requirements == ((length+usable+1)/usable), "element_size: %d, length: %d, requirements: %d Got value: %d",element_size,length,requirements,((length+usable+1)/usable));
	}
	// Create a buffer n times with a random element size and check the requirements are correct with a random length.
	for(int i = 0; i < n; i++){
		int usable_element_size = (rand() % 10000) + 1; // Zero division error if usable element size is 0.
		int length = rand() % 100000;
		SkyElementBuffer* buff =  sky_element_buffer_create(usable_element_size,100);
		int requirements = sky_element_buffer_element_requirement_for(buff,length);
		ASSERT(requirements == ((length+usable_element_size+1)/usable_element_size), "element_size: %d, length: %d, requirements: %d Got value: %d",usable_element_size,length,requirements,((length+usable_element_size+1)/usable_element_size));
		sky_element_buffer_destroy(buff);
	}
}

/*
 * Test the wipe function, should wipe all elements in the buffer.
 * Store multiple elements and wipe them. Check that the buffer is empty.
 */
TEST(wipe_element_buffer)
{
	// Create a buffer with 16 byte elements and 1000 elements.
	SkyElementBuffer* buff = sky_element_buffer_create(16, 1000);
	check_element_buffer(buff, 16, 1000);

	// Create random packets
	const int packet_count = 100;
	const int packet_size = 20; // Each packets consumes two elements
	uint8_t packets[packet_count * packet_size];
	fillrand(packets, sizeof(packets));

	// Store multiple packets in the buffer.
	for (int i = 0; i < packet_count; i++)
	{
		int store_index = sky_element_buffer_store(buff, &packets[i * packet_size], packet_size);
		ASSERT(store_index >= 0, "sky_element_buffer_store() returned %d", store_index);
		ASSERT(buff->last_write_index == 2*i+1, "Invalid last write index. Expected: %d, got: %d", 2*i+1, buff->last_write_index);
	}

	// Check that the data is stored correctly.
	ASSERT(buff->free_elements == 800, "Invalid free elements. Expected: %d, got: %d", 800, buff->free_elements);
	ASSERT(buff->last_write_index == 199, "Invalid last write index. Expected: %d, got: %d", 199, buff->last_write_index);

	// Loop through the buffer and check that the data is correct.
	for (int i = 0; i < packet_count; i++)
	{
		// Go through the buffer and check the data is correct
		uint8_t readout[200];
		int ret = sky_element_buffer_read(buff, readout, 2 * i, sizeof(readout));
		ASSERT(ret == packet_size, "ret = %d", ret);
		ASSERT_MEMORY(&packets[i * packet_size], readout, packet_size);
	}

	// Wipe the buffer and check that it is empty.
	sky_element_buffer_wipe(buff);
	ASSERT(buff->free_elements == 1000, "Expected free elements: %d, got: %d", 1000, buff->free_elements);
	ASSERT(buff->last_write_index == 0, "Last write index should be 0, got: %d", buff->last_write_index);

	// Loop through the buffer and check that the data is wiped. Reading data should return -110, SKY_RET_EBUFFER_INVALID_INDEX.
	for(int i = 0; i < 100; i++)
	{
		uint8_t readout[200];
		int ret = sky_element_buffer_read(buff, readout, 2 * i, sizeof(readout));
		ASSERT(ret == SKY_RET_EBUFFER_INVALID_INDEX, "There should be no data at index: %d", 2*i);
	}

	sky_element_buffer_destroy(buff);
}

/*
 * Test the deleting elements from the buffer.
 * The test adds different size packets to the buffer and makes sure they can be read back.
 * After this all packets are removed one-by-one in random order making sure existing packets can still read out and deleted cannot be.
 */
TEST(element_deleting, ONLY)
{
	// Create a buffer with 16 byte elements and 1000 elements.
	const unsigned int element_size = 16;
	const unsigned int element_count = 1000;
	SkyElementBuffer *buff = sky_element_buffer_create(element_size, element_count);
	check_element_buffer(buff, element_size, element_count);

	// Create random packets
	const unsigned int packet_count = 100;
	uint8_t *packet_data[packet_count];
	unsigned int packet_len[packet_count];
	unsigned int packet_index[packet_count];

	// Start using the buffer from random position
	buff->last_write_index = randint_u32(0, element_count - 1);

	// Store multiple chains in the buffer.
	unsigned int total_element_count = 0;
	for (unsigned int i = 0; i < packet_count; i++)
	{
		// Create random length packet
		unsigned int len = randint_u32(8, 66);
		packet_len[i] = len;
		packet_data[i] = malloc(len);
		fillrand(packet_data[i], len);

		// Store packet to the buffer
		int store_index = packet_index[i] = sky_element_buffer_store(buff, packet_data[i], len);
		ASSERT(store_index >= 0, "sky_element_buffer_store() returned %d", store_index);

		total_element_count += ELEMENT_COUNT(len, element_size);
		//ASSERT(buff->last_write_index == total_element_count - 1, "Invalid last write index. Expected: %d, got: %d", total_element_count, buff->last_write_index, i);

		//printf("packet_len=%d total_element_count=%d last_write_index=%d free_elements=%d\n", len, total_element_count, buff->last_write_index, buff->free_elements);
	}

	// Check that the data is stored correctly.
	ASSERT(sky_element_buffer_entire_buffer_is_ok(buff) == 1);
	ASSERT(buff->free_elements == element_count - total_element_count, "Invalid free elements. Expected: %d, got: %d", 800, buff->free_elements);
	// ASSERT(buff->last_write_index == 199, "Invalid last write index. Expected: %d, got: %d", 199, buff->last_write_index);

	// Loop through the buffer and check that all the packets are available and correct.
	for (unsigned int i = 0; i < 100; i++)
	{
		uint8_t readout[200];
		int read_len = sky_element_buffer_read(buff, readout, packet_index[i], sizeof(readout));
		ASSERT(read_len > 0, "sky_element_buffer_read() returned %d");
		ASSERT(read_len == (int)packet_len[i], "Read out wrong packet length. %d != %d", read_len, packet_len[i]);
		ASSERT_MEMORY(packet_data[i], readout, packet_len[i]);
	}

	// Try deleting non existing elements
	if(0) { // TODO
		int ret;
		uint8_t readout[200];

		// Try to read which is out range
		ret = sky_element_buffer_read(buff, readout, element_count, sizeof(readout));
		ASSERT(ret == SKY_RET_EBUFFER_INVALID_INDEX, "sky_element_buffer_read() returned %d", ret);

		ret = sky_element_buffer_read(buff, readout, 2*element_count, sizeof(readout));
		ASSERT(ret == SKY_RET_EBUFFER_INVALID_INDEX, "sky_element_buffer_read() returned %d", ret);
	}


	// Delete the chains in a random order and check that the data is correct.
	unsigned int delete_order[packet_count];
	for (unsigned int i = 0; i < packet_count; i++)
		delete_order[i] = packet_index[i];
	shuffle((int*)delete_order, packet_count);

	// Delete packet one-by-one and make sure all packets read correctly
	for (unsigned int i = 0; i < packet_count; i++)
	{
		//printf("Deleting #%d\n", delete_order[i]);

		// Delete packet from the buffer
		int ret = sky_element_buffer_delete(buff, delete_order[i]);
		ASSERT(ret == SKY_RET_OK);
		//ASSERT(buff->free_elements == 800+2*(i+1), "Invalid free elements. Expected: %d, got: %d, with index: %d",800+2*(i+1),buff->free_elements, i);
		ASSERT(sky_element_buffer_entire_buffer_is_ok(buff) == 1);

		// Loop through the buffer and check that the data is correct.
		for (unsigned int j = 0; j < packet_count; j++)
		{
			sky_element_idx_t index = delete_order[j];
			//printf("   j = %d, index = %d\n", j, index);

			if (j <= i) {
				// Make sure elements is not present.
				uint8_t readout[200];
				int read_len = sky_element_buffer_read(buff, readout, index, sizeof(readout));
				ASSERT(read_len == SKY_RET_EBUFFER_INVALID_INDEX, "There should be no data at index: %d", index);
			}
			else
			{
				// Find the list index of the packet
				unsigned int list_index = 0;
				for (; list_index < packet_count; list_index++) {
					if (packet_index[list_index] == index)
						break;
				}

				// Make sure correct data can be read out.
				uint8_t readout[200];
				int read_len = sky_element_buffer_read(buff, readout, index, sizeof(readout));
				ASSERT(read_len > 0, "sky_element_buffer_read() returned %d");
				ASSERT(read_len == (int)packet_len[list_index], "Read out wrong packet length. %d != %d", read_len, packet_len[list_index]);
				ASSERT_MEMORY(packet_data[list_index], readout, read_len);
			}

		}
	}

	// Free all packets
	for (unsigned int i = 0; i < packet_count; i++)
		free(packet_data[i]);

	sky_element_buffer_destroy(buff);
}

// TODO: Implement test storing data to fragmentated
//TODO: Implement tests for using invalid arguments for the functions.