
add_library(Binc
        adapter.c
        advertisement.c
        agent.c
        application.c
        characteristic.c
        descriptor.c
        device.c
        logger.c
        parser.c
        service.c
        utility.c
        )

target_include_directories (Binc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(Binc ${GLIB_LIBRARIES} m)

set(PUBLIC_HEADERS
    adapter.h
    advertisement.h
    agent.h
    application.h
    characteristic.h
    descriptor.h
    device.h
    logger.h
    parser.h
    service.h
    utility.h
)

install(
    TARGETS Binc
    DESTINATION lib
)

install(
    FILES ${PUBLIC_HEADERS}
    DESTINATION include
)
