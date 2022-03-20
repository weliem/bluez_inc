cmake_minimum_required(VERSION 3.16)
project(test C)

set(CMAKE_C_STANDARD 99)

# Show all the warnings
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

include(FindPkgConfig)
pkg_check_modules(GLIB glib-2.0 gio-2.0 REQUIRED)
include_directories(${GLIB_INCLUDE_DIRS})

add_executable(test example/main.c adapter.h adapter.c device.c device.h logger.c logger.h utility.c utility.h service.c service.h characteristic.c characteristic.h parser.c parser.h agent.c agent.h forward_decl.h example/service_handler_manager.c example/service_handler_manager.h example/services/hts_service_handler.c example/services/hts_service_handler.h example/services/dis_service_handler.c example/services/dis_service_handler.h example/services/cts_service_handler.c example/services/cts_service_handler.h example/services/blp_service_handler.c example/services/blp_service_handler.h example/observation_units.h example/observation.c example/observation.h example/observation_units.c example/observation_location.c example/observation_location.h device_internal.h characteristic_internal.h service_internal.h example/device_info.c example/device_info.h example/observation_type.c example/observation_type.h example/services/wss_service_handler.c example/services/wss_service_handler.h example/services/plx_service_handler.c example/services/plx_service_handler.h example/services/plx_features.c example/services/plx_features.h example/services/plx_spot_measurement.c example/services/plx_spot_measurement.h example/services/glx_service_handler.c example/services/glx_service_handler.h example/services/glx_feature.c example/services/glx_feature.h example/services/glx_measurement.c example/services/glx_measurement.h advertisement.c advertisement.h application.c application.h descriptor.c descriptor.h descriptor_internal.h)
target_link_libraries(${PROJECT_NAME} ${GLIB_LIBRARIES} m)
