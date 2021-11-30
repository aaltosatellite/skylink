project(tests C)

set(CMAKE_C_STANDARD 11)

include_directories(${skylink_SOURCE_DIR})
link_libraries(skylink ${ZMQ_LIBRARIES} pthread m)


add_compile_options(-O2 -Wall)
add_executable(module_test
        module_tests.c

        ../src/arq_ring.c
        ../src/diag.c
        ../src/elementbuffer.c
        ../src/fec.c
        ../src/frame.c
        ../src/hmac.c
        ../src/mac.c
        ../src/packet_ring.c
        ../src/skylink_rx.c
        ../src/skylink_tx.c
        ../src/utilities.c

        crc/crc_algos.c
        tools/tools.c

        elebuffer_tests.c
        ring_test.c
        packet_encode_test.c
        tst_utilities.c
        fec_test.c
        hmac_tests.c
        arq_tests.c
        arq_tests2.c
        sky_tx_test.c
        #sky_rx_test.c
        mac_test.c
        )


add_executable(cycle_test
        tx_rx_cycle_test.c

        ../src/arq_ring.c
        ../src/diag.c
        ../src/elementbuffer.c
        ../src/fec.c
        ../src/frame.c
        ../src/hmac.c
        ../src/mac.c
        ../src/packet_ring.c
        ../src/skylink_rx.c
        ../src/skylink_tx.c
        ../src/utilities.c

        crc/crc_algos.c
        tools/tools.c
        tst_utilities.c
        )


add_compile_options(-O2 -Wall)
add_executable(zmq_peer
        tx_rx_zmq.c

        ../src/arq_ring.c
        ../src/diag.c
        ../src/elementbuffer.c
        ../src/fec.c
        ../src/frame.c
        ../src/hmac.c
        ../src/mac.c
        ../src/packet_ring.c
        ../src/skylink_rx.c
        ../src/skylink_tx.c
        ../src/utilities.c

        crc/crc_algos.c
        tools/tools.c
        tst_utilities.c
        )


add_compile_options(-O2 -Wall)
add_executable(alloc_test
        alloc_test.c

        ../src/arq_ring.c
        ../src/diag.c
        ../src/elementbuffer.c
        ../src/fec.c
        ../src/frame.c
        ../src/hmac.c
        ../src/mac.c
        ../src/packet_ring.c
        ../src/skylink_rx.c
        ../src/skylink_tx.c
        ../src/utilities.c

        crc/crc_algos.c
        tools/tools.c
        tst_utilities.c
        )
