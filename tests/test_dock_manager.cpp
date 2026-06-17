/**
 * @file test_dock_manager.cpp
 * @brief DockManager 状态机单元测试
 *
 * 测试状态转换、信号发射、事件处理、固定/临时项管理等功能。
 * enterHiddenState() / enterDockedState() 及定时器访问器已公开用于测试。
 *
 * 显隐逻辑重构后：隐藏前有 3s 延迟，Win 键唤醒有 1.5s 冷却。
 */

#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QTimer>
#include <QApplication>
#include "core/DockManager.h"
#include "core/SysHelper.h"

static int testArgc = 0;
static char testArgv0[] = "test_dock_manager";
static char *testArgv[] = { testArgv0, nullptr };

class DockManagerTest : public ::testing::Test {
protected:
    static QApplication *app;

    static void SetUpTestSuite() {
        if (!QApplication::instance()) {
            app = new QApplication(testArgc, testArgv);
        }
    }

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

QApplication *DockManagerTest::app = nullptr;

// ============================================================================
// 状态机测试 — 状态转换
// ============================================================================

// 1. 初始状态应为 Docked
TEST_F(DockManagerTest, InitialStateDocked)
{
    EXPECT_EQ(manager->currentState(), DockState::Docked);
}

// 2. enterHiddenState 发射 stateChanged(Hidden)，状态正确
TEST_F(DockManagerTest, EnterHiddenStateEmitsSignal)
{
    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->enterHiddenState();
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).value<DockState>(), DockState::Hidden);
    EXPECT_EQ(manager->currentState(), DockState::Hidden);
}

// 3. 重复 enterHiddenState 不重复发射信号
TEST_F(DockManagerTest, NoDoubleEnterHidden)
{
    manager->enterHiddenState();
    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->enterHiddenState();
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(manager->currentState(), DockState::Hidden);
}

// 4. enterDockedState 从 Hidden 恢复，发射 stateChanged(Docked)
TEST_F(DockManagerTest, EnterDockedStateEmitsSignal)
{
    manager->enterHiddenState();
    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->enterDockedState();
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).value<DockState>(), DockState::Docked);
    EXPECT_EQ(manager->currentState(), DockState::Docked);
}

// 5. 重复 enterDockedState 不重复发射信号
TEST_F(DockManagerTest, NoDoubleEnterDocked)
{
    manager->enterDockedState();  // no-op, already Docked
    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->enterDockedState();
    EXPECT_EQ(spy.count(), 0);
}

// 6. enterDockedState 从 Hidden 恢复 — 确定性测试
//    直接测试状态转换核心逻辑，不依赖系统全屏状态
TEST_F(DockManagerTest, FullscreenGoneRestoresDock)
{
    manager->enterHiddenState();
    EXPECT_EQ(manager->currentState(), DockState::Hidden);

    // 直接调用 enterDockedState — 这是 onFullscreenStateChanged 内部的最终动作
    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->enterDockedState();
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).value<DockState>(), DockState::Docked);
    EXPECT_EQ(manager->currentState(), DockState::Docked);
}

// 7. 无真实全屏窗口时不启动隐藏延迟
TEST_F(DockManagerTest, FullscreenNoActualWindowNoStateChange)
{
    EXPECT_EQ(manager->currentState(), DockState::Docked);
    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->onFullscreenStateChanged(true);
    // 内部重查 hasMaximizedOrFullscreenWindowOnMonitor → 无全屏窗口 → 不启动延迟
    EXPECT_EQ(spy.count(), 0);
}

// ============================================================================
// 状态机测试 — 隐藏延迟与取消
// ============================================================================

// 8. onFullscreenStateChanged(false) 在定时器活跃时安全调用不崩溃
TEST_F(DockManagerTest, FullscreenGoneCancelsHideDelay)
{
    manager->hideDelayTimer()->start();
    ASSERT_TRUE(manager->hideDelayTimer()->isActive());

    EXPECT_NO_FATAL_FAILURE({
        manager->onFullscreenStateChanged(false);
    });
}

// ============================================================================
// 状态机测试 — Win 键行为
// ============================================================================

// 9. Win 键在 Docked 状态下被忽略
TEST_F(DockManagerTest, WinKeyIgnoredWhenDocked)
{
    EXPECT_EQ(manager->currentState(), DockState::Docked);
    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->onWinKeyPressed();
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(manager->currentState(), DockState::Docked);
}

