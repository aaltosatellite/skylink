
include_directories( ${CMAKE_SOURCE_DIR}/src/ )
include_directories( ${CMAKE_SOURCE_DIR}/tests/ )


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

    test_memusage.c
    ../tools/tools.c
    ../tst_utilities.c
)
