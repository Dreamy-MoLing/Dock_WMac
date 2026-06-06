/**
 * @file test_click_state_machine.cpp
 * @brief ClickStateMachine 单元测试
 *
 * 测试状态判定逻辑（不依赖真实窗口，通过 mock 行为验证）。
 */
#include <gtest/gtest.h>
#include "core/ClickStateMachine.h"
#include "core/WindowCache.h"

class ClickStateMachineTest : public ::testing::Test {
protected:
    void SetUp() override {
        cache = new WindowCache();
        machine = new ClickStateMachine(cache);
    }
    void TearDown() override {
        delete machine;
        delete cache;
    }
    WindowCache *cache = nullptr;
    ClickStateMachine *machine = nullptr;
};

TEST_F(ClickStateMachineTest, NotRunningReturnsLaunchApp)
{
    // S0: 进程不在运行 → LaunchApp
    auto action = machine->evaluate("testapp", "C:/test.exe", false);
    EXPECT_EQ(action, ClickStateMachine::Action::LaunchApp);
}

TEST_F(ClickStateMachineTest, RunningNoWindowsReturnsShowHidden)
{
    // S0.5: 进程运行但无任何窗口 → ShowHiddenWindow
    // cache 初始为空，hasVisible/hasMinimized/hasHidden 都返回 false
    // 但 isRunning=true 时，determineState 走到兜底 BackgroundRunning
    auto action = machine->evaluate("testapp", "C:/test.exe", true);
    EXPECT_EQ(action, ClickStateMachine::Action::ShowHiddenWindow);
}

TEST_F(ClickStateMachineTest, HandleClickDoesNotCrash)
{
    // 点击处理不崩溃
    EXPECT_NO_FATAL_FAILURE({
        machine->handleClick("testapp", "C:/test.exe", false);
    });
    EXPECT_NO_FATAL_FAILURE({
        machine->handleClick("testapp", "C:/test.exe", true);
    });
}

TEST_F(ClickStateMachineTest, HandleDoubleClickDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE({
        machine->handleDoubleClick("C:/test.exe");
    });
}

TEST_F(ClickStateMachineTest, StateEnumValues)
{
    // 验证状态枚举值存在且互不相同
    auto s0 = ClickStateMachine::State::NoWindows;
    auto s0p5 = ClickStateMachine::State::BackgroundRunning;
    auto s1 = ClickStateMachine::State::AllMinimized;
    auto s2 = ClickStateMachine::State::ForegroundActive;
    auto s3 = ClickStateMachine::State::BackgroundVisible;
    EXPECT_NE(static_cast<int>(s0), static_cast<int>(s1));
    EXPECT_NE(static_cast<int>(s0p5), static_cast<int>(s2));
}

TEST_F(ClickStateMachineTest, EmptyExecPathLaunchAppDoesNotCrash)
{
    // execPath 为空时 LaunchApp 不应崩溃
    auto action = machine->evaluate("testapp", "", false);
    EXPECT_EQ(action, ClickStateMachine::Action::LaunchApp);
    EXPECT_NO_FATAL_FAILURE({
        machine->execute(action, "testapp", "");
    });
}

TEST_F(ClickStateMachineTest, ActionEnumValues)
{
    // 验证动作枚举值存在
    auto a0 = ClickStateMachine::Action::None;
    auto a1 = ClickStateMachine::Action::LaunchApp;
    auto a2 = ClickStateMachine::Action::ShowHiddenWindow;
    auto a3 = ClickStateMachine::Action::RestoreLastActive;
    auto a4 = ClickStateMachine::Action::MinimizeAll;
    auto a5 = ClickStateMachine::Action::BringToForeground;
    EXPECT_NE(static_cast<int>(a0), static_cast<int>(a1));
    EXPECT_NE(static_cast<int>(a2), static_cast<int>(a3));
    EXPECT_NE(static_cast<int>(a4), static_cast<int>(a5));
}