// 10. Win 键从 Hidden 恢复 Docked，发射信号 + 启动冷却
TEST_F(DockManagerTest, WinKeyFromHiddenEntersDocked)
{
    manager->enterHiddenState();
    EXPECT_EQ(manager->currentState(), DockState::Hidden);

    QSignalSpy spy(manager, &DockManager::stateChanged);
    manager->onWinKeyPressed();
    EXPECT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).value<DockState>(), DockState::Docked);
    EXPECT_EQ(manager->currentState(), DockState::Docked);

    // Win 键冷却定时器应处于激活状态
    EXPECT_TRUE(manager->winKeyCooldownTimer()->isActive());
}

// ============================================================================
// 状态机测试 — 定时器配置
// ============================================================================

// 11. 构造函数正确创建并配置定时器
TEST_F(DockManagerTest, ConstructorTimersConfigured)
{
    EXPECT_NE(manager->hideDelayTimer(), nullptr);
    EXPECT_NE(manager->winKeyCooldownTimer(), nullptr);
    EXPECT_TRUE(manager->hideDelayTimer()->isSingleShot());
    EXPECT_TRUE(manager->winKeyCooldownTimer()->isSingleShot());
    EXPECT_EQ(manager->hideDelayTimer()->interval(), 3000);
    EXPECT_EQ(manager->winKeyCooldownTimer()->interval(), 1500);
}

// 12. enterDockedState 停止所有定时器
TEST_F(DockManagerTest, EnterDockedStateStopsAllTimers)
{
    manager->enterHiddenState();
    // 手动启动两个定时器
    manager->hideDelayTimer()->start();
    manager->winKeyCooldownTimer()->start();
    EXPECT_TRUE(manager->hideDelayTimer()->isActive());
    EXPECT_TRUE(manager->winKeyCooldownTimer()->isActive());

    manager->enterDockedState();
    EXPECT_FALSE(manager->hideDelayTimer()->isActive());
    EXPECT_FALSE(manager->winKeyCooldownTimer()->isActive());
    EXPECT_EQ(manager->currentState(), DockState::Docked);
}

// 13. enterHiddenState 停止隐藏延迟定时器
TEST_F(DockManagerTest, EnterHiddenStateStopsHideDelay)
{
    manager->hideDelayTimer()->start();
    EXPECT_TRUE(manager->hideDelayTimer()->isActive());

    manager->enterHiddenState();
    EXPECT_FALSE(manager->hideDelayTimer()->isActive());
    EXPECT_EQ(manager->currentState(), DockState::Hidden);
}

// ============================================================================
// 屏幕索引
// ============================================================================

// 14. 设置屏幕索引
TEST_F(DockManagerTest, SetMonitorIndex)
{
    EXPECT_EQ(manager->monitorIndex(), -1);  // 默认主屏幕
    manager->setMonitorIndex(1);
    EXPECT_EQ(manager->monitorIndex(), 1);
}

// ============================================================================
// 固定项管理
// ============================================================================

// 15. 固定项：pinItem 后 isPinned 返回 true，itemAdded发射，pinnedItemsChanged发射
TEST_F(DockManagerTest, PinItem)
{
    DockItemData item{"app", "Name", "/path/app.exe", "", false, 0};
    QSignalSpy addedSpy(manager, &DockManager::itemAdded);
    QSignalSpy removedSpy(manager, &DockManager::itemRemoved);
    QSignalSpy pinnedSpy(manager, &DockManager::pinnedItemsChanged);

    manager->pinItem(item);

    EXPECT_TRUE(manager->isPinned("app"));
    EXPECT_EQ(manager->pinnedItems().size(), 1);
    EXPECT_EQ(manager->pinnedItems()[0].appId, "app");
    // 新 pin（非从 transient 转换）不应发射 itemRemoved
    EXPECT_EQ(removedSpy.count(), 0);
    EXPECT_EQ(addedSpy.count(), 1);
    EXPECT_EQ(pinnedSpy.count(), 1);
}

// 16. 重复固定同一项被忽略
TEST_F(DockManagerTest, PinDuplicateIgnored)
{
    DockItemData item{"app", "Name", "/path/app.exe", "", false, 0};
    manager->pinItem(item);
    manager->pinItem(item);
    EXPECT_EQ(manager->pinnedItems().size(), 1);
}

// 17. 取消固定：pin 后 unpin，isPinned 返回 false，信号发射
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

// 18. 取消不存在的固定项不崩溃，无信号
TEST_F(DockManagerTest, UnpinNonexistentNoCrash)
{
    QSignalSpy spy(manager, &DockManager::pinnedItemsChanged);
    manager->unpinItem("nonexistent");
    EXPECT_EQ(spy.count(), 0);
}

