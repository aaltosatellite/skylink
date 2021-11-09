project(tests C)

set(CMAKE_C_STANDARD 11)

include_directories(${skylink_SOURCE_DIR})
link_libraries(skylink ${ZMQ_LIBRARIES} pthread m)


add_executable(tests
    testmain.c
    crc/crc_algos.c
    tools/tools.c
    elebuffer_tests.c
    ring_test.c
    packet_encode_test.c
    tst_utilities.c
    fec_test.c
    hmac_tests.c
    tx_rx_cycle_test.c
    zmq_trial.c
    tx_rx_zmq.c
)
