cmake_minimum_required(VERSION 3.16)
project(bluez_inc C)

set(CMAKE_C_STANDARD 99)

# Set default build type to Debug if not specified
#if(NOT CMAKE_BUILD_TYPE)
#  set(CMAKE_BUILD_TYPE Debug)
#endif()

message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# Show all the warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror -Wextra -Wno-unused-function -Wno-unused-parameter -Wstrict-prototypes -Wshadow -Wconversion")

include(FindPkgConfig)
pkg_check_modules(GLIB glib-2.0 gio-2.0 REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

add_subdirectory(binc)
add_subdirectory(examples/central)
add_subdirectory(examples/peripheral)

install(
    DIRECTORY include/
    DESTINATION include
)
