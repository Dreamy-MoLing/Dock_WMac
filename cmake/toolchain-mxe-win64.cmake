# MXE cross-compilation toolchain for x86_64-w64-mingw32.static
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mxe-win64.cmake \
#         -DCMAKE_BUILD_TYPE=Release -B build_mxe
#   cmake --build build_mxe --parallel $(nproc)

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# MXE root — adjust if your MXE is elsewhere
set(MXE_ROOT "/opt/mxe")

# Cross compiler prefix and tools
set(CMAKE_C_COMPILER   "${MXE_ROOT}/usr/bin/x86_64-w64-mingw32.static-gcc")
set(CMAKE_CXX_COMPILER "${MXE_ROOT}/usr/bin/x86_64-w64-mingw32.static-g++")
set(CMAKE_RC_COMPILER  "${MXE_ROOT}/usr/bin/x86_64-w64-mingw32.static-windres")

set(CMAKE_FIND_ROOT_PATH
    "${MXE_ROOT}/usr/x86_64-w64-mingw32.static"
    "${MXE_ROOT}/usr/x86_64-pc-linux-gnu"
)

# Search for headers/libs in the target sysroot only
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Static build flags
set(CMAKE_EXE_LINKER_FLAGS "-static -static-libgcc -static-libstdc++" CACHE STRING "Static link flags")
set(BUILD_SHARED_LIBS OFF)

# Qt6 platform
set(QT_QPA_PLATFORM_PLUGIN_PATH "${MXE_ROOT}/usr/x86_64-w64-mingw32.static/qt6/plugins")
