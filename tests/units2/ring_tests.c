#include "units.h"

// Suppress warnings about overflow in the tests. Overflow is intentional.
#pragma GCC diagnostic ignored "-Woverflow"

// Helper function to check that a receive ring is created/wiped correctly
void check_rcv_ring(SkyRcvRing *rcv_ring, int length, int horizon_width, int original_sequence)
{
    /*
    Receive ring assertions.
    */
    ASSERT_NE(rcv_ring, NULL);
    ASSERT(rcv_ring->length == length);
    ASSERT(rcv_ring->head_sequence == original_sequence);
    ASSERT(rcv_ring->tail_sequence == original_sequence);
    ASSERT(rcv_ring->head == 0);
    ASSERT(rcv_ring->tail == 0);
    ASSERT(rcv_ring->horizon_width == horizon_width);
    ASSERT(rcv_ring->storage_count == 0);

    //Memory of rcv_ring->buff should be allocated and set to 0, indexes should be EB_NULL_IDX (65532).
    for (int i = 0; i < rcv_ring->length; i++)
    {
        ASSERT(rcv_ring->buff[i].idx == EB_NULL_IDX, "Index %d at recieve ring buffer index %d is not EB_NULL_IDX (65532).", rcv_ring->buff[i].idx, i);
        ASSERT(rcv_ring->buff[i].sequence == 0, "Sequence %d at recieve ring buffer index %d is not zero.", rcv_ring->buff[i].sequence, i);
    }
}

// Helper function to check that a send ring is created/wiped correctly
void check_send_ring(SkySendRing *send_ring, int length, int original_sequence)
{
    /*
    Send ring assertions.
    */
    ASSERT_NE(send_ring, NULL);
    ASSERT(send_ring->length == length);
    ASSERT(send_ring->head == 0);
    ASSERT(send_ring->tx_head == 0);
    ASSERT(send_ring->tail == 0);
    ASSERT(send_ring->storage_count == 0);
    ASSERT(send_ring->head_sequence == original_sequence);
    ASSERT(send_ring->tail_sequence == original_sequence);
    ASSERT(send_ring->tx_sequence == original_sequence);
    ASSERT(send_ring->resend_count == 0);

    //Memory of send_ring->buff should be allocated and set to 0, indexes should be EB_NULL_IDX (65532).
    for (int i = 0; i < send_ring->length; i++)
    {
        ASSERT(send_ring->buff[i].idx == EB_NULL_IDX, "Index %d at send ring buffer index %d is not EB_NULL_IDX (65532).", send_ring->buff[i].idx, i);
        ASSERT(send_ring->buff[i].sequence == 0, "Sequence %d at send ring buffer index %d is not zero.", send_ring->buff[i].sequence, i);
    }

    // Check that resend_list is empty.
    for (int i = 0; i < ARQ_RESEND_SCHEDULE_DEPTH; i++)
    {
        ASSERT(send_ring->resend_list[i] == 0, "Resend list index %d is not zero.", i);
    }
}


