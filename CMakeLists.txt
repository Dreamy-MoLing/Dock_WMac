cmake_minimum_required(VERSION 3.20)
project(Dock_WMac VERSION 0.2.4 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Svg)
qt_standard_project_setup()

# 核心源文件（与 tests/ 共享，新增文件只需在这里加一次）
set(DOCK_CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/ConfigManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DockManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ProcessMonitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SysHelper.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SysHelper_winhooks.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SysHelper_dwm.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SysHelper_dwm_state.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WindowCache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ClickStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/IconProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PinnedItemsReader.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/WindowPreviewPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/OverflowPanel.cpp
)

set(DOCK_CORE_HEADERS
    ${CMAKE_SOURCE_DIR}/include/core/Types.h
    ${CMAKE_SOURCE_DIR}/include/core/AppIdHelper.h
    ${CMAKE_SOURCE_DIR}/include/core/PathManager.h
    ${CMAKE_SOURCE_DIR}/include/core/ConfigManager.h
    ${CMAKE_SOURCE_DIR}/include/core/DockManager.h
    ${CMAKE_SOURCE_DIR}/include/core/ProcessMonitor.h
    ${CMAKE_SOURCE_DIR}/include/core/SysHelper.h
    ${CMAKE_SOURCE_DIR}/include/core/WindowCache.h
    ${CMAKE_SOURCE_DIR}/include/core/ClickStateMachine.h
    ${CMAKE_SOURCE_DIR}/include/core/IconProvider.h
    ${CMAKE_SOURCE_DIR}/include/core/Logger.h
    ${CMAKE_SOURCE_DIR}/include/core/PinnedItemsReader.h
    ${CMAKE_SOURCE_DIR}/include/ui/WindowPreviewPanel.h
    ${CMAKE_SOURCE_DIR}/include/ui/OverflowPanel.h
)

add_executable(dock_wmac WIN32
    src/main.cpp
    src/core/Application.cpp
    src/ui/DockWindow.cpp
    src/ui/DockWindow_position.cpp
    src/ui/DockWindow_input.cpp
    src/ui/DockWindow_menu.cpp
    src/ui/DockWindow_theme.cpp
    src/ui/DockWindow_transition.cpp
    src/ui/DockWindow_itemmanager.cpp
    src/ui/DockItem.cpp
    src/ui/DockAnimation.cpp
    include/core/Application.h
    include/ui/DockWindow.h
    include/ui/DockItem.h
    include/ui/DockAnimation.h
    ${DOCK_CORE_SOURCES}
    ${DOCK_CORE_HEADERS}
    resources/resources.qrc
    resources/app.rc
)

target_include_directories(dock_wmac PRIVATE include)
target_link_libraries(dock_wmac Qt6::Widgets Qt6::Svg dwmapi shlwapi shell32)

# 输出文件名为 WMacDock.exe（内部 target 名保持 dock_wmac 不变）
set_target_properties(dock_wmac PROPERTIES
    RUNTIME_OUTPUT_NAME "WMacDock"
)

# ---- 部署 — cmake --install 时自动调用 windeployqt ----
find_program(WINDEPLOYQT_EXECUTABLE windeployqt
    HINTS "${Qt6_DIR}/../../../bin"
)
if(NOT WINDEPLOYQT_EXECUTABLE)
    message(WARNING "windeployqt not found — cmake --install will skip Qt DLL deployment")
endif()

# exe 安装到根目录（扁平结构）
install(TARGETS dock_wmac RUNTIME DESTINATION .)

# windeployqt 直接部署到安装目录（exe 已在上一步安装到位）
if(WINDEPLOYQT_EXECUTABLE)
    install(CODE "
        execute_process(
            COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
                --no-translations
                --compiler-runtime
                \"\${CMAKE_INSTALL_PREFIX}/WMacDock.exe\"
            RESULT_VARIABLE deploy_result
        )
        if(NOT deploy_result EQUAL 0)
            message(FATAL_ERROR \"windeployqt failed with code \${deploy_result}\")
        endif()
    ")
endif()

# ---- 打包 (CPack) ----
set(CPACK_PACKAGE_NAME "Dock_WMac")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "Dreamy_Moling")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Windows macOS-style dock")
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}_${CPACK_PACKAGE_VERSION}_x64")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)

# ---- 测试 ----
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

