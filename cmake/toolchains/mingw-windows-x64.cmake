# ============================================================================
# MinGW-w64 cross-compilation toolchain (chainload target for vcpkg)
# Target: Windows x86_64, static linking, POSIX threading
# Used by: VCPKG_CHAINLOAD_TOOLCHAIN_FILE in CMakePresets.json
# ============================================================================

# 1. Target system identification
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# 2. Cross-compilers (POSIX threading variant — required by Qt6)
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)

# 3. Static linking flags
set(CMAKE_C_FLAGS_INIT   "-static-libgcc -static")
set(CMAKE_CXX_FLAGS_INIT "-static-libgcc -static-libstdc++ -static")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++ -static")

# 4. Search path policy — only look in the cross-compiler sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# 5. Allow vcpkg to chainload this file
# VCPKG_CHAINLOAD_TOOLCHAIN_FILE is set by CMakePresets.json
