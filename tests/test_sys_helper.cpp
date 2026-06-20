/**
 * @file test_sys_helper.cpp
 * @brief SysHelper Win32 API 包装层单元测试
 *
 * 测试窗口检测、全屏判断、最大化检测、钩子安装、任务栏管理、
 * DWM 模糊效果、主题检测、开机自启管理等功能。
 *
 * 设计原则：
 * - 所有测试必须在无图形界面的环境中安全运行（CI / headless）
 * - 测试的是"行为契约"（不崩溃、返回值合理），而非特定环境下的具体值
 * - 禁止使用 EXPECT_TRUE(x == true || x == false) 这种无意义断言
 * - 使用 EXPECT_NO_FATAL_FAILURE 验证函数调用不崩溃
 */

#include <gtest/gtest.h>
#include <chrono>
#include "core/SysHelper.h"

// ============================================================================
// Test Fixture
// ============================================================================

class SysHelperTest : public ::testing::Test {
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
// 1. Initialization
// ============================================================================

TEST_F(SysHelperTest, Initialization)
{
    // SysHelper 构造后不应为空指针
    EXPECT_NE(sysHelper, nullptr);
}

// ============================================================================
// 2. ForegroundWindowStateReturnsBool
// ============================================================================

TEST_F(SysHelperTest, ForegroundWindowStateReturnsBool)
{
    // getForegroundWindowState() 必须在任何环境下不崩溃地返回
    EXPECT_NO_FATAL_FAILURE({
        bool state = sysHelper->getForegroundWindowState();
        (void)state;
    });
}

// ============================================================================
// 3. BlurSupportedReturnsBool
// ============================================================================

TEST_F(SysHelperTest, BlurSupportedReturnsBool)
{
    // isBlurSupported() 应在任何环境下不崩溃地返回
    EXPECT_NO_FATAL_FAILURE({
        bool supported = sysHelper->isBlurSupported();
        (void)supported;
    });
}

// ============================================================================
// 4. IsLightThemeReturnsBool
// ============================================================================

TEST_F(SysHelperTest, IsLightThemeReturnsBool)
{
    // isLightTheme() 应在任何环境下不崩溃地返回
    EXPECT_NO_FATAL_FAILURE({
        bool isLight = sysHelper->isLightTheme();
        (void)isLight;
    });
}

// ============================================================================
// 5. AutoStartStateCheck
// ============================================================================

TEST_F(SysHelperTest, AutoStartStateCheck)
{
    // isAutoStartEnabled() 应在任何环境下不崩溃地返回
    EXPECT_NO_FATAL_FAILURE({
        bool enabled = sysHelper->isAutoStartEnabled();
        (void)enabled;
    });
}

// ============================================================================
// 6. AutoStartMutationDisabledInTests
// ============================================================================

TEST_F(SysHelperTest, AutoStartMutationDisabledInTests)
{
    EXPECT_FALSE(sysHelper->isAutoStartEnabled());
    EXPECT_FALSE(sysHelper->setAutoStart(true));
    EXPECT_FALSE(sysHelper->setAutoStart(false));
}

// ============================================================================
// 7. WindowHookInstall
// ============================================================================

TEST_F(SysHelperTest, WindowHookInstall)
{
    // installWindowHook() 返回 bool，不崩溃
    EXPECT_NO_FATAL_FAILURE({
        bool result = sysHelper->installWindowHook();
        (void)result;
    });
}

// ============================================================================
// 8. KeyboardHookInstallUninstall
// ============================================================================

TEST_F(SysHelperTest, KeyboardHookInstallUninstall)
{
    // installKeyboardHook() 返回 bool（headless 环境下可能失败，可以接受）
    EXPECT_NO_FATAL_FAILURE({
        bool result = sysHelper->installKeyboardHook();
        (void)result;
    });

    // uninstallKeyboardHook() 不崩溃
    EXPECT_NO_FATAL_FAILURE({
        sysHelper->uninstallKeyboardHook();
    });
}

// ============================================================================
// 9. TaskbarMutationDisabledInTests
// ============================================================================

TEST_F(SysHelperTest, TaskbarMutationDisabledInTests)
{
    // DOCK_WMAC_TESTING makes both calls no-ops, protecting developer and CI shells.
    EXPECT_NO_FATAL_FAILURE({
        sysHelper->hideNativeTaskbar();
    });

    EXPECT_NO_FATAL_FAILURE({
        sysHelper->restoreNativeTaskbar();
    });
}

// ============================================================================
// 10. DetectionPerformance
// ============================================================================

TEST_F(SysHelperTest, DetectionPerformance)
{
    // getForegroundWindowState() + isBlurSupported() + isLightTheme()
    // 三项检测总计应在 200ms 内完成
    auto start = std::chrono::high_resolution_clock::now();

    EXPECT_NO_FATAL_FAILURE({
        sysHelper->getForegroundWindowState();
        sysHelper->isBlurSupported();
        sysHelper->isLightTheme();
    });

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    EXPECT_LT(duration.count(), 200);
}

// ============================================================================
// 11. BlurBehindWindowNoCrash
// ============================================================================

TEST_F(SysHelperTest, BlurBehindWindowNoCrash)
{
    // enableBlurBehindWindow(0) 传入无效 WId，应优雅失败而非崩溃
    EXPECT_NO_FATAL_FAILURE({
        sysHelper->enableBlurBehindWindow(0);
    });
}
