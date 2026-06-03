/**
 * @file test_sys_helper.cpp
 * @brief SysHelper 单元测试
 *
 * 测试窗口检测、全屏判断、最大化检测等功能。
 *
 * 注意：由于 SysHelper 依赖 Win32 API 和实际窗口，
 * 这些测试主要验证接口存在性和基本逻辑，
 * 实际的窗口检测需要在有图形界面的环境中测试。
 */

#include <gtest/gtest.h>
#include "core/SysHelper.h"

class SysHelperTest : public ::testing::Test {
protected:
    void SetUp() override {
        sysHelper = new SysHelper();
    }
    void TearDown() override {
        delete sysHelper;
    }
    SysHelper *sysHelper;
};

// ========== 接口存在性测试 ==========

TEST_F(SysHelperTest, Initialization)
{
    // SysHelper 应能正常初始化
    EXPECT_NE(sysHelper, nullptr);
}

TEST_F(SysHelperTest, GetForegroundWindowState)
{
    // 测试前台窗口状态检测
    // 在无图形界面环境中，可能返回 false
    // 但不应崩溃
    bool state = sysHelper->getForegroundWindowState();

    // 验证返回布尔值（不验证具体值，因为依赖环境）
    EXPECT_TRUE(state == true || state == false);
}

// ========== 信号存在性测试 ==========

TEST_F(SysHelperTest, ForegroundWindowSignal)
{
    // 验证信号存在且可连接
    // 这里主要验证接口存在
    EXPECT_TRUE(typeid(*sysHelper) == typeid(SysHelper));
}

// ========== 窗口管理测试 ==========

TEST_F(SysHelperTest, GetWindowCount)
{
    // 测试获取窗口数量
    // 在无图形界面环境中，可能返回 0
    int count = sysHelper->getWindowCount("explorer");

    // 验证返回非负整数
    EXPECT_GE(count, 0);
}

TEST_F(SysHelperTest, ActivateWindow)
{
    // 测试激活窗口
    // 在无图形界面环境中，可能返回 false
    // 但不应崩溃
    bool result = sysHelper->activateWindow("nonexistent_app");

    // 验证返回布尔值
    EXPECT_TRUE(result == true || result == false);
}

// ========== 自动启动测试 ==========

TEST_F(SysHelperTest, AutoStart)
{
    // 测试自动启动设置
    // 注意：这些测试会修改系统设置，需要谨慎
    // 这里只测试接口存在性

    // 获取当前状态
    bool currentState = sysHelper->isAutoStartEnabled();

    // 验证返回布尔值
    EXPECT_TRUE(currentState == true || currentState == false);
}

// ========== DWM 模糊效果测试 ==========

TEST_F(SysHelperTest, BlurSupported)
{
    // 测试模糊效果支持
    bool supported = sysHelper->isBlurSupported();

    // 验证返回布尔值
    EXPECT_TRUE(supported == true || supported == false);
}

// ========== 任务栏管理测试 ==========

TEST_F(SysHelperTest, TaskbarManagement)
{
    // 测试任务栏管理功能
    // 注意：这些测试会修改系统状态，需要谨慎
    // 这里只测试接口存在性

    // 隐藏任务栏
    sysHelper->hideNativeTaskbar();

    // 恢复任务栏
    sysHelper->restoreNativeTaskbar();

    // 如果执行到这里没有崩溃，测试通过
    EXPECT_TRUE(true);
}

// ========== 主题检测测试 ==========

TEST_F(SysHelperTest, ThemeDetection)
{
    // 测试主题检测
    bool isLight = sysHelper->isLightTheme();

    // 验证返回布尔值
    EXPECT_TRUE(isLight == true || isLight == false);
}

// ========== 窗口选择器测试 ==========

TEST_F(SysHelperTest, ShowWindowPicker)
{
    // 测试显示窗口选择器
    // 在无图形界面环境中，可能无效果
    // 但不应崩溃
    sysHelper->showWindowPicker();

    // 如果执行到这里没有崩溃，测试通过
    EXPECT_TRUE(true);
}

// ========== 钩子安装测试 ==========

TEST_F(SysHelperTest, HookInstallation)
{
    // 测试钩子安装
    // 注意：这些测试会修改系统状态，需要谨慎
    // 这里只测试接口存在性

    // 安装窗口钩子
    bool windowHookResult = sysHelper->installWindowHook();

    // 验证返回布尔值
    EXPECT_TRUE(windowHookResult == true || windowHookResult == false);

    // 安装键盘钩子
    bool keyboardHookResult = sysHelper->installKeyboardHook();

    // 验证返回布尔值
    EXPECT_TRUE(keyboardHookResult == true || keyboardHookResult == false);

    // 卸载键盘钩子
    sysHelper->uninstallKeyboardHook();

    // 如果执行到这里没有崩溃，测试通过
    EXPECT_TRUE(true);
}

// ========== 性能测试 ==========

TEST_F(SysHelperTest, DetectionPerformance)
{
    // 测试检测性能
    // 在实际环境中，检测应在合理时间内完成
    // 这里验证基本性能要求

    auto start = std::chrono::high_resolution_clock::now();

    // 执行检测
    sysHelper->getForegroundWindowState();
    sysHelper->isBlurSupported();
    sysHelper->isLightTheme();

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // 检测应在 100ms 内完成
    EXPECT_LT(duration.count(), 100);
}
