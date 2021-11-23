project(skylink C)

set(CMAKE_C_STANDARD 11)

include_directories(skylink) # Shall be removed
include_directories(../platforms/posix)

add_compile_options(-O2 -Wall)

add_library(skylink SHARED
        arq_ring.c
        diag.c
        elementbuffer.c
        fec.c
        frame.c
        hmac.c
        mac.c
        packet_ring.c
        skylink_rx.c
        skylink_tx.c
        utilities.c
        ext/cifra/blockwise.c
        ext/cifra/hmac.c
        ext/cifra/sha256.c
        ext/gr-satellites/golay24.c
        ext/libfec/ccsds_tab.c
        ext/libfec/decode_rs_8.c
        ext/libfec/encode_rs_8.c
        )

target_include_directories( skylink PUBLIC ${PROJECT_SOURCE_DIR} )
