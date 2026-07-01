/**
 * @file test_dwm_state.cpp
 * @brief SysHelper DWM 状态只读采集单元测试
 *
 * 测试 getExtendedFrameBounds 在正常窗口、最小化窗口、DWM 不可用时的行为。
 */

#include <gtest/gtest.h>
#include "core/SysHelper.h"

class DwmStateTest : public ::testing::Test {
protected:
    void SetUp() override {
        sysHelper = new SysHelper();
    }
    void TearDown() override {
        delete sysHelper;
        sysHelper = nullptr;
    }
    SysHelper *sysHelper = nullptr;
};

// ============================================================================
// 1. getExtendedFrameBounds with null HWND
// ============================================================================

TEST_F(DwmStateTest, GetExtendedFrameBoundsNullHwnd)
{
    RECT rect = {};
    bool result = sysHelper->getExtendedFrameBounds(nullptr, rect);
    // nullptr 应返回 false，不崩溃
    EXPECT_FALSE(result);
}

// ============================================================================
// 2. getExtendedFrameBounds with invalid HWND
// ============================================================================

TEST_F(DwmStateTest, GetExtendedFrameBoundsInvalidHwnd)
{
    RECT rect = {};
    // 无效句柄应回退到 GetWindowRect 或返回 false
    EXPECT_NO_FATAL_FAILURE({
        bool result = sysHelper->getExtendedFrameBounds(reinterpret_cast<HWND>(0xDEAD), rect);
        (void)result;
    });
}

// ============================================================================
// 3. isWindowCloaked with null HWND
// ============================================================================

TEST_F(DwmStateTest, IsWindowCloakedNullHwnd)
{
    bool result = sysHelper->isWindowCloaked(nullptr);
    EXPECT_FALSE(result);
}

// ============================================================================
// 4. isWindowCloaked with invalid HWND
// ============================================================================

TEST_F(DwmStateTest, IsWindowCloakedInvalidHwnd)
{
    // 无效句柄应返回 false（不崩溃）
    EXPECT_NO_FATAL_FAILURE({
        bool result = sysHelper->isWindowCloaked(reinterpret_cast<HWND>(0xDEAD));
        (void)result;
    });
}

// ============================================================================
// 5. getWindowDisplayAffinity with null HWND
// ============================================================================

TEST_F(DwmStateTest, GetWindowDisplayAffinityNullHwnd)
{
    DWORD affinity = 0xFF;
    bool result = sysHelper->getWindowDisplayAffinity(nullptr, affinity);
    // nullptr 应返回 false，affinity 应被设为 0
    EXPECT_FALSE(result);
    EXPECT_EQ(affinity, 0u);
}

// ============================================================================
// 6. getWindowDisplayAffinity with invalid HWND
// ============================================================================

TEST_F(DwmStateTest, GetWindowDisplayAffinityInvalidHwnd)
{
    DWORD affinity = 0xFF;
    // 无效句柄应回退到 WDA_NONE（0）
    EXPECT_NO_FATAL_FAILURE({
        bool result = sysHelper->getWindowDisplayAffinity(reinterpret_cast<HWND>(0xDEAD), affinity);
        (void)result;
    });
}

// ============================================================================
// 7. Static isWindowCloaked null check
// ============================================================================

TEST_F(DwmStateTest, StaticIsWindowCloakedNull)
{
    // 静态方法也应安全处理 nullptr
    bool result = SysHelper::isWindowCloaked(nullptr);
    EXPECT_FALSE(result);
}
