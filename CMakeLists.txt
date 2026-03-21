# CMakeLists.txt — Build configuration for the BirdNET VAMP plugin.
#
# Builds a shared library (.so) that implements the VAMP plugin interface,
# allowing Audacity and other VAMP hosts to load and run BirdNET inference.
#
# Requirements:
#   - CMake >= 3.16
#   - VAMP Plugin SDK (libvamp-sdk, headers in /usr/include/vamp-sdk)
#   - C++17 compatible compiler (g++)
#
# Usage:
#   mkdir build && cd build
#   cmake .. -DCMAKE_BUILD_TYPE=Release
#   make -j$(nproc)

cmake_minimum_required(VERSION 3.16)
project(BirdNetVamp)

add_library(birdnet-vamp SHARED
    BirdNetPlugin.cpp
)

target_include_directories(birdnet-vamp PRIVATE
    /usr/include/vamp-sdk
)

target_link_libraries(birdnet-vamp
    vamp-sdk
)

set_target_properties(birdnet-vamp PROPERTIES
    PREFIX ""           # output: birdnet-vamp.so (no "lib" prefix)
    CXX_STANDARD 17
)
