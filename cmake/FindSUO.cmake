# - Try to find Suo
# Once done this will define
# SUO_FOUND - System has Suo
# SUO_INCLUDE_DIRS - The Suo include directories
# SUO_LIBRARIES - The libraries needed to use Suo
# SUO_DEFINITIONS - Compiler switches required for using Suo

#include(CMakeFindDependencyMacro)

#find_dependency(ZeroMQ REQUIRED)
#find_dependency(LiquidDSP REQUIRED)
#find_dependency(SoapySDR REQUIRED)

include(GNUInstallDirs)

find_library( SUO_LIBRARY
    NAMES suo
    HINTS ${SUO_GIT}/build/libsuo
)

find_path( SUO_INCLUDE_DIR
    suo.h
    HINTS ${SUO_GIT}/libsuo
    HINTS ${PROJECT_BINARY_DIR}/prebuilt/ ${CMAKE_INSTALL_INCLUDEDIR}
)

set(SUO_INCLUDE_DIRS ${SUO_INCLUDE_DIR})
set(SUO_LIBRARIES ${SUO_LIBRARY} liquid zmq )
#set(SUO_DEPENDENCIES liquid zmq)


include ( FindPackageHandleStandardArgs )

# handle the QUIETLY and REQUIRED arguments and set SUO_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(  SUO DEFAULT_MSG
    SUO_LIBRARIES
    SUO_INCLUDE_DIRS )
