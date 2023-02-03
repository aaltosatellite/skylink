project(skymodem CXX)

add_compile_options(-g -Wall -Wextra)

find_package(SUO REQUIRED)

message("SUO_INCLUDE_DIRS" ${SUO_INCLUDE_DIRS})

add_executable(skymodem
    skymodem.cpp
    vc_interface.cpp
)

target_include_directories( skymodem PUBLIC
    ../src
    /home/aalto/suo.git/nlohmann
    ${SUO_INCLUDE_DIRS}
)

target_link_libraries( skymodem LINK_PUBLIC
    skylink
    ${SUO_LIBRARIES}
)

