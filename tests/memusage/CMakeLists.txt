
add_executable(test_memusage

    # Recompile the whole skylink with different malloc definition
    ${CMAKE_SOURCE_DIR}/src/reliable_vc.c
    ${CMAKE_SOURCE_DIR}/src/diag.c
    ${CMAKE_SOURCE_DIR}/src/elementbuffer.c
    ${CMAKE_SOURCE_DIR}/src/fec.c
    ${CMAKE_SOURCE_DIR}/src/frame.c
    ${CMAKE_SOURCE_DIR}/src/hmac.c
    ${CMAKE_SOURCE_DIR}/src/mac.c
    ${CMAKE_SOURCE_DIR}/src/sequence_ring.c
    ${CMAKE_SOURCE_DIR}/src/skylink_rx.c
    ${CMAKE_SOURCE_DIR}/src/skylink_tx.c
    ${CMAKE_SOURCE_DIR}/src/utilities.c

    ${CMAKE_SOURCE_DIR}/src/ext/gr-satellites/golay24.c
    ${CMAKE_SOURCE_DIR}/src/ext/blake3/blake3.c
    ${CMAKE_SOURCE_DIR}/src/ext/blake3/blake3_dispatch.c
    ${CMAKE_SOURCE_DIR}/src/ext/blake3/blake3_portable.c
    ${CMAKE_SOURCE_DIR}/src/ext/libfec/ccsds_tab.c
    ${CMAKE_SOURCE_DIR}/src/ext/libfec/decode_rs_8.c
    ${CMAKE_SOURCE_DIR}/src/ext/libfec/encode_rs_8.c

    test_memusage.c
    ../utils/tools.c
    ../utils/tst_utilities.c
)

include_directories( test_memusage . )
include_directories( test_memusage ../utils )
include_directories( test_memusage ${CMAKE_SOURCE_DIR}/src/ )