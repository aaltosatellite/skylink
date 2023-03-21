
add_executable(unit_tests
    units.c
    ../utils/tools.c
    ../utils/tst_utilities.c
    
    elementbuffer_test.c
    ring_test.c
    packet_encode_test.c
    fec_test.c
    hmac_test.c
    arq_test1.c
    arq_test2.c
    tx_test.c
    #rx_test.c
    mac_test.c
)

target_link_libraries(unit_tests PRIVATE skylink m)
target_compile_options(unit_tests PRIVATE -O2 -Wall -Wextra)
target_include_directories(unit_tests PRIVATE ../utils )
