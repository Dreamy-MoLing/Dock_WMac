/**
 * @file test_click_state_machine.cpp
 * @brief ClickStateMachine 单元测试
 *
 * 测试状态判定逻辑（不依赖真实窗口，通过 mock 行为验证）。
 */
#include <gtest/gtest.h>
#include <vector>
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
    // 验证 5 个状态枚举值全部互不相同
    using S = ClickStateMachine::State;
    std::vector<S> states = {
        S::NoWindows, S::BackgroundRunning, S::AllMinimized,
        S::ForegroundActive, S::BackgroundVisible
    };
    for (size_t i = 0; i < states.size(); ++i) {
        for (size_t j = i + 1; j < states.size(); ++j) {
            EXPECT_NE(static_cast<int>(states[i]), static_cast<int>(states[j]))
                << "状态枚举值冲突: state[" << i << "] == state[" << j << "]";
        }
    }
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
    // 验证 6 个动作枚举值全部互不相同
    using A = ClickStateMachine::Action;
    std::vector<A> actions = {
        A::None, A::LaunchApp, A::ShowHiddenWindow,
        A::RestoreLastActive, A::MinimizeAll, A::BringToForeground
    };
    for (size_t i = 0; i < actions.size(); ++i) {
        for (size_t j = i + 1; j < actions.size(); ++j) {
            EXPECT_NE(static_cast<int>(actions[i]), static_cast<int>(actions[j]))
                << "动作枚举值冲突: action[" << i << "] == action[" << j << "]";
        }
    }
}