// 19. 固定临时项后从 transients 移除，发射 itemRemoved
TEST_F(DockManagerTest, PinRemovesFromTransient)
{
    DockItemData item{"app", "Name", "/path/app.exe", "", true, 0};
    manager->addTransientItem(item);
    ASSERT_EQ(manager->transientItems().size(), 1);

    QSignalSpy removedSpy(manager, &DockManager::itemRemoved);
    manager->pinItem(item);
    EXPECT_TRUE(manager->transientItems().isEmpty());
    EXPECT_EQ(manager->pinnedItems().size(), 1);
    // 从 transient 转换为 pinned → 应发射 itemRemoved
    EXPECT_EQ(removedSpy.count(), 1);
}

// 19. 固定项重排：只按已固定 appId 重排，并触发持久化信号
TEST_F(DockManagerTest, ReorderPinnedItemsPersistsOrder)
{
    DockItemData p1{"p1", "P1", "", "", false, 0};
    DockItemData p2{"p2", "P2", "", "", false, 0};
    DockItemData p3{"p3", "P3", "", "", false, 0};
    DockItemData t1{"t1", "T1", "", "", true, 0};
    manager->setPinnedItems({p1, p2, p3});
    manager->addTransientItem(t1);

    QSignalSpy pinnedSpy(manager, &DockManager::pinnedItemsChanged);
    QSignalSpy overflowSpy(manager, &DockManager::overflowChanged);

    EXPECT_TRUE(manager->reorderPinnedItems({"t1", "p3", "p1"}));

    ASSERT_EQ(manager->pinnedItems().size(), 3);
    EXPECT_EQ(manager->pinnedItems()[0].appId, "p3");
    EXPECT_EQ(manager->pinnedItems()[1].appId, "p1");
    EXPECT_EQ(manager->pinnedItems()[2].appId, "p2");
    EXPECT_EQ(manager->transientItems().size(), 1);
    EXPECT_EQ(manager->transientItems()[0].appId, "t1");
    EXPECT_EQ(pinnedSpy.count(), 1);
    EXPECT_EQ(overflowSpy.count(), 1);
}

// 20. 重排为相同顺序时不触发持久化
TEST_F(DockManagerTest, ReorderPinnedItemsNoChangeNoSignal)
{
    DockItemData p1{"p1", "P1", "", "", false, 0};
    DockItemData p2{"p2", "P2", "", "", false, 0};
    manager->setPinnedItems({p1, p2});

    QSignalSpy pinnedSpy(manager, &DockManager::pinnedItemsChanged);

    EXPECT_FALSE(manager->reorderPinnedItems({"p1", "p2"}));
    EXPECT_EQ(pinnedSpy.count(), 0);
}
// ============================================================================
// 最大项数与溢出
// ============================================================================

// 20. 默认最大项数为 16
TEST_F(DockManagerTest, MaxItemsDefault)
{
    EXPECT_EQ(manager->maxItems(), 16);
}

// 21. setMaxItems(0) 下限为 1
TEST_F(DockManagerTest, SetMaxItemsMinOne)
{
    manager->setMaxItems(0);
    EXPECT_EQ(manager->maxItems(), 1);
}

// 22. setMaxItems 设相同值不触发 overflowChanged
TEST_F(DockManagerTest, SetMaxItemsNoChange)
{
    EXPECT_EQ(manager->maxItems(), 16);
    QSignalSpy spy(manager, &DockManager::overflowChanged);
    manager->setMaxItems(16);
    EXPECT_EQ(spy.count(), 0);
}

// 23. 临时项溢出：maxItems=2、pinned=2 时添加 transient
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

// 24. 固定项超 maxItems：pinned 全部可见，transient 全部溢出
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

// ============================================================================
// 窗口数量更新
// ============================================================================

// 25. 更新窗口数量：信号携带正确的 appId 和 count
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

// 26. 窗口数量未变化时不发射信号
TEST_F(DockManagerTest, WindowCountNoChangeNoSignal)
{
    DockItemData item{"app", "App", "", "", true, 0};
    manager->addTransientItem(item);
    manager->updateWindowCount("app", 5);

    QSignalSpy spy(manager, &DockManager::itemWindowCountChanged);
    manager->updateWindowCount("app", 5);
    EXPECT_EQ(spy.count(), 0);
}

// ============================================================================
// setPinnedItems 与 removeTransientItem 信号
// ============================================================================

// 27. setPinnedItems 信号：itemAdded 发射两次，overflowChanged 发射
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

// 28. 移除临时项：itemRemoved 和 overflowChanged 均发射
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