//Test creating rings with various parameters. Check that the rings are created correctly.
//TODO: Make the test have less code duplication.
TEST(create_rings)
{
    // Create a send ring and a receive ring.
    SkyRcvRing *rcv_ring = sky_rcv_ring_create(20, 3, 0);
    SkySendRing *send_ring = sky_send_ring_create(20, 0);



    // Check that the rings are created correctly.

    check_rcv_ring(rcv_ring, 20, 3, 0);
    check_send_ring(send_ring, 20, 0);
 

    //Free the rings.
    sky_rcv_ring_destroy(rcv_ring);
    sky_send_ring_destroy(send_ring);

    //Create rings with different parameters.
    rcv_ring = sky_rcv_ring_create(100, 4, 123);
    send_ring = sky_send_ring_create(100, 123);

    // Check that the rings are created correctly.
    
    check_rcv_ring(rcv_ring, 100, 4, 123);
    check_send_ring(send_ring, 100, 123);

    //Free the rings.
    sky_rcv_ring_destroy(rcv_ring);
    sky_send_ring_destroy(send_ring);


    // Test creating a sequence that naturally wraps around. (Sequence number higher than max of u_int8_t) (300 translates to 44)

    rcv_ring = sky_rcv_ring_create(100, 4, 300);
    send_ring = sky_send_ring_create(100, 300);

    // Check that the rings are created correctly.

    check_rcv_ring(rcv_ring, 100, 4, 44);
    check_send_ring(send_ring, 100, 44);

    //Free the rings.
    sky_rcv_ring_destroy(rcv_ring);
    sky_send_ring_destroy(send_ring);


    //Test creating rings with invalid parameters.

    // LEN < 3
    rcv_ring = sky_rcv_ring_create(2, 4, 0);
    send_ring = sky_send_ring_create(2, 0);

    ASSERT(rcv_ring == NULL);
    ASSERT(send_ring == NULL);

    // LEN < (HORIZON_WIDTH + 3)

    rcv_ring = sky_rcv_ring_create(5, 4, 0);
    //Invalid send ring length (No horizon width)
    send_ring = sky_send_ring_create(2, 0);

    ASSERT(rcv_ring == NULL);
    ASSERT(send_ring == NULL);

}

//Test that the rings are destroyed correctly.
TEST(destroy_rings)
{
    // Create a send ring and a receive ring.
    SkyRcvRing *rcv_ring = sky_rcv_ring_create(20, 3, 0);
    SkySendRing *send_ring = sky_send_ring_create(20, 0);

    // Check that the rings are created correctly.

    check_rcv_ring(rcv_ring, 20, 3, 0);
    check_send_ring(send_ring, 20, 0);

    //Free the rings.
    sky_rcv_ring_destroy(rcv_ring);
    sky_send_ring_destroy(send_ring);

    //TODO: How do I reliably test that the memory was freed? This doesn't seem to be possible by normal means.

}

//Test that the rings are wiped correctly.
TEST(wipe_rings)
{
    // Create a send ring and a receive ring.
    SkyRcvRing *rcv_ring = sky_rcv_ring_create(20, 3, 0);
    SkySendRing *send_ring = sky_send_ring_create(20, 0);


    // Check that the rings are created correctly.

    check_rcv_ring(rcv_ring, 20, 3, 0);
    check_send_ring(send_ring, 20, 0);

    // Create an element buffer for both rings and fill it with some data.
    
    SkyElementBuffer *rcv_eb = sky_element_buffer_create(10,20);

    SkyElementBuffer *send_eb = sky_element_buffer_create(10,20);


    //Fill the element buffers with some data.
    for (int i = 0; i < 20; i++)
    {
        //Create data with length 10 for both element buffers.
        u_int8_t *data = malloc(10);
        u_int8_t *data2 = malloc(10);
        for (int j = 0; j < 10; j++)
        {
            data[j] = i;
            data2[j] = i;
        }
        // Data needs to be const for the element buffer store function.
        const u_int8_t *const_data = data;
        const u_int8_t *const_data2 = data2;

        //Write the data to the element buffers.
        sky_element_buffer_store(rcv_eb, const_data, 10);
        sky_element_buffer_store(send_eb, const_data2, 10);
    }

    // Check that the element buffers are filled correctly. This is not a test for the element buffer, so we don't need extensive testing.
    // Functionality of storing to the element buffer will be tested in another test.
    ASSERT(rcv_eb->free_elements == 0);
    ASSERT(send_eb->free_elements == 0);


    //Change values of the rcv_ring and send_ring to make sure that the wipe function sets them to the correct values.
    rcv_ring->head_sequence = 123;
    rcv_ring->tail_sequence = 123;
    rcv_ring->head = 123;
    rcv_ring->tail = 123;
    rcv_ring->storage_count = 123;

    send_ring->head_sequence = 123;
    send_ring->tail_sequence = 123;
    send_ring->head = 123;
    send_ring->tail = 123;
    send_ring->storage_count = 123;
    send_ring->tx_head = 123;
    send_ring->tx_sequence = 123;
    send_ring->resend_count = 123;
    

    //Wipe the rings.
    sky_rcv_ring_wipe(rcv_ring,NULL,0);
    sky_send_ring_wipe(send_ring,NULL,0);

    // Check that the rings are wiped correctly.

    check_rcv_ring(rcv_ring, 20, 3, 0);
    check_send_ring(send_ring, 20, 0);

    //Free the rings.
    sky_rcv_ring_destroy(rcv_ring);
    sky_send_ring_destroy(send_ring);
}


