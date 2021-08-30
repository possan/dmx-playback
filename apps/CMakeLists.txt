cmake_minimum_required (VERSION 3.9)
project (dmxplayback)

option(GPIO "Build with GPIO support" OFF)
option(RECORDER "Build recorder application" ON)
option(PLAYBACK "Build playback application" ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (PLAYBACK)
    add_subdirectory(artnet-playback)
endif(PLAYBACK)

if (RECORDER)
    add_subdirectory(artnet-recorder)
endif(RECORDER)
