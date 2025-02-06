cmake_minimum_required(VERSION 3.16)
project(skylink C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

set(CMAKE_BUILD_TYPE Debug) # Force debug mode




find_package(ZMQ REQUIRED)


add_compile_options(-O2)
add_compile_options(-g -Wall -Wextra)


add_subdirectory(src)
add_subdirectory(tests)
#add_subdirectory(skymodem)
