
add_executable(unit_tests2
    "frame_tests.c"
    "fec_tests.c"
    "hmac_tests.c"
    "../utils/tools.c"
    "units.c"
    "narwhal.c"
)

target_link_libraries(
    unit_tests2 PRIVATE
    skylink
)

target_compile_options(
    unit_tests2 PRIVATE
    -g -O0 -Wall -Wextra
)

target_include_directories(
    unit_tests2 PRIVATE
    "../utils"
)
