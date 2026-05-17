cmake_minimum_required(VERSION 3.20)
project(Dock_WMac VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Widgets Svg)

# 平台判断：选择对应 SysHelper 实现
if(WIN32)
    set(SYS_SRC src/core/SysHelper_win.cpp)
else()
    set(SYS_SRC src/core/SysHelper_linux.cpp)
    find_package(X11 REQUIRED)
endif()

add_executable(dock_wmac
    src/main.cpp
    src/core/DockManager.cpp
    src/core/ConfigManager.cpp
    src/core/SysHelper.cpp
    src/core/Logger.cpp
    ${SYS_SRC}
    src/ui/DockWindow.cpp
    src/ui/DockItem.cpp
    src/ui/AnimationHandler.cpp
    include/core/Types.h
    include/core/DockManager.h
    include/core/ConfigManager.h
    include/core/SysHelper.h
    include/core/Logger.h
    include/ui/DockWindow.h
    include/ui/DockItem.h
    include/ui/AnimationHandler.h
    resources/resources.qrc
)

target_include_directories(dock_wmac PRIVATE include)
target_link_libraries(dock_wmac Qt5::Widgets Qt5::Svg)
if(NOT WIN32)
    target_link_libraries(dock_wmac ${X11_LIBRARIES})
    target_include_directories(dock_wmac PRIVATE ${X11_INCLUDE_DIR})
endif()

# 可选测试
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
