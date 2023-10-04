#include "units.h"

// Suppress warnings about overflow in the tests. Overflow is intentional.
#pragma GCC diagnostic ignored "-Woverflow"
// Helper function to check that a receive ring is created correctly
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

// Helper function to check that a send ring is created correctly
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