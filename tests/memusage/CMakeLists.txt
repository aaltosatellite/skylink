
include_directories( ${CMAKE_SOURCE_DIR}/src/ )
include_directories( ${CMAKE_SOURCE_DIR}/tests/ )


add_executable(test_memusage

    # Recompile the whole skylink with different malloc definition
    ../src/reliable_vc.c
    ../src/diag.c
    ../src/elementbuffer.c
    ../src/fec.c
    ../src/frame.c
    ../src/hmac.c
    ../src/mac.c
    ../src/sequence_ring.c
    ../src/skylink_rx.c
    ../src/skylink_tx.c
    ../src/utilities.c

    test_memusage.c
#    crc/crc_algos.c
    tools/tools.c
    tst_utilities.c
)
