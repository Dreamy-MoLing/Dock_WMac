/**
 * @file test_window_cloaked.cpp
 * @brief SysHelper::isWindowCloaked 单元测试
 *
 * 测试 DWMWA_CLOAKED 只读采集在各种句柄条件下的行为。
 */

#include <gtest/gtest.h>
#include "core/SysHelper.h"

// ============================================================================
// 1. nullptr 返回 false
// ============================================================================

TEST(WindowCloakedTest, NullHwndReturnsFalse)
{
    EXPECT_FALSE(SysHelper::isWindowCloaked(nullptr));
}

// ============================================================================
// 2. 无效句柄不崩溃
// ============================================================================

TEST(WindowCloakedTest, InvalidHwndNoCrash)
{
    EXPECT_NO_FATAL_FAILURE({
        bool result = SysHelper::isWindowCloaked(reinterpret_cast<HWND>(0xDEAD));
        (void)result;
    });
}

// ============================================================================
// 3. 无效句柄返回值合理
// ============================================================================

TEST(WindowCloakedTest, InvalidHwndReturnsFalse)
{
    // 无效句柄在 Windows 8+ 上 DwmGetWindowAttribute 失败，应返回 false
    bool result = SysHelper::isWindowCloaked(reinterpret_cast<HWND>(0xDEAD));
    EXPECT_FALSE(result);
}

// ============================================================================
// 4. 连续调用不崩溃
// ============================================================================

TEST(WindowCloakedTest, RepeatedCallsNoCrash)
{
    EXPECT_NO_FATAL_FAILURE({
        for (int i = 0; i < 10; ++i) {
            SysHelper::isWindowCloaked(nullptr);
            SysHelper::isWindowCloaked(reinterpret_cast<HWND>(0xDEAD));
        }
    });
}
