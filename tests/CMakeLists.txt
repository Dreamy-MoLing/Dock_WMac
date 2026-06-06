cmake_minimum_required(VERSION 3.20)

option(BUILD_TESTS "Build unit tests" OFF)
if(NOT BUILD_TESTS)
    return()
endif()

include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

# 核心源文件编译为 object library，避免每个测试重复编译
add_library(dock_core_objects OBJECT
    ${CMAKE_SOURCE_DIR}/src/core/ConfigManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DockManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ProcessMonitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SysHelper.cpp
    ${CMAKE_SOURCE_DIR}/src/core/WindowCache.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ClickStateMachine.cpp
    ${CMAKE_SOURCE_DIR}/src/core/IconProvider.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/core/PinnedItemsReader.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/WindowPreviewPanel.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/OverflowPanel.cpp
    ${CMAKE_SOURCE_DIR}/include/core/Types.h
    ${CMAKE_SOURCE_DIR}/include/core/ConfigManager.h
    ${CMAKE_SOURCE_DIR}/include/core/AppIdHelper.h
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
target_include_directories(dock_core_objects PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(dock_core_objects PUBLIC Qt6::Widgets dwmapi)

# 链接库
set(TEST_LIBS GTest::gtest_main Qt6::Widgets Qt6::Test)

add_executable(test_config test_config.cpp)
target_link_libraries(test_config dock_core_objects ${TEST_LIBS})

add_executable(test_dock_manager test_dock_manager.cpp)
target_link_libraries(test_dock_manager dock_core_objects ${TEST_LIBS})

add_executable(test_process_monitor test_process_monitor.cpp)
target_link_libraries(test_process_monitor dock_core_objects ${TEST_LIBS})

add_executable(test_sys_helper test_sys_helper.cpp)
target_link_libraries(test_sys_helper dock_core_objects ${TEST_LIBS})

add_executable(test_pinned_items_reader test_pinned_items_reader.cpp)
target_link_libraries(test_pinned_items_reader dock_core_objects ${TEST_LIBS})

add_executable(test_application test_application.cpp)
target_link_libraries(test_application dock_core_objects ${TEST_LIBS})

add_test(NAME test_config COMMAND test_config)
add_test(NAME test_dock_manager COMMAND test_dock_manager)
add_test(NAME test_process_monitor COMMAND test_process_monitor)
add_test(NAME test_sys_helper COMMAND test_sys_helper)
add_test(NAME test_pinned_items_reader COMMAND test_pinned_items_reader)
add_test(NAME test_application COMMAND test_application)

add_executable(test_window_cache test_window_cache.cpp)
target_link_libraries(test_window_cache dock_core_objects ${TEST_LIBS})
add_test(NAME test_window_cache COMMAND test_window_cache)

add_executable(test_click_state_machine test_click_state_machine.cpp)
target_link_libraries(test_click_state_machine dock_core_objects ${TEST_LIBS})
add_test(NAME test_click_state_machine COMMAND test_click_state_machine)
