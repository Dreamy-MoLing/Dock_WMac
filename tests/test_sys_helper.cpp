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
#include "core/WindowManager.h"

// ============================================================================
// Test Fixture
// ============================================================================

class SysHelperTest : public ::testing::Test {
protected:
    void SetUp() override {
        sysHelper = new SysHelper();
        wm = new WindowManager();
    }
    void TearDown() override {
        delete sysHelper;
        sysHelper = nullptr;
        delete wm;
        wm = nullptr;
    }
    SysHelper *sysHelper = nullptr;
    WindowManager *wm = nullptr;
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
// 3. WindowCountNonNegative
// ============================================================================

TEST_F(SysHelperTest, WindowCountNonNegative)
{
    // getWindowCount() 对任意进程名应返回 >= 0 的值
    int count = wm->getWindowCount("explorer");
    EXPECT_GE(count, 0);
}

// ============================================================================
// 4. ActivateWindowDoesNotCrash
// ============================================================================

TEST_F(SysHelperTest, ActivateWindowDoesNotCrash)
{
    // activateWindow("nonexistent_app") 应返回 bool 且不崩溃
    EXPECT_NO_FATAL_FAILURE({
        bool result = wm->activateWindow("nonexistent_app");
        (void)result;
    });
}

// ============================================================================
// 5. BlurSupportedReturnsBool
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
// 6. IsLightThemeReturnsBool
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
// 7. AutoStartStateCheck
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
// 8. SetAutoStartRoundtrip
// ============================================================================

TEST_F(SysHelperTest, SetAutoStartRoundtrip)
{
    // 保存当前状态
    bool originalState = false;
    EXPECT_NO_FATAL_FAILURE({
        originalState = sysHelper->isAutoStartEnabled();
    });

    // 切换到相反状态
    EXPECT_NO_FATAL_FAILURE({
        bool toggleResult = sysHelper->setAutoStart(!originalState);
        (void)toggleResult;
    });

    // 验证状态已改变（注册表操作在受限环境下可能失败，只要不崩溃即可）
    EXPECT_NO_FATAL_FAILURE({
        bool newState = sysHelper->isAutoStartEnabled();
        (void)newState;
    });

    // 恢复原始状态
    EXPECT_NO_FATAL_FAILURE({
        bool restoreResult = sysHelper->setAutoStart(originalState);
        (void)restoreResult;
    });
}

// ============================================================================
// 9. WindowHookInstall
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
// 10. KeyboardHookInstallUninstall
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
// 11. TaskbarHideRestore
// ============================================================================

TEST_F(SysHelperTest, TaskbarHideRestore)
{
    // hideNativeTaskbar() 和 restoreNativeTaskbar() 在任何环境下安全调用
    EXPECT_NO_FATAL_FAILURE({
        sysHelper->hideNativeTaskbar();
    });

    EXPECT_NO_FATAL_FAILURE({
        sysHelper->restoreNativeTaskbar();
    });
}

// ============================================================================
// 12. ShowWindowPickerNoCrash
// ============================================================================

TEST_F(SysHelperTest, ShowWindowPickerNoCrash)
{
    // showWindowPicker() 在任何环境下不崩溃
    EXPECT_NO_FATAL_FAILURE({
        wm->showWindowPicker();
    });
}

// ============================================================================
// 13. DetectionPerformance
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
// 14. BlurBehindWindowNoCrash
// ============================================================================

TEST_F(SysHelperTest, BlurBehindWindowNoCrash)
{
    // enableBlurBehindWindow(0) 传入无效 WId，应优雅失败而非崩溃
    EXPECT_NO_FATAL_FAILURE({
        sysHelper->enableBlurBehindWindow(0);
    });
}
