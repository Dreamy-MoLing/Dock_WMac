cmake_minimum_required(VERSION 3.20)
project(Dock_WMac VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

# 平台判断：选择对应 SysHelper 实现
if(WIN32)
    set(SYS_SRC src/core/SysHelper_win.cpp)
else()
    set(SYS_SRC src/core/SysHelper_linux.cpp)
    find_package(X11 REQUIRED)
endif()

add_executable(dock_wmac
    src/main.cpp
    src/core/Application.cpp
    src/core/DockManager.cpp
    src/core/ConfigManager.cpp
    src/core/IPCHelper.cpp
    src/core/ProcessMonitor.cpp
    src/core/SysHelper.cpp
    src/core/Logger.cpp
    ${SYS_SRC}
    src/ui/DockWindow.cpp
    src/ui/DockItem.cpp
    include/core/Types.h
    include/core/Application.h
    include/core/DockManager.h
    include/core/ConfigManager.h
    include/core/IPCHelper.h
    include/core/ProcessMonitor.h
    include/core/SysHelper.h
    include/core/Logger.h
    include/ui/DockWindow.h
    include/ui/DockItem.h
    resources/resources.qrc
)

target_include_directories(dock_wmac PRIVATE include)
target_link_libraries(dock_wmac Qt6::Widgets Qt6::Svg)
if(WIN32)
    target_link_libraries(dock_wmac dwmapi)
else()
    target_link_libraries(dock_wmac ${X11_LIBRARIES})
    target_include_directories(dock_wmac PRIVATE ${X11_INCLUDE_DIR})
endif()

# Cross-compilation: MXE static linking needs explicit transitive dependencies
# Qt6 already handles freetype; only supplement brotli/bz2 for static link
if(MINGW OR CMAKE_CROSSCOMPILING)
    foreach(lib brotlidec brotlicommon bz2)
        find_library(MXE_${lib}_LIB ${lib}
            PATHS /mxe/usr/x86_64-w64-mingw32.static.posix/lib
            NO_DEFAULT_PATH
        )
    endforeach()
    # Force whole-archive to break circular dep: freetype ↔ brotli/bz2
    # Without this, static linker discards brotli symbols before freetype uses them
    if(MXE_brotlidec_LIB AND MXE_brotlicommon_LIB AND MXE_bz2_LIB)
        target_link_options(dock_wmac PRIVATE
            "SHELL:-Wl,--whole-archive ${MXE_brotlidec_LIB} ${MXE_brotlicommon_LIB} ${MXE_bz2_LIB} -Wl,--no-whole-archive"
        )
    endif()
endif()

# 安装规则 (make install)
include(GNUInstallDirs)

install(TARGETS dock_wmac
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(NOT WIN32)
    # .desktop 文件
    install(FILES ${CMAKE_SOURCE_DIR}/resources/dock-wmac.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )
    # 应用图标
    install(FILES ${CMAKE_SOURCE_DIR}/resources/icons/dock-wmac.svg
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
    )
endif()

# 可选测试
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
