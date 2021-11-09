# - Try to find Suo
# Once done this will define
# SUO_FOUND - System has Suo
# SUO_INCLUDE_DIRS - The Suo include directories
# SUO_LIBRARIES - The libraries needed to use Suo
# SUO_DEFINITIONS - Compiler switches required for using Suo

find_path ( SUO_INCLUDE_DIR suo/suo.h )
find_library ( SUO_LIBRARY NAMES suo )

message(WARNING "Found Suo-library" ${SUO_LIBRARY} ${SUO_INCLUDE_DIR})

set ( SUO_LIBRARIES ${SUO_LIBRARY} )
set ( SUO_INCLUDE_DIRS ${SUO_INCLUDE_DIR}/suo )

include ( FindPackageHandleStandardArgs )
# handle the QUIETLY and REQUIRED arguments and set SUO_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args ( Suo DEFAULT_MSG SUO_LIBRARY SUO_INCLUDE_DIR )
