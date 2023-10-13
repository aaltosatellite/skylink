/*
Tests for element_buffer.c
*/
#include "units.h"

// Suppress warnings about overflow in the tests. Overflow is intentional.
#pragma GCC diagnostic ignored "-Woverflow"

// Helper function for checking a created element buffer
void check_element_buffer(SkyElementBuffer* buff, int usable_element_size, int element_count){
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
    if (n > 1) 
    {
        size_t i;
        for (i = 0; i < n - 1; i++) 
        {
          size_t j = i + rand() / (RAND_MAX / (n - i) + 1);
          int t = array[j];
          array[j] = array[i];
          array[i] = t;
        }
    }
}

// Test the element buffer creation function
TEST(create_element_buffer){
    SkyElementBuffer* buff =  sky_element_buffer_create(20,100);
    check_element_buffer(buff,20,100);
    sky_element_buffer_destroy(buff);
}
// NOTE: There are no checks for creating element buffers with zero size or count, should this be checked???

// Test storing and reading data in the buffer 

TEST(store_and_read_data){
    // Create a buffer with 16 usable byte elements and 100 elements.
    SkyElementBuffer* buff =  sky_element_buffer_create(16,100);

    // Check the created buffer
    check_element_buffer(buff,16,100);

    // Create a buffer of data to store with size > 16 bytes. This should use multiple elements.
    uint8_t *data = malloc(20);

    for (size_t i = 0; i < 20; i++)
    {
        data[i] = i;
    }
    sky_element_buffer_store(buff,data,20);
    ASSERT(buff->free_elements == 98, "Invalid free elements. Expected: %d, got: %d",98, buff->free_elements);
    ASSERT(buff->last_write_index == 1, "Invalid last write index. Expected: %d, got: %d",1,buff->last_write_index);

    // Go through the buffer and check the data is correct
    uint8_t *read_data = malloc(20);
    sky_element_buffer_read(buff,read_data,0,20);
    // Go through the buffer and check the data is correct
    for (size_t i = 0; i < 20; i++)
    {
        ASSERT(read_data[i] == i);
    }
    // Check that the data length function works as intended.
    ASSERT(sky_element_buffer_get_data_length(buff,0) == 20,"Invalid data length. Expected: %d, got: %d",20,sky_element_buffer_get_data_length(buff,0));
    // Free the data
    free(data);
    free(read_data);
    sky_element_buffer_destroy(buff);
}

