cmake_minimum_required (VERSION 3.9)
project (dmxplayback)

set(CMAKE_SYSROOT /work/rpi-sysroot/sysroot/)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(artnet-playback)
add_subdirectory(artnet-recorder)
