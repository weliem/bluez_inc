cmake_minimum_required(VERSION 3.20)
project(test C)

set(CMAKE_C_STANDARD 99)

include(FindPkgConfig)
pkg_check_modules(GLIB glib-2.0 gio-2.0 REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

add_executable(test main.c adapter.h adapter.c device.c device.h logger.c logger.h utility.c utility.h service.c service.h characteristic.c characteristic.h parser.c parser.h agent.c agent.h forward_decl.h service_handler_manager.c service_handler_manager.h hts_service_handler.c hts_service_handler.h dis_service_handler.c dis_service_handler.h cts_service_handler.c cts_service_handler.h blp_service_handler.c blp_service_handler.h observation_units.h)
target_link_libraries(${PROJECT_NAME} ${GLIB_LIBRARIES} m)
