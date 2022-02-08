project(gs C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-g -Wall -Wextra)

set(SUO_GIT "/home/elmore/skylink/suo/")
find_package(SUO REQUIRED)

add_executable(gs
    main.c
    vc_interface.c
    modem_suo.c
)


target_include_directories( gs PUBLIC
#    ${skylink_SOURCE_DIRS}
    ${SUO_INCLUDE_DIRS}
    ${ZMQ_INCLUDE_DIRS}
)

message(${SUO_LIBRARIES})

target_link_libraries( gs LINK_PUBLIC
    skylink
    ${SUO_LIBRARIES}
    ${ZMQ_LIBRARIES}
    pthread
    m
)
