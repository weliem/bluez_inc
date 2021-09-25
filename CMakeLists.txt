cmake_minimum_required(VERSION 3.20)
project(test C)

set(CMAKE_C_STANDARD 99)

include(FindPkgConfig)
pkg_check_modules(GLIB glib-2.0 gio-2.0 REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

add_executable(test main.c adapter.h adapter.c device.c device.h uuid.c uuid.h logger.c logger.h utility.c utility.h service.c service.h characteristic.c characteristic.h parser.c parser.h agent.c agent.h)
target_link_libraries(${PROJECT_NAME} ${GLIB_LIBRARIES} m)
