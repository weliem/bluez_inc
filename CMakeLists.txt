cmake_minimum_required(VERSION 3.20)
project(test C)

set(CMAKE_C_STANDARD 99)

include(FindPkgConfig)
pkg_check_modules(GLIB glib-2.0 gio-2.0 REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

add_executable(test example/main.c adapter.h adapter.c device.c device.h logger.c logger.h utility.c utility.h service.c service.h characteristic.c characteristic.h parser.c parser.h agent.c agent.h forward_decl.h example/service_handler_manager.c example/service_handler_manager.h example/hts_service_handler.c example/hts_service_handler.h example/dis_service_handler.c example/dis_service_handler.h example/cts_service_handler.c example/cts_service_handler.h example/blp_service_handler.c example/blp_service_handler.h example/observation_units.h example/observation.c example/observation.h example/observation_units.c example/observation_location.c example/observation_location.h)
target_link_libraries(${PROJECT_NAME} ${GLIB_LIBRARIES} m)
