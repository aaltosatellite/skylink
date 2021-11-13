project(gs C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-g -Wall -Wextra)

SET(SUO_INCLUDE_DIRS ../../suo.git/libsuo)

find_package(Suo REQUIRED)


add_executable(gs
    main.c
    vc_interface.c
    modem_suo.c
)

include_directories( gs
    ${skylink_SOURCE_DIR}
    ${SUO_INCLUDE_DIRS}
    ${ZMQ_INCLUDE_DIRS}
)

target_link_libraries( gs
    skylink ${skylink_LIBRARIES}
    ${SUO_LIBRARIES}
    ${ZMQ_LIBRARIES}
    pthread
    m
)