// Test the two functions for checking element requirements.
TEST(element_requirements){
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

// Test the wipe function, should wipe all elements in the buffer. Store multiple elements and wipe them. Check that the buffer is empty.
TEST(wipe_element_buffer){
    // Create a buffer with 16 usable byte elements and 1000 elements.
    SkyElementBuffer* buff =  sky_element_buffer_create(16,1000);

    // Check the created buffer
    check_element_buffer(buff,16,1000);

    // Store multiple chains in the buffer.
    for(int i = 0; i < 100; i++){
        uint8_t *data = malloc(20);
        for (size_t j = 0; j < 20; j++)
        {
            data[j] = j;
        }
        sky_element_buffer_store(buff,data,20);
        ASSERT(buff->last_write_index == 2*i+1, "Invalid last write index. Expected: %d, got: %d",2*i+1,buff->last_write_index);
        free(data);
    }
    // Check that the data is stored correctly.
    ASSERT(buff->free_elements == 800, "Invalid free elements. Expected: %d, got: %d",800, buff->free_elements);
    ASSERT(buff->last_write_index == 199, "Invalid last write index. Expected: %d, got: %d", 199, buff->last_write_index);

    // Loop through the buffer and check that the data is correct.
    for(int i = 0; i < 100; i++){
        uint8_t *read_data = malloc(20);
        sky_element_buffer_read(buff,read_data,2*i,20);
        // Go through the buffer and check the data is correct
        for (size_t j = 0; j < 20; j++)
        {
            ASSERT(read_data[j] == j, "Invalid data at index I: %d, J: %d. Expected: %d, got: %d",i,j,j,read_data[j]);
        }
        free(read_data);
    }

    // Wipe the buffer and check that it is empty.
    sky_element_buffer_wipe(buff);
    ASSERT(buff->free_elements == 1000, "Expected free elements: %d, got: %d",1000,buff->free_elements);
    ASSERT(buff->last_write_index == 0, "Last write index should be 0, got: %d",buff->last_write_index);
    // Loop through the buffer and check that the data is wiped. Reading data should return -110, SKY_RET_EBUFFER_INVALID_INDEX.
    for(int i = 0; i < 100; i++){
        uint8_t *read_data = malloc(20);
        ASSERT(sky_element_buffer_read(buff,read_data,2*i,20) == SKY_RET_EBUFFER_INVALID_INDEX, "There should be no data at index: %d",2*i);
        free(read_data);
    }
    sky_element_buffer_destroy(buff);
}


/* Test the delete function, should delete a chain of elements starting from the given index.
Have multiple chains and make sure that only the correct chain is deleted.*/
TEST(delete_element){
    // Create a buffer with 16 usable byte elements and 1000 elements.
    SkyElementBuffer* buff =  sky_element_buffer_create(16,1000);

    // Check the created buffer
    check_element_buffer(buff,16,1000);

    // Store multiple chains in the buffer.

    for(int i = 0; i < 100; i++){
        uint8_t *data = malloc(20);
        for (size_t j = 0; j < 20; j++)
        {
            data[j] = j*10;
        }
        sky_element_buffer_store(buff,data,20);
        ASSERT(buff->last_write_index == 2*i+1, "Invalid last write index. Expected: %d, got: %d, at index: %d",2*i+1,buff->last_write_index, i);
        free(data);
    }

    // Check that the data is stored correctly.
    ASSERT(buff->free_elements == 800, "Invalid free elements. Expected: %d, got: %d",800, buff->free_elements);
    ASSERT(buff->last_write_index == 199, "Invalid last write index. Expected: %d, got: %d", 199, buff->last_write_index);

    // Loop through the buffer and check that the data is correct.
    for(int i = 0; i < 100; i++){
        uint8_t *read_data = malloc(20);
        sky_element_buffer_read(buff,read_data,2*i,20);
        // Go through the buffer and check the data is correct
        for (size_t j = 0; j < 20; j++)
        {
            ASSERT(read_data[j] == j*10, "Invalid data at index I: %d, J: %d. Expected: %d, got: %d",i,j,j*100,read_data[j]);
        }
        free(read_data);
    }

    // Delete the chains in a random order and check that the data is correct.
    int *delete_order = malloc(100*sizeof(int));
    for(int i = 0; i < 100; i++){
        delete_order[i] = 2*i;
    }
    shuffle(delete_order,100);

    for(int i = 0; i < 100; i++){
        sky_element_buffer_delete(buff,delete_order[i]);
        ASSERT(buff->free_elements == 800+2*(i+1), "Invalid free elements. Expected: %d, got: %d, with index: %d",800+2*(i+1),buff->free_elements, i);
        // Loop through the buffer and check that the data is correct.
        for(int j = 0; j < 100; j++){
            uint8_t *read_data = malloc(20);
            if(j <= i){
                ASSERT(sky_element_buffer_read(buff,read_data,delete_order[j],20) == SKY_RET_EBUFFER_INVALID_INDEX, "There should be no data at index: %d",delete_order[j]);
            }else{
                sky_element_buffer_read(buff,read_data,delete_order[j],20);
                // Go through the buffer and check the data is correct
                ASSERT(sky_element_buffer_read(buff,read_data,delete_order[j],20) >= 0, "There should be data at index: %d",delete_order[j]);
                for (size_t k = 0; k < 20; k++)
                {
                    ASSERT(read_data[k] == k*10, "Invalid data at index %d. Expected: %d, got: %d",k,k*10,read_data[k]);
                }
            }
            free(read_data);
        }
    }
    // Free the delete order array and destroy the buffer.
    free(delete_order);
    sky_element_buffer_destroy(buff);

}

//TODO: Implement tests for using invalid arguments for the functions.