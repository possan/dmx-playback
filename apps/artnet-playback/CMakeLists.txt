include_directories(../vendor)

find_package(PkgConfig)
pkg_check_modules(GLIB REQUIRED glib-2.0)
pkg_check_modules(GIO REQUIRED gio-2.0)
pkg_check_modules(GSTREAMER REQUIRED gstreamer-1.0)
pkg_check_modules(GSTREAMVERVIDEO REQUIRED gstreamer-video-1.0)

include_directories(
    ${GLIB_INCLUDE_DIRS}
    ${GIO_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${GSTREAMVERVIDEO_INCLUDE_DIRS}
)

add_executable (
    artnet-playback
    main.cpp
)

target_link_libraries(
    artnet-playback
    ${GLIB_LIBRARIES}
    ${GIO_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMVERVIDEO_LIBRARIES}
)

target_link_directories(
    artnet-playback
    PUBLIC ${GLIB_LIBRARY_DIRS}
    PUBLIC ${GIO_LIBRARY_DIRS}
    PUBLIC ${GSTREAMER_LIBRARY_DIRS}
    PUBLIC ${GSTREAMVERVIDEO_LIBRARY_DIRS}
)

