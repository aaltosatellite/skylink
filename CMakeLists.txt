cmake_minimum_required(VERSION 3.20)
project(skylink_cmake C)

set(CMAKE_C_STANDARD 11)

include_directories(gs)
include_directories(platforms)
include_directories(platforms/posix)
include_directories(src/ext/cifra)
include_directories(src/ext/gr-satellites)
include_directories(src/ext/libfec)
include_directories(src/skylink)
include_directories(src/tests)
include_directories(src/tests/crc)
include_directories(src/tests/tools)

add_compile_options(-O2)
add_compile_options(-g)
add_compile_options(-Wall)
add_compile_options(-Wextra)
#add_library(zmq)
#add_library(m)
link_libraries(zmq)
link_libraries(m)

if(FALSE)
add_executable(skylink_gs
        gs/main.c

        gs/modem.h
        gs/modem_suo.c
        gs/suo.h

        gs/vc_interface.c
        gs/vcs.h

        platforms/posix/debug.h
        platforms/posix/timestamp.h
        platforms/posix/wrapmalloc.h
        src/ext/cifra/bitops.h
        src/ext/cifra/blockwise.c
        src/ext/cifra/blockwise.h
        src/ext/cifra/chash.h
        src/ext/cifra/handy.h
        src/ext/cifra/hmac.c
        src/ext/cifra/hmac.h
        src/ext/cifra/sha2.h
        src/ext/cifra/sha256.c
        src/ext/cifra/tassert.h
        src/ext/gr-satellites/golay24.c
        src/ext/gr-satellites/golay24.h
        src/ext/libfec/ccsds_tab.c
        src/ext/libfec/decode_rs.h
        src/ext/libfec/decode_rs_8.c
        src/ext/libfec/encode_rs.h
        src/ext/libfec/encode_rs_8.c
        src/ext/libfec/fec.h
        src/ext/libfec/fixed.h

        src/skylink/conf.h
        src/skylink/diag.h
        src/skylink/endian.h
        src/skylink/platform.h

        src/skylink/elementbuffer.h
        src/elementbuffer.c

        src/skylink/fec.h
        src/fec.c

        src/skylink/hmac.h
        src/hmac.c

        src/skylink/skylink.h
        src/skylink.c

        src/skylink/arq_ring.h
        src/arq_ring.c

        src/tests/crc/crc_algos.h
        src/tests/crc/crc_algos.c

        src/tests/tools/tools.h
        src/tests/tools/tools.c

        src/mac_2.c
        src/skypacket.c
        )
endif()

link_libraries(pthread)

add_executable(elementbuffer_test
        src/tests/testmain.c
        src/tests/crc/crc_algos.c
        src/tests/crc/crc_algos.h
        src/tests/tools/tools.c
        src/tests/tools/tools.h
        src/tests/elebuffer_tests.c
        src/tests/elebuffer_tests.h
        src/tests/ring_test.c
        src/tests/ring_test.h

        src/skylink/utilities.h
        src/utilities.c

        #platforms/posix/debug.h
        #platforms/posix/timestamp.h
        #platforms/posix/wrapmalloc.h
        src/ext/cifra/bitops.h
        src/ext/cifra/blockwise.c
        src/ext/cifra/blockwise.h
        src/ext/cifra/chash.h
        src/ext/cifra/handy.h
        src/ext/cifra/hmac.c
        src/ext/cifra/hmac.h
        src/ext/cifra/sha2.h
        src/ext/cifra/sha256.c
        src/ext/cifra/tassert.h
        src/ext/gr-satellites/golay24.c
        src/ext/gr-satellites/golay24.h
        src/ext/libfec/ccsds_tab.c
        src/ext/libfec/decode_rs.h
        src/ext/libfec/decode_rs_8.c
        src/ext/libfec/encode_rs.h
        src/ext/libfec/encode_rs_8.c
        src/ext/libfec/fec.h
        src/ext/libfec/fixed.h
        src/skylink/conf.h
        src/skylink/diag.h
        src/skylink/elementbuffer.h
        src/skylink/endian.h
        src/skylink/fec.h
        src/skylink/hmac.h
        src/skylink/platform.h
        src/skylink/skylink.h
        src/skylink/arq_ring.h
        src/elementbuffer.c
        src/fec.c
        src/hmac.c
        src/mac_2.c

        #src/skylink.c

        src/arq_ring.c
        src/skypacket.c
        src/skylink/skypacket.h
        src/skylink/mac_2.h
        src/tests/packet_encode_test.c
        src/tests/packet_encode_test.h
        src/tests/tst_utilities.c
        src/tests/tst_utilities.h
        src/diag.c
        src/tests/fec_test.c src/tests/fec_test.h src/skylink_rx.c src/skylink/skylink_rx.h src/skylink_tx.c src/skylink/skylink_tx.h src/skylink_cycle.c src/skylink_cycle.h src/tests/hmac_tests.c src/tests/hmac_tests.h)