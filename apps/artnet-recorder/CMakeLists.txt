include_directories(../vendor)

find_package(PkgConfig)
pkg_search_module(libpng REQUIRED libpng)

include_directories(
    ${LIBPNG_INCLUDE_DIRS}
)

add_executable (artnet-recorder
    main.cpp
    artnetserver.cpp
    recorder.cpp
    ${LIBPNG_LINK_FLAGS}
)

target_link_libraries(
     artnet-recorder
     ${LIBPNG_LIBRARIES}
)

target_link_directories(
    artnet-recorder
    PUBLIC ${LIBPNG_LIBRARY_DIRS}
)

