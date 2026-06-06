/**
 * @file test_dock_manager.cpp
 * @brief DockManager 状态机单元测试
 *
 * 测试状态转换、信号发射、事件处理、固定/临时项管理等功能。
 * DockState 枚举仅包含 Docked 和 Hidden（Animating 已在 Phase E2 移除）。
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

// 1. 初始状态应为 Docked
TEST_F(DockManagerTest, InitialStateDocked)
{
    EXPECT_EQ(manager->currentState(), DockState::Docked);
}

// 2. 前台窗口最大化时状态转为 Hidden，信号精确发射一次
TEST_F(DockManagerTest, StateTransitionDockedToHidden)
{
    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->onForegroundWindowChanged(true);
    EXPECT_EQ(manager->currentState(), DockState::Hidden);
    EXPECT_EQ(spy.count(), 1);
}

// 3. 隐藏后恢复：状态转为 Docked，信号精确发射一次
TEST_F(DockManagerTest, StateTransitionHiddenToDocked)
{
    manager->onForegroundWindowChanged(true);
    EXPECT_EQ(manager->currentState(), DockState::Hidden);

    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->onForegroundWindowChanged(false);
    EXPECT_EQ(manager->currentState(), DockState::Docked);
    EXPECT_EQ(spy.count(), 1);
}

// 4. 重复最大化被忽略：第二次调用不发射信号，状态保持 Hidden
TEST_F(DockManagerTest, RepeatedMaximizeIgnored)
{
    manager->onForegroundWindowChanged(true);
    EXPECT_EQ(manager->currentState(), DockState::Hidden);

    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->onForegroundWindowChanged(true);
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(manager->currentState(), DockState::Hidden);
}

// 5. Win 键恢复隐藏的 Dock：状态转为 Docked，信号精确发射一次
TEST_F(DockManagerTest, WinKeyShowsHiddenDock)
{
    manager->onForegroundWindowChanged(true);
    EXPECT_EQ(manager->currentState(), DockState::Hidden);

    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->onWinKeyPressed();
    EXPECT_EQ(manager->currentState(), DockState::Docked);
    EXPECT_EQ(spy.count(), 1);
}

// 6. Win 键在 Docked 状态下被忽略：无信号，状态保持 Docked
TEST_F(DockManagerTest, WinKeyIgnoredWhenDocked)
{
    EXPECT_EQ(manager->currentState(), DockState::Docked);

    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->onWinKeyPressed();
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(manager->currentState(), DockState::Docked);
}

// 7. 固定项：pinItem 后 isPinned 返回 true，pinnedItems 包含该项
TEST_F(DockManagerTest, PinItem)
{
    DockItemData item{"app", "Name", "/path/app.exe", "", false, 0};
    manager->pinItem(item);
    EXPECT_TRUE(manager->isPinned("app"));
    auto pinned = manager->pinnedItems();
    ASSERT_EQ(pinned.size(), 1);
    EXPECT_EQ(pinned[0].appId, "app");
}

// 8. 重复固定同一项被忽略：pinnedItems 仍只有 1 个
TEST_F(DockManagerTest, PinDuplicateIgnored)
{
    DockItemData item{"app", "Name", "/path/app.exe", "", false, 0};
    manager->pinItem(item);
    manager->pinItem(item);
    EXPECT_EQ(manager->pinnedItems().size(), 1);
}

// 9. 取消固定：pin 后 unpin，isPinned 返回 false，pinnedItemsChanged 信号发射
TEST_F(DockManagerTest, UnpinItem)
{
    DockItemData item{"app", "Name", "/path/app.exe", "", false, 0};
    manager->pinItem(item);
    EXPECT_TRUE(manager->isPinned("app"));

    QSignalSpy spy(manager, &DockManager::pinnedItemsChanged);
    manager->unpinItem("app");
    EXPECT_FALSE(manager->isPinned("app"));
    EXPECT_EQ(spy.count(), 1);
}

// 10. 取消不存在的固定项不崩溃，无信号
TEST_F(DockManagerTest, UnpinNonexistentNoCrash)
{
    QSignalSpy spy(manager, &DockManager::pinnedItemsChanged);
    manager->unpinItem("nonexistent");
    EXPECT_EQ(spy.count(), 0);
    // 不崩溃即为通过
}

// 11. 固定临时项后从 transients 移除：transientItems 空，pinnedItems 有 1 个
TEST_F(DockManagerTest, PinRemovesFromTransient)
{
    DockItemData item{"app", "Name", "/path/app.exe", "", true, 0};
    manager->addTransientItem(item);
    ASSERT_EQ(manager->transientItems().size(), 1);

    manager->pinItem(item);
    EXPECT_TRUE(manager->transientItems().isEmpty());
    EXPECT_EQ(manager->pinnedItems().size(), 1);
}

// 12. 默认最大项数为 16
TEST_F(DockManagerTest, MaxItemsDefault)
{
    EXPECT_EQ(manager->maxItems(), 16);
}

// 13. setMaxItems(0) 下限为 1
TEST_F(DockManagerTest, SetMaxItemsMinOne)
{
    manager->setMaxItems(0);
    EXPECT_EQ(manager->maxItems(), 1);
}

// 14. setMaxItems 设相同值不触发 overflowChanged
TEST_F(DockManagerTest, SetMaxItemsNoChange)
{
    EXPECT_EQ(manager->maxItems(), 16);
    QSignalSpy spy(manager, &DockManager::overflowChanged);
    manager->setMaxItems(16);
    EXPECT_EQ(spy.count(), 0);
}

// 15. 临时项溢出：maxItems=2、pinned=2 时添加 transient，visibleItems=2、overflowItems=1
TEST_F(DockManagerTest, TransientOverflow)
{
    manager->setMaxItems(2);
    DockItemData p1{"p1", "P1", "", "", false, 0};
    DockItemData p2{"p2", "P2", "", "", false, 0};
    manager->setPinnedItems({p1, p2});

    DockItemData t1{"t1", "T1", "", "", true, 0};
    manager->addTransientItem(t1);

    EXPECT_EQ(manager->visibleItems().size(), 2);
    EXPECT_EQ(manager->overflowItems().size(), 1);
    EXPECT_EQ(manager->overflowItems()[0].appId, "t1");
}

// 16. 固定项超 maxItems：pinned 全部可见，transient 全部溢出
TEST_F(DockManagerTest, MorePinnedThanMax)
{
    manager->setMaxItems(2);
    DockItemData p1{"p1", "P1", "", "", false, 0};
    DockItemData p2{"p2", "P2", "", "", false, 0};
    DockItemData p3{"p3", "P3", "", "", false, 0};
    manager->setPinnedItems({p1, p2, p3});
    EXPECT_EQ(manager->visibleItems().size(), 3);

    DockItemData t1{"t1", "T1", "", "", true, 0};
    manager->addTransientItem(t1);
    EXPECT_EQ(manager->visibleItems().size(), 3);
    EXPECT_EQ(manager->overflowItems().size(), 1);
}

// 17. 更新窗口数量：信号携带正确的 appId 和 count
TEST_F(DockManagerTest, UpdateWindowCount)
{
    DockItemData item{"app", "App", "", "", true, 0};
    manager->addTransientItem(item);

    QSignalSpy spy(manager, &DockManager::itemWindowCountChanged);
    manager->updateWindowCount("app", 3);

    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), "app");
    EXPECT_EQ(spy.at(0).at(1).toInt(), 3);
}

// 18. 窗口数量未变化时不发射信号
TEST_F(DockManagerTest, WindowCountNoChangeNoSignal)
{
    DockItemData item{"app", "App", "", "", true, 0};
    manager->addTransientItem(item);
    manager->updateWindowCount("app", 5); // 从默认1改为5

    QSignalSpy spy(manager, &DockManager::itemWindowCountChanged);
    manager->updateWindowCount("app", 5); // 相同值
    EXPECT_EQ(spy.count(), 0);
}

// 19. setPinnedItems 信号：itemAdded 发射两次，overflowChanged 发射
TEST_F(DockManagerTest, SetPinnedItemsSignals)
{
    QSignalSpy itemAddedSpy(manager, &DockManager::itemAdded);
    QSignalSpy overflowSpy(manager, &DockManager::overflowChanged);

    DockItemData p1{"p1", "P1", "", "", false, 0};
    DockItemData p2{"p2", "P2", "", "", false, 0};
    manager->setPinnedItems({p1, p2});

    EXPECT_EQ(itemAddedSpy.count(), 2);
    EXPECT_EQ(overflowSpy.count(), 1);
}

// 20. 移除临时项：itemRemoved 和 overflowChanged 均发射
TEST_F(DockManagerTest, RemoveTransientItem)
{
    DockItemData t1{"t1", "T1", "", "", true, 0};
    manager->addTransientItem(t1);

    QSignalSpy removedSpy(manager, &DockManager::itemRemoved);
    QSignalSpy overflowSpy(manager, &DockManager::overflowChanged);

    manager->removeTransientItem("t1");

    EXPECT_EQ(removedSpy.count(), 1);
    EXPECT_EQ(overflowSpy.count(), 1);
}
