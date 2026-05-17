/**
 * @file test_dock_manager.cpp
 * @brief DockManager 状态机单元测试
 */

#include <gtest/gtest.h>
#include "core/DockManager.h"
#include "core/SysHelper.h"

TEST(DockManagerTest, InitialStateIsDocked)
{
    SysHelper sysHelper;
    DockManager manager;
    manager.initialize(&sysHelper);

    EXPECT_EQ(manager.currentState(), DockState::Docked);
}
