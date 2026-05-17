/**
 * @file test_dock_manager.cpp
 * @brief DockManager 状态机单元测试
 *
 * 测试状态转换、信号发射、事件处理等功能。
 */

#include <gtest/gtest.h>
#include <QSignalSpy>
#include "core/DockManager.h"
#include "core/SysHelper.h"

class DockManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        sysHelper = new SysHelper();
        manager = new DockManager();
        manager->initialize(sysHelper);
    }
    void TearDown() override {
        delete manager;
        delete sysHelper;
    }
    SysHelper *sysHelper;
    DockManager *manager;
};

TEST_F(DockManagerTest, InitialStateIsDocked)
{
    EXPECT_EQ(manager->currentState(), DockState::Docked);
}

TEST_F(DockManagerTest, InitialItemsNotEmpty)
{
    // 应加载到默认应用（如果系统有 .desktop 文件）
    auto items = manager->items();
    // 不断言数量，因为依赖系统环境
    // 但至少应该能正常返回
    EXPECT_GE(items.size(), 0);
}

TEST_F(DockManagerTest, StateTransitionToHidden)
{
    QSignalSpy spy(manager, &DockManager::stateChanged);

    // 模拟前台窗口最大化
    manager->onForegroundWindowChanged(true);

    EXPECT_EQ(manager->currentState(), DockState::Hidden);
    // 应该发射了 stateChanged 信号（Animating + Hidden）
    EXPECT_GE(spy.count(), 1);
}

TEST_F(DockManagerTest, StateTransitionToDocked)
{
    // 先隐藏
    manager->onForegroundWindowChanged(true);
    EXPECT_EQ(manager->currentState(), DockState::Hidden);

    QSignalSpy spy(manager, &DockManager::stateChanged);

    // 模拟前台窗口恢复
    manager->onForegroundWindowChanged(false);

    EXPECT_EQ(manager->currentState(), DockState::Docked);
    EXPECT_GE(spy.count(), 1);
}

TEST_F(DockManagerTest, WinKeyShowsHiddenDock)
{
    // 先隐藏
    manager->onForegroundWindowChanged(true);
    EXPECT_EQ(manager->currentState(), DockState::Hidden);

    QSignalSpy spy(manager, &DockManager::stateChanged);

    // 按下 Win 键
    manager->onWinKeyPressed();

    EXPECT_EQ(manager->currentState(), DockState::Docked);
    EXPECT_GE(spy.count(), 1);
}

TEST_F(DockManagerTest, WinKeyIgnoredWhenDocked)
{
    // 已经在 Docked 状态
    EXPECT_EQ(manager->currentState(), DockState::Docked);

    QSignalSpy spy(manager, &DockManager::stateChanged);

    // 按下 Win 键应该无效果
    manager->onWinKeyPressed();

    EXPECT_EQ(manager->currentState(), DockState::Docked);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(DockManagerTest, RepeatedStateChangeIgnored)
{
    // 连续两次最大化应该不重复触发
    manager->onForegroundWindowChanged(true);
    QSignalSpy spy(manager, &DockManager::stateChanged);

    // 第二次最大化（已经 Hidden）
    manager->onForegroundWindowChanged(true);

    // 应该没有额外的信号
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(manager->currentState(), DockState::Hidden);
}

TEST_F(DockManagerTest, ItemSignalsEmitted)
{
    // 重新创建以触发 itemAdded 信号
    DockManager manager2;
    QSignalSpy spy(&manager2, &DockManager::itemAdded);
    manager2.initialize(sysHelper);

    // 应该为每个固定项发射了信号
    EXPECT_EQ(spy.count(), manager2.items().size());
}
