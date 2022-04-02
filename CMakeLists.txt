cmake_minimum_required(VERSION 3.16)
project(test C)

set(CMAKE_C_STANDARD 99)

# Show all the warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Werror")

include(FindPkgConfig)
pkg_check_modules(GLIB glib-2.0 gio-2.0 REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

add_subdirectory(binc)

add_executable(test example/main.c)
# add_executable(peripheral example/peripheral.c)
target_link_libraries(${PROJECT_NAME} ${GLIB_LIBRARIES} m Binc)
