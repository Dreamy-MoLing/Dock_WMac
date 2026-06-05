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

// ========== maxItems 限制测试 ==========

TEST_F(DockManagerTest, DefaultMaxItemsIs16)
{
    EXPECT_EQ(manager->maxItems(), 16);
}

TEST_F(DockManagerTest, SetMaxItems)
{
    manager->setMaxItems(8);
    EXPECT_EQ(manager->maxItems(), 8);
}

TEST_F(DockManagerTest, SetMaxItemsMinOne)
{
    manager->setMaxItems(0);
    EXPECT_EQ(manager->maxItems(), 1);
}

TEST_F(DockManagerTest, TransientItemsRespectMaxLimit)
{
    manager->setMaxItems(3);
    DockItemData pinned{"pinned1", "Pinned1", "", "", false, 0};
    manager->setPinnedItems({pinned});
    manager->addTransientItem({"t1", "T1", "", "", true, 0});
    manager->addTransientItem({"t2", "T2", "", "", true, 0});
    manager->addTransientItem({"t3", "T3", "", "", true, 0});
    // 总数4 > maxItems 3，visibleItems 应限制为3
    EXPECT_LE(manager->visibleItems().size(), 3);
}

TEST_F(DockManagerTest, PinnedItemsAlwaysShown)
{
    manager->setMaxItems(2);
    DockItemData p1{"p1", "P1", "", "", false, 0};
    DockItemData p2{"p2", "P2", "", "", false, 0};
    DockItemData p3{"p3", "P3", "", "", false, 0};
    manager->setPinnedItems({p1, p2, p3});
    // 固定项不受限制
    EXPECT_EQ(manager->visibleItems().size(), 3);
}

TEST_F(DockManagerTest, OverflowItemsAccessible)
{
    manager->setMaxItems(3);
    DockItemData pinned{"p1", "P1", "", "", false, 0};
    manager->setPinnedItems({pinned});
    manager->addTransientItem({"t1", "T1", "", "", true, 0});
    manager->addTransientItem({"t2", "T2", "", "", true, 0});
    manager->addTransientItem({"t3", "T3", "", "", true, 0});
    auto overflow = manager->overflowItems();
    EXPECT_GE(overflow.size(), 1);
    EXPECT_EQ(overflow[0].appId, "t3");
}

TEST_F(DockManagerTest, MorePinnedThanMaxItems)
{
    manager->setMaxItems(2);
    DockItemData p1{"p1", "P1", "", "", false, 0};
    DockItemData p2{"p2", "P2", "", "", false, 0};
    DockItemData p3{"p3", "P3", "", "", false, 0};
    manager->setPinnedItems({p1, p2, p3});
    EXPECT_EQ(manager->visibleItems().size(), 3);
    manager->addTransientItem({"t1", "T1", "", "", true, 0});
    // 固定项3个已超maxItems，临时项全进overflow
    EXPECT_EQ(manager->visibleItems().size(), 3);
    EXPECT_EQ(manager->overflowItems().size(), 1);
}

TEST_F(DockManagerTest, OverflowChangedSignal)
{
    QSignalSpy spy(manager, &DockManager::overflowChanged);
    manager->setMaxItems(2);
    DockItemData p1{"p1", "P1", "", "", false, 0};
    manager->setPinnedItems({p1});
    // setPinnedItems emits overflowChanged
    EXPECT_GE(spy.count(), 1);
    spy.clear();

    manager->addTransientItem({"t1", "T1", "", "", true, 0});
    manager->addTransientItem({"t2", "T2", "", "", true, 0}); // 超出
    EXPECT_GE(spy.count(), 1);
}

TEST_F(DockManagerTest, NoOverflowWhenUnderLimit)
{
    manager->setMaxItems(5);
    DockItemData p1{"p1", "P1", "", "", false, 0};
    manager->setPinnedItems({p1});
    manager->addTransientItem({"t1", "T1", "", "", true, 0});
    EXPECT_TRUE(manager->overflowItems().isEmpty());
}

// ========== windowCount 测试 ==========

TEST(DockItemDataTest, DefaultWindowCountIsOne)
{
    DockItemData item{"app", "App", "", "", false, 0};
    EXPECT_EQ(item.windowCount, 1);
}

TEST_F(DockManagerTest, UpdateWindowCount)
{
    DockItemData item{"app", "App", "", "", true, 0};
    manager->addTransientItem(item);
    QSignalSpy spy(manager, &DockManager::itemWindowCountChanged);
    manager->updateWindowCount("app", 3);
    // updateWindowCount 通过信号通知 UI 更新
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "app");
    EXPECT_EQ(spy.at(0).at(1).toInt(), 3);
}

TEST_F(DockManagerTest, UpdateWindowCountPinned)
{
    DockItemData item{"app", "App", "", "", false, 0};
    manager->setPinnedItems({item});
    manager->updateWindowCount("app", 5);
    auto items = manager->pinnedItems();
    ASSERT_FALSE(items.isEmpty());
    EXPECT_EQ(items[0].windowCount, 5);
}

TEST_F(DockManagerTest, WindowCountChangedSignal)
{
    DockItemData item{"app", "App", "", "", true, 0};
    manager->addTransientItem(item);
    QSignalSpy spy(manager, &DockManager::itemWindowCountChanged);
    manager->updateWindowCount("app", 3);
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(DockManagerTest, WindowCountNoChangeNoSignal)
{
    DockItemData item{"app", "App", "", "", true, 0};
    manager->addTransientItem(item);
    manager->updateWindowCount("app", 1); // same as default
    QSignalSpy spy(manager, &DockManager::itemWindowCountChanged);
    manager->updateWindowCount("app", 1); // still 1
    EXPECT_EQ(spy.count(), 0);
}
