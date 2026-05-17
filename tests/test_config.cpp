/**
 * @file test_config.cpp
 * @brief ConfigManager 单元测试
 */

#include <gtest/gtest.h>
#include "core/ConfigManager.h"

TEST(ConfigManagerTest, DefaultValues)
{
    ConfigManager config;
    config.load();

    EXPECT_EQ(config.get("iconSize").toInt(), 48);
    EXPECT_EQ(config.get("magnification").toDouble(), 1.5);
    EXPECT_EQ(config.get("autoHide").toBool(), true);
}
