include_directories(../vendor)

if (GPIO)
    add_definitions(-DGPIO)
endif (GPIO)

find_package(PkgConfig)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMERPLUGBASE REQUIRED gstreamer-plugins-base-1.0)
pkg_check_modules(GSTREAMERVIDEO REQUIRED gstreamer-video-1.0)

include_directories(
    AFTER
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMERPLUGBASE_INCLUDE_DIRS}
    ${GSTREAMERVIDEO_INCLUDE_DIRS}
)

add_executable (
    artnet-playback
    main.cpp
    config.cpp
    player.cpp
    artnetsender.cpp
)

target_link_libraries(
    artnet-playback
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMERPLUGBASE_LIBRARIES}
    ${GSTREAMERVIDEO_LIBRARIES}
)

target_link_directories(
    artnet-playback
    PUBLIC ${GLIB_LIBRARY_DIRS}
    PUBLIC ${GIO_LIBRARY_DIRS}
    PUBLIC ${GSTREAMER_LIBRARY_DIRS}
    PUBLIC ${GSTREAMERPLUGBASE_LIBRARY_DIRS}
    PUBLIC ${GSTREAMERVIDEO_LIBRARY_DIRS}
)
