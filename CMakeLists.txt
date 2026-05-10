# CMakeLists.txt — Build configuration for the BirdNET VAMP plugin.
#
# Builds a shared library (.so) that implements the VAMP plugin interface,
# allowing Audacity and Sonic Visualiser VAMP hosts to load and run BirdNET inference.
#
# This build is configured to improve binary portability on Linux x86_64
# glibc-based distributions by:
#   - linking the VAMP SDK statically, when available;
#   - linking libstdc++ statically;
#   - linking libgcc statically;
#   - keeping glibc dynamically linked, which is the recommended approach
#     for Linux shared libraries and plugins.
#
# Requirements:
#   - CMake >= 3.16
#   - A C++17-compatible compiler
#   - VAMP Plugin SDK headers
#   - Static VAMP SDK library: libvamp-sdk.a
#
# Usage:
#   mkdir build && cd build
#   cmake .. -DCMAKE_BUILD_TYPE=Release
#   make -j$(nproc)

cmake_minimum_required(VERSION 3.16)
project(BirdNetVamp)

# Locate the static VAMP SDK library.
#
# Linking against libvamp-sdk.a avoids requiring users to have
# libvamp-sdk.so installed on their systems when loading the plugin.
find_library(VAMP_SDK_STATIC
    NAMES libvamp-sdk.a
    PATHS /usr/lib/x86_64-linux-gnu /usr/lib64 /usr/lib
    REQUIRED
)

# Build the BirdNET VAMP plugin as a shared library (.so).
add_library(birdnet-vamp SHARED
    BirdNetPlugin.cpp
)

# Include the VAMP SDK headers.
target_include_directories(birdnet-vamp PRIVATE
    /usr/include/vamp-sdk
)

# Link against the static VAMP SDK library.
target_link_libraries(birdnet-vamp
    ${VAMP_SDK_STATIC}
)

# Link libstdc++ and libgcc statically to reduce runtime dependencies.
#
# glibc is intentionally not linked statically. Keeping glibc dynamic
# is generally safer and more compatible for Linux plugins loaded by
# host applications such as Audacity or Sonic Visualiser.
target_link_options(birdnet-vamp PRIVATE
    -static-libstdc++
    -static-libgcc
    -Wl,--as-needed
)

# Plugin output and C++ configuration.
set_target_properties(birdnet-vamp PROPERTIES
    PREFIX ""                       # Output: birdnet-vamp.so, not libbirdnet-vamp.so
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)