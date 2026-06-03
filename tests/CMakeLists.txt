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

find_package(Qt6 REQUIRED COMPONENTS Test)

# 核心源文件（测试需要链接核心实现，头文件需加入源列表以触发 AUTOMOC）
set(CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/ConfigManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DockManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/ProcessMonitor.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SysHelper.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
    ${CMAKE_SOURCE_DIR}/include/core/Types.h
    ${CMAKE_SOURCE_DIR}/include/core/ConfigManager.h
    ${CMAKE_SOURCE_DIR}/include/core/DockManager.h
    ${CMAKE_SOURCE_DIR}/include/core/ProcessMonitor.h
    ${CMAKE_SOURCE_DIR}/include/core/SysHelper.h
    ${CMAKE_SOURCE_DIR}/include/core/Logger.h
)

# 链接库
set(TEST_LIBS GTest::gtest_main Qt6::Widgets Qt6::Test dwmapi)

add_executable(test_config test_config.cpp ${CORE_SOURCES})
target_include_directories(test_config PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_config ${TEST_LIBS})

add_executable(test_dock_manager test_dock_manager.cpp ${CORE_SOURCES})
target_include_directories(test_dock_manager PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_dock_manager ${TEST_LIBS})

add_executable(test_process_monitor test_process_monitor.cpp ${CORE_SOURCES})
target_include_directories(test_process_monitor PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_process_monitor ${TEST_LIBS})

add_executable(test_sys_helper test_sys_helper.cpp ${CORE_SOURCES})
target_include_directories(test_sys_helper PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(test_sys_helper ${TEST_LIBS})

add_test(NAME test_config COMMAND test_config)
add_test(NAME test_dock_manager COMMAND test_dock_manager)
add_test(NAME test_process_monitor COMMAND test_process_monitor)
add_test(NAME test_sys_helper COMMAND test_sys_helper)
