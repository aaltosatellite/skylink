project(skylink C)

set(CMAKE_C_STANDARD 11)

include_directories(../platforms/posix)


add_library(skylink OBJECT)

target_compile_definitions(
    skylink PRIVATE
    "SKY_DEBUG"
)

target_include_directories(
    skylink PRIVATE
    "${PROJECT_SOURCE_DIR}"
    "${CMAKE_SOURCE_DIR}/platforms/linux/"
)

target_compile_options(
    skylink PRIVATE
    "$<$<CONFIG:Debug>:-g3>"
    "$<$<CONFIG:Debug>:-Og>"
    "$<$<NOT:$<CONFIG:Debug>>:-g0>"
    "$<$<NOT:$<CONFIG:Debug>>:-O2>"
    "-Wall"
    "-Wextra"
    "-fPIC"
)

target_sources(
    skylink PRIVATE
    "reliable_vc.c"
    "diag.c"
    "element_buffer.c"
    "fec.c"
    "frame.c"
    "hmac.c"
    "mac.c"
    "sequence_ring.c"
    "skylink_rx.c"
    "skylink_tx.c"
    "utilities.c"
    "ext/gr-satellites/golay24.c"
    "ext/blake3/blake3.c"
    "ext/blake3/blake3_dispatch.c"
    "ext/blake3/blake3_portable.c"
    "ext/libfec/ccsds_tab.c"
    "ext/libfec/decode_rs_8.c"
    "ext/libfec/encode_rs_8.c"
)

add_library(skylink_shared SHARED $<TARGET_OBJECTS:skylink>)
add_library(skylink_static SHARED $<TARGET_OBJECTS:skylink>)
