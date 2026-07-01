/**
 * @file test_display_affinity.cpp
 * @brief SysHelper::getWindowDisplayAffinity 单元测试
 *
 * 测试 GetWindowDisplayAffinity 只读采集在各种句柄条件下的行为。
 */

#include <gtest/gtest.h>
#include "core/SysHelper.h"

// ============================================================================
// 1. nullptr 返回 false，affinity 设为 0
// ============================================================================

TEST(DisplayAffinityTest, NullHwndReturnsFalseAndZeroAffinity)
{
    DWORD affinity = 0xFF;
    bool result = SysHelper::getWindowDisplayAffinity(nullptr, affinity);
    EXPECT_FALSE(result);
    EXPECT_EQ(affinity, 0u);
}

// ============================================================================
// 2. 无效句柄不崩溃
// ============================================================================

TEST(DisplayAffinityTest, InvalidHwndNoCrash)
{
    DWORD affinity = 0;
    EXPECT_NO_FATAL_FAILURE({
        bool result = SysHelper::getWindowDisplayAffinity(
            reinterpret_cast<HWND>(0xDEAD), affinity);
        (void)result;
    });
}

// ============================================================================
// 3. 无效句柄回退到 WDA_NONE
// ============================================================================

TEST(DisplayAffinityTest, InvalidHwndFallsBackToWdaNone)
{
    DWORD affinity = 0xFF;
    bool result = SysHelper::getWindowDisplayAffinity(
        reinterpret_cast<HWND>(0xDEAD), affinity);
    // 无效句柄：GetWindowDisplayAffinity 失败，affinity 应被设为 0 (WDA_NONE)
    EXPECT_EQ(affinity, 0u);
}

// ============================================================================
// 4. 连续调用不崩溃
// ============================================================================

TEST(DisplayAffinityTest, RepeatedCallsNoCrash)
{
    EXPECT_NO_FATAL_FAILURE({
        for (int i = 0; i < 10; ++i) {
            DWORD aff = 0;
            SysHelper::getWindowDisplayAffinity(nullptr, aff);
            SysHelper::getWindowDisplayAffinity(reinterpret_cast<HWND>(0xDEAD), aff);
        }
    });
}