// Test pushing packets to both rings and reading them.
TEST(push_and_read_packets){
    // Create a send ring and a receive ring.
    SkyRcvRing *rcv_ring = sky_rcv_ring_create(20, 3, 0);
    SkySendRing *send_ring = sky_send_ring_create(20, 0);

    // Check that the rings are created correctly.

    check_rcv_ring(rcv_ring, 20, 3, 0);
    check_send_ring(send_ring, 20, 0);

    // Create an element buffer for both rings and fill it with some data. (Element usable space is 16 bytes.)
    
    SkyElementBuffer *eb = sky_element_buffer_create(20,20);

    ASSERT(eb->free_elements == 20, "Element buffer free elements is not 20, it is %d.", eb->free_elements);

    ASSERT(eb->element_usable_space == 16, "Element buffer usable space is not 16, it is %d.", eb->element_usable_space);


    // Create a payload with a size that will fill 8 elements in the element buffer.

    u_int8_t *pl  = malloc(120);

    // Fill payload.
    for (int i = 0; i < 120; i++)
    {
        pl[i] = i;
    }

    // Data needs to be const to push the packet.
    const u_int8_t *const_pl = pl;
    
    // Push the packet to rcv ring.

    rcvRing_push_rx_packet(rcv_ring, eb, const_pl, 120, 0);

    // Check that the packet was pushed correctly.

    ASSERT(rcv_ring->buff[0].idx == 0);
    ASSERT(rcv_ring->buff[0].sequence == 0);

    // Check that the element buffer is filled correctly.
    ASSERT(eb->free_elements == 12, "Element buffer free elements is not 12, it is %d.", eb->free_elements);


    // Check the data length in the element buffer.
    int len = sky_element_buffer_get_data_length(eb, 0);

    // Check that the data length is correct.
    ASSERT(len == 120, "Data length is not 120, it is %d.", len);

    // Set memory of pl to 0.
    memset(pl, 0, 120);

    // Make sure that the sequence number, head and tail were updated correctly.
    ASSERT(rcv_ring->head_sequence == 1, "Head sequence is not 1, it is %d.", rcv_ring->head_sequence);
    ASSERT(rcv_ring->tail_sequence == 0, "Tail sequence is not 0, it is %d.", rcv_ring->tail_sequence);
    ASSERT(rcv_ring->head == 1, "Head is not 1, it is %d.", rcv_ring->head);
    ASSERT(rcv_ring->tail == 0, "Tail is not 0, it is %d.", rcv_ring->tail);
    ASSERT(rcv_ring->storage_count == 1, "Storage count is not 1, it is %d.", rcv_ring->storage_count);

    // Read the data from the element buffer.
    rcvRing_read_next_received(rcv_ring, eb, pl, 120);

    // Check that the data was read correctly. This also asserts that the data was written correctly.
    for (int i = 0; i < 120; i++)
    {
        ASSERT(pl[i] == i, "Data at index %d is not %d, it is %d.", i, i, pl[i]);
    }

    // Assert that the read elements were removed from the element buffer.
    ASSERT(eb->free_elements == 20, "Element buffer free elements is not 20, it is %d.", eb->free_elements);
    
    // Make sure that the sequence number, head and tail were updated correctly.
    ASSERT(rcv_ring->head_sequence == 1, "Head sequence is not 1, it is %d.", rcv_ring->head_sequence);
    ASSERT(rcv_ring->tail_sequence == 1, "Tail sequence is not 1, it is %d.", rcv_ring->tail_sequence);
    ASSERT(rcv_ring->head == 1, "Head is not 1, it is %d.", rcv_ring->head);
    ASSERT(rcv_ring->tail == 1, "Tail is not 1, it is %d.", rcv_ring->tail);
    ASSERT(rcv_ring->storage_count == 0, "Storage count is not 0, it is %d.", rcv_ring->storage_count);

    // Send ring: Push the packet to send ring.
    sendRing_push_packet_to_send(send_ring, eb, const_pl, 120);

    // Check that the packet was pushed correctly.
    ASSERT(send_ring->buff[0].idx == 7, "Index is not 7, it is %d.", send_ring->buff[0].idx);
    ASSERT(send_ring->buff[0].sequence == 0, "Sequence is not 0, it is %d.", send_ring->buff[0].sequence);

    // Check that the element buffer is filled correctly.
    ASSERT(eb->free_elements == 12, "Element buffer free elements is not 12, it is %d.", eb->free_elements);

    // Check the data length in the element buffer.
    len = sky_element_buffer_get_data_length(eb, 7);

    // Check that the data length is correct.
    ASSERT(len == 120, "Data length is not 120, it is %d.", len);

    // Set memory of pl to 0.
    memset(pl, 0, 120);

    // Read the data from the element buffer.
    sendRing_read_to_tx(send_ring, eb, pl, &send_ring->buff[0].sequence, 0);

    // Check that the data was read correctly. This also asserts that the data was written correctly.
    for (int i = 0; i < 120; i++)
    {
        ASSERT(pl[i] == i, "Data at index %d is not %d, it is %d.", i, i, pl[i]);
    }

    //Make sure that the sequence number, head and tail were updated correctly.
    ASSERT(send_ring->head_sequence == 1, "Head sequence is not 1, it is %d.", send_ring->head_sequence);
    ASSERT(send_ring->tail_sequence == 0, "Tail sequence is not 0, it is %d.", send_ring->tail_sequence);
    ASSERT(send_ring->head == 1, "Head is not 1, it is %d.", send_ring->head);
    ASSERT(send_ring->tail == 0, "Tail is not 0, it is %d.", send_ring->tail);
    ASSERT(send_ring->tx_head == 1, "Tx head is not 1, it is %d.", send_ring->tx_head);
    ASSERT(send_ring->tx_sequence == 1, "Tx sequence is not 1, it is %d.", send_ring->tx_sequence);
    ASSERT(send_ring->storage_count == 1, "Storage count is not 0, it is %d.", send_ring->storage_count);

    // Clean the tail up to the head.
    sendRing_clean_tail_up_to(send_ring, eb, 1);

    //Make sure that the sequence number, head and tail were updated correctly.

    ASSERT(send_ring->head_sequence == 1, "Head sequence is not 1, it is %d.", send_ring->head_sequence);
    ASSERT(send_ring->tail_sequence == 1, "Tail sequence is not 1, it is %d.", send_ring->tail_sequence);
    ASSERT(send_ring->head == 1, "Head is not 1, it is %d.", send_ring->head);
    ASSERT(send_ring->tail == 1, "Tail is not 1, it is %d.", send_ring->tail);
    ASSERT(send_ring->storage_count == 0, "Storage count is not 0, it is %d.", send_ring->storage_count);

    // Assert that the read elements were removed from the element buffer.
    ASSERT(eb->free_elements == 20, "Element buffer free elements is not 20, it is %d.", eb->free_elements);

    // Free the element buffer.
    sky_element_buffer_destroy(eb);

    // Free the payload.
    free(pl);

    // Free the rings.
    sky_rcv_ring_destroy(rcv_ring);
    sky_send_ring_destroy(send_ring);
}

