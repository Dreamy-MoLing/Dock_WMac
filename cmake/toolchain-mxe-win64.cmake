# ============================================================================
# Dock_WMac MXE cross-compilation toolchain
# Target: Windows x86_64, static linking, POSIX threading
# Image:  openscad/mxe-x86_64-gui (MXE + Qt6 pre-compiled)
# ============================================================================

# 1. Target system identification
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# 2. MXE root (inside openscad/mxe-x86_64-gui image)
set(MXE_ROOT "/mxe")

# 3. Cross-compiler prefix
set(CMAKE_C_COMPILER   "${MXE_ROOT}/usr/bin/x86_64-w64-mingw32.static.posix-gcc")
set(CMAKE_CXX_COMPILER "${MXE_ROOT}/usr/bin/x86_64-w64-mingw32.static.posix-g++")
set(CMAKE_RC_COMPILER  "${MXE_ROOT}/usr/bin/x86_64-w64-mingw32.static.posix-windres")

# 4. Search path — only look in the cross-compiler sysroot
set(CMAKE_FIND_ROOT_PATH
    "${MXE_ROOT}/usr/x86_64-w64-mingw32.static.posix"
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 5. Static linking flags
set(CMAKE_C_FLAGS_INIT   "-static-libgcc -static")
set(CMAKE_CXX_FLAGS_INIT "-static-libgcc -static-libstdc++ -static")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++ -static")

# 6. Qt6 path inside MXE
list(APPEND CMAKE_PREFIX_PATH
    "${MXE_ROOT}/usr/x86_64-w64-mingw32.static.posix/qt6"
)
