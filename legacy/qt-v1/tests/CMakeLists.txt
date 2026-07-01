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
# Prevent GTest from adding install() rules (we only need it for testing)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test Widgets)

# Qt DLL 路径 — 测试运行时需要 Qt DLL 在 PATH 中
get_target_property(QT_QMAKE Qt6::qmake IMPORTED_LOCATION)
cmake_path(GET QT_QMAKE PARENT_PATH QT_BIN_DIR)

# 核心源文件编译为 object library，避免每个测试重复编译
# 源文件列表由父 CMakeLists.txt 的 DOCK_CORE_SOURCES/DOCK_CORE_HEADERS 统一定义
add_library(dock_core_objects OBJECT
    ${DOCK_CORE_SOURCES}
    ${DOCK_CORE_HEADERS}
)
target_include_directories(dock_core_objects PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(dock_core_objects PUBLIC Qt6::Widgets dwmapi shell32 ole32 uuid propsys comctl32)
# Tests must never mutate HKCU startup state or hide the runner's taskbar.
target_compile_definitions(dock_core_objects PRIVATE DOCK_WMAC_TESTING)

add_library(music_core_objects OBJECT
    ${MUSIC_CORE_SOURCES}
    ${MUSIC_CORE_HEADERS}
)
target_include_directories(music_core_objects PUBLIC ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(music_core_objects PUBLIC Qt6::Core)

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

add_executable(test_dwm_state test_dwm_state.cpp)
target_link_libraries(test_dwm_state dock_core_objects ${TEST_LIBS})
add_test(NAME test_dwm_state COMMAND test_dwm_state)

add_executable(test_window_cloaked test_window_cloaked.cpp)
target_link_libraries(test_window_cloaked dock_core_objects ${TEST_LIBS})
add_test(NAME test_window_cloaked COMMAND test_window_cloaked)

add_executable(test_display_affinity test_display_affinity.cpp)
target_link_libraries(test_display_affinity dock_core_objects ${TEST_LIBS})
add_test(NAME test_display_affinity COMMAND test_display_affinity)

add_executable(test_windows_integration test_windows_integration.cpp)
target_link_libraries(test_windows_integration dock_core_objects GTest::gtest Qt6::Widgets Qt6::Test)
add_test(NAME test_windows_integration COMMAND test_windows_integration)
set_tests_properties(test_windows_integration PROPERTIES LABELS "integration;windows")

add_executable(test_lrc_parser music/test_lrc_parser.cpp)
target_link_libraries(test_lrc_parser music_core_objects ${TEST_LIBS})
add_test(NAME test_lrc_parser COMMAND test_lrc_parser)

add_executable(test_audio_level music/test_audio_level.cpp)
target_link_libraries(test_audio_level music_core_objects ${TEST_LIBS})
add_test(NAME test_audio_level COMMAND test_audio_level)

add_executable(test_now_playing_panel
    music/test_now_playing_panel.cpp
    ${CMAKE_SOURCE_DIR}/include/music/ui/NowPlayingPanel.h
    ${CMAKE_SOURCE_DIR}/src/music/ui/NowPlayingPanel.cpp
)
target_include_directories(test_now_playing_panel PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_now_playing_panel ${TEST_LIBS})
add_test(NAME test_now_playing_panel COMMAND test_now_playing_panel)

# 确保测试运行时能找到 Qt DLL
foreach(test_name
    test_config test_dock_manager test_process_monitor test_sys_helper
    test_pinned_items_reader test_application test_window_cache test_click_state_machine
    test_dwm_state test_window_cloaked test_display_affinity test_windows_integration
    test_lrc_parser test_audio_level test_now_playing_panel
)
    set_tests_properties(${test_name} PROPERTIES
        ENVIRONMENT_MODIFICATION
            "PATH=path_list_prepend:${QT_BIN_DIR};DOCK_WMAC_DATA_DIR=set:${CMAKE_CURRENT_BINARY_DIR}/test-data/${test_name}"
    )
endforeach()
