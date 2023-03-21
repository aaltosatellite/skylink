
add_executable(cycle_test
    tx_rx_cycle_test.c

    zmq_endpoint.c
    zmq_endpoint.h

    ../utils/tools.c
    ../utils/tst_utilities.c
)

target_link_libraries( cycle_test PRIVATE skylink ${ZMQ_LIBRARIES} pthread m )
target_compile_options( cycle_test PRIVATE -O2 -Wall -Wextra )
target_include_directories( cycle_test PRIVATE ../utils )