cmake_minimum_required(VERSION 3.20)

option(BUILD_TESTS "Build unit tests" OFF)
if(NOT BUILD_TESTS)
    return()
endif()

find_package(GTest QUIET)
if(NOT GTest_FOUND)
    message(WARNING "Google Test not found — skipping tests")
    return()
endif()

enable_testing()

find_package(Qt6 REQUIRED COMPONENTS Test)

# 平台源文件
if(WIN32)
    set(PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/core/SysHelper_win.cpp)
else()
    set(PLATFORM_SRC ${CMAKE_SOURCE_DIR}/src/core/SysHelper_linux.cpp)
endif()

# 公共源文件（测试需要链接核心实现，头文件需加入源列表以触发 AUTOMOC）
set(CORE_SOURCES
    ${CMAKE_SOURCE_DIR}/src/core/ConfigManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/DockManager.cpp
    ${CMAKE_SOURCE_DIR}/src/core/IPCHelper.cpp
    ${CMAKE_SOURCE_DIR}/src/core/SysHelper.cpp
    ${PLATFORM_SRC}
    ${CMAKE_SOURCE_DIR}/include/core/Types.h
    ${CMAKE_SOURCE_DIR}/include/core/ConfigManager.h
    ${CMAKE_SOURCE_DIR}/include/core/DockManager.h
    ${CMAKE_SOURCE_DIR}/include/core/IPCHelper.h
    ${CMAKE_SOURCE_DIR}/include/core/SysHelper.h
)

# 链接库
set(TEST_LIBS GTest::GTest GTest::Main Qt6::Widgets Qt6::Test)
if(NOT WIN32)
    list(APPEND TEST_LIBS ${X11_LIBRARIES})
endif()

add_executable(test_config test_config.cpp ${CORE_SOURCES})
target_include_directories(test_config PRIVATE ${CMAKE_SOURCE_DIR}/include ${X11_INCLUDE_DIR})
target_link_libraries(test_config ${TEST_LIBS})

add_executable(test_dock_manager test_dock_manager.cpp ${CORE_SOURCES})
target_include_directories(test_dock_manager PRIVATE ${CMAKE_SOURCE_DIR}/include ${X11_INCLUDE_DIR})
target_link_libraries(test_dock_manager ${TEST_LIBS})

add_test(NAME test_config COMMAND test_config)
add_test(NAME test_dock_manager COMMAND test_dock_manager)
