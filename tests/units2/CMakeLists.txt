
add_executable(unit_tests2
    "frame_tests.c"
    "cycle_test.c"
    "fec_tests.c"
    "hmac_tests.c"
    "ring_tests.c"
    "element_buffer_tests.c"
    "mac_tdd_tests.c"
    "crc_tests.c"
    "rx_tx_tests.c"
    "reliable_vc_tests.c"
    "../utils/tools.c"
    "units.c"
    "narwhal.c"
)

target_link_libraries(
    unit_tests2 PRIVATE
    skylink ${ZMQ_LIBRARIES} pthread m
)

target_compile_options(
    unit_tests2 PRIVATE
    -g -O0 -Wall -Wextra
)

target_include_directories(
    unit_tests2 PRIVATE
    "../utils"
)