// Test wrapping around the ring indexes and naturally wrapping around the sequence number with u_int8_t overflow.
TEST(wrap_around){
    // Create a send ring and a receive ring and push multiple packets to them in order to have packets on both sides of the wrap around.
    
    SkyRcvRing *rcv_ring = sky_rcv_ring_create(8, 3, 250);
    SkySendRing *send_ring = sky_send_ring_create(8, 250);

    // Check that the rings are created correctly.

    check_rcv_ring(rcv_ring, 8, 3, 250);
    check_send_ring(send_ring, 8, 250);

    // Set the head and tail to 5 to have packets on both sides of the wrap around.

    rcv_ring->head = 5;
    rcv_ring->tail = 5;
    send_ring->head = 5;
    send_ring->tail = 5;
    send_ring->tx_head = 5;

    // Create an element buffer for both rings and fill it with some data. (Element usable space is 16 bytes.)

    SkyElementBuffer *eb = sky_element_buffer_create(20,200);

    ASSERT(eb->free_elements == 200, "Element buffer free elements is not 200, it is %d.", eb->free_elements);
    ASSERT(eb->element_usable_space == 16, "Element buffer usable space is not 16, it is %d.", eb->element_usable_space);

    // Create a payload and push 8 different packets to the rings.

    u_int8_t *pl  = malloc(120);

    // Fill payload.
    for (int i = 0; i < 120; i++)
    {
        pl[i] = i;
    }

    // Data needs to be const to push the packet.
    const u_int8_t *const_pl = pl;

    // Push the packets to the rings.
    for (int i = 0; i < 7; i++)
    {
        int pushed = rcvRing_push_rx_packet(rcv_ring, eb, &const_pl[12*i], 12, 250+i);
        int pushed2 = sendRing_push_packet_to_send(send_ring, eb, &const_pl[12*i], 12);
        ASSERT(pushed >= 0, "Packet %d was not pushed to receive ring. Error code: %d", i, pushed);
        ASSERT(pushed2 >= 0, "Packet %d was not pushed to send ring. Error code: %d", i, pushed2);
    }
    int j = 0;
    u_int8_t seq = 250;
    // Check that the packets were pushed correctly.
    for (u_int8_t i = 5; i != 4; i++, j++)
    {
        ASSERT(rcv_ring->buff[i].idx == 2*j, "Index is not %d, it is %d. I: %d", 2*j, rcv_ring->buff[i].idx, i);
        ASSERT(rcv_ring->buff[i].sequence == seq, "Sequence is not %d, it is %d. I: %d", seq, rcv_ring->buff[i].sequence, i);
        ASSERT(send_ring->buff[i].idx == 2*j+1, "Index is not %d, it is %d. I: %d", 2*j+1, send_ring->buff[i].idx, i);
        ASSERT(send_ring->buff[i].sequence == seq, "Sequence is not %d, it is %d. I: %d", seq, send_ring->buff[i].sequence, i);
        seq++;
        if(i == 7)
            i = 255; // Wraps around after i++
    }

    // Check that the element buffer is filled correctly. (200 - 2*7)
    ASSERT(eb->free_elements == 186, "Element buffer free elements is not 4, it is %d.", eb->free_elements);

    // Check the data length in the element buffer for all indices.
    for (int i = 0; i < 8; i++)
    {
        if(i != 4){ // Empty index
            int rcv_len = sky_element_buffer_get_data_length(eb, rcv_ring->buff[i].idx);
            int send_len = sky_element_buffer_get_data_length(eb, send_ring->buff[i].idx);
            ASSERT(rcv_len == 12, "Data length is not 12, it is %d.", rcv_len);
            ASSERT(send_len == 12, "Data length is not 12, it is %d.", send_len);
        }
    }
    // Set memory of pl to 0.
    memset(pl, 0, 120);

    // Read the data from the element buffer.

    for (int i = 0; i < 7; i++)
    {
        int read = rcvRing_read_next_received(rcv_ring, eb, &pl[12*i], 12);
        ASSERT(read >= 0, "Packet %d was not read from receive ring. Error code: %d", i, read);
    }

    // Check that the data was read correctly. This also asserts that the data was written correctly.
    for (int i = 0; i < 84; i++)
    {
        ASSERT(pl[i] == i, "Data at index %d is not %d, it is %d.", i, i, pl[i]);
    }

    // Assert that the read elements were removed from the element buffer.
    ASSERT(eb->free_elements == 193, "Element buffer free elements is not 200, it is %d.", eb->free_elements);

    // Make sure that the sequence number, head and tail were updated correctly.
    ASSERT(rcv_ring->head_sequence == 1, "Head sequence is not 1, it is %d.", rcv_ring->head_sequence);
    ASSERT(rcv_ring->tail_sequence == 1, "Tail sequence is not 1, it is %d.", rcv_ring->tail_sequence);
    ASSERT(rcv_ring->head == 4, "Head is not 4, it is %d.", rcv_ring->head);
    ASSERT(rcv_ring->tail == 4, "Tail is not 4, it is %d.", rcv_ring->tail);
    ASSERT(rcv_ring->storage_count == 0, "Storage count is not 0, it is %d.", rcv_ring->storage_count);

    // Set memory of pl to 0.
    memset(pl, 0, 120);

    // Read the data from the element buffer.
    for (int i = 0; i < 7; i++)
    {
        int read = sendRing_read_to_tx(send_ring, eb, &pl[12*i], &send_ring->buff[i].sequence, 0);
        ASSERT(read >= 0, "Packet %d was not read from send ring. Error code: %d", i, read);
    }


    ASSERT(send_ring->tx_sequence == 1, "Tx sequence is not 1, it is %d.", send_ring->tx_sequence);
    ASSERT(send_ring->tx_head == 4, "Tx head is not 4, it is %d.", send_ring->tx_head);

    // Check that the data was read correctly. This also asserts that the data was written correctly.
    for (int i = 0; i < 84; i++)
    {
        ASSERT(pl[i] == i, "Data at index %d is not %d, it is %d.", i, i, pl[i]);
    }
    // Clean the tail up to the head.
    sendRing_clean_tail_up_to(send_ring, eb, 1);
    //Make sure that the sequence number, head and tail were updated correctly.

    ASSERT(send_ring->head_sequence == 1, "Head sequence is not 1, it is %d.", send_ring->head_sequence);
    ASSERT(send_ring->tail_sequence == 1, "Tail sequence is not 1, it is %d.", send_ring->tail_sequence);
    ASSERT(send_ring->head == 4, "Head is not 4, it is %d.", send_ring->head);
    ASSERT(send_ring->tail == 4, "Tail is not 4, it is %d.", send_ring->tail);
    ASSERT(send_ring->storage_count == 0, "Storage count is not 0, it is %d.", send_ring->storage_count);

    // Assert that the read elements were removed from the element buffer.
    ASSERT(eb->free_elements == 200, "Element buffer free elements is not 200, it is %d.", eb->free_elements);

    // Free the element buffer.
    sky_element_buffer_destroy(eb);

    // Free the payload.
    free(pl);

    // Free the rings.
    sky_rcv_ring_destroy(rcv_ring);
    sky_send_ring_destroy(send_ring);

}

// Send ring: Test that the resend list is filled correctly.
TEST(resend_list){

    // Create a send ring.
    SkyVCConfig config;
	config.send_ring_len = 10;
	config.rcv_ring_len = 10;
	config.horizon_width = 4;
	config.element_size = 64;
	SkyVirtualChannel* vc = sky_vc_create(&config);

    // Check that the send ring is created correctly..
    check_send_ring(vc->sendRing, 10, 0);

    // Add some packets to the ring and have some of them be lost.
    // TODO:
}


// TODO: Test invalid parameters for all functions and test more edge cases.
// Test lost packets.