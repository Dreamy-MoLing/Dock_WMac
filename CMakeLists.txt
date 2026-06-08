cmake_minimum_required(VERSION 3.20)
project(Dock_WMac VERSION 0.2.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)

add_executable(dock_wmac WIN32
    src/main.cpp
    src/core/Application.cpp
    src/core/DockManager.cpp
    src/core/ConfigManager.cpp
    src/core/ProcessMonitor.cpp
    src/core/SysHelper.cpp
    src/core/WindowCache.cpp
    src/core/ClickStateMachine.cpp
    src/core/IconProvider.cpp
    src/core/Logger.cpp
    src/core/PinnedItemsReader.cpp
    src/ui/DockWindow.cpp
    src/ui/DockItem.cpp
    src/ui/WindowPreviewPanel.cpp
    src/ui/OverflowPanel.cpp
    include/core/Types.h
    include/core/AppIdHelper.h
    include/core/Application.h
    include/core/DockManager.h
    include/core/ConfigManager.h
    include/core/ProcessMonitor.h
    include/core/SysHelper.h
    include/core/WindowCache.h
    include/core/ClickStateMachine.h
    include/core/IconProvider.h
    include/core/Logger.h
    include/core/PinnedItemsReader.h
    include/ui/DockWindow.h
    include/ui/DockItem.h
    include/ui/WindowPreviewPanel.h
    include/ui/OverflowPanel.h
    resources/resources.qrc
    resources/app.rc
)

target_include_directories(dock_wmac PRIVATE include)
target_link_libraries(dock_wmac Qt6::Widgets Qt6::Svg dwmapi shlwapi shell32)

# 输出文件名为 Mac任务栏.exe（内部 target 名保持 dock_wmac 不变）
set_target_properties(dock_wmac PROPERTIES
    RUNTIME_OUTPUT_NAME "Mac任务栏"
)

# windeployqt 自动部署 Qt 运行时依赖
find_program(WINDEPLOYQT_EXECUTABLE NAMES windeployqt HINTS "${Qt6_DIR}/../../../bin")
if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET dock_wmac POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --no-translations
            --no-opengl-sw
            "$<TARGET_FILE:dock_wmac>"
        COMMENT "Deploying Qt runtime dependencies..."
    )
else()
    message(WARNING "windeployqt not found. Qt DLLs will not be deployed automatically.")
endif()

# 独立卸载器（纯 Win32，无 Qt 依赖）
add_executable(uninstall WIN32
    src/uninstall/uninstall.cpp
)
target_link_libraries(uninstall user32 shell32 shlwapi advapi32)
set_target_properties(uninstall PROPERTIES
    RUNTIME_OUTPUT_NAME "uninstall"
)

# 安装规则
include(GNUInstallDirs)
install(TARGETS dock_wmac RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

# 可选测试
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
