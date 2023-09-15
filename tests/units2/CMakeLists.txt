
add_executable(unit_tests2
    "frame_tests.c"
    "../utils/tools.c"
    "../utils/tst_utilities.c"
    "narwhal.c"
)

target_link_libraries(
    unit_tests2 PRIVATE
    skylink
)

target_compile_options(
    unit_tests2 PRIVATE
    -O2 -Wall -Wextra
)

target_include_directories(
    unit_tests2 PRIVATE
    "../utils"
)
