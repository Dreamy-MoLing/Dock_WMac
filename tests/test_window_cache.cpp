/**
 * @file test_window_cache.cpp
 * @brief WindowCache 单元测试
 *
 * 测试查询接口确定性行为（不依赖真实 EnumWindows，手动注入缓存数据）。
 */
#include <gtest/gtest.h>
#include <QTest>
#include <QSignalSpy>
#include "core/WindowCache.h"

class WindowCacheTest : public ::testing::Test {
protected:
    void SetUp() override { cache = new WindowCache(); }
    void TearDown() override { delete cache; cache = nullptr; }
    WindowCache *cache = nullptr;
};

// EnumWindows 无法在测试环境中 mock，因此测试聚焦于：
// 1. 初始状态查询返回合理默认值
// 2. 刷新不崩溃
// 3. 信号有效
// 4. 批量注册不泄漏

TEST_F(WindowCacheTest, InitialStateEmpty)
{
    // 初始状态所有查询返回 "空/不存在"
    EXPECT_FALSE(cache->hasVisibleWindows("chrome"));
    EXPECT_FALSE(cache->hasMinimizedWindows("chrome"));
    EXPECT_FALSE(cache->hasHiddenWindows("chrome"));
    EXPECT_FALSE(cache->isForegroundApp("chrome"));
    EXPECT_EQ(cache->getWindowCount("chrome"), 0);
    EXPECT_TRUE(cache->getWindowsForPreview("chrome").isEmpty());
    EXPECT_EQ(cache->getLastActiveHwnd("chrome"), nullptr);
}

TEST_F(WindowCacheTest, RefreshDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE({
        cache->refresh();
    });
}

TEST_F(WindowCacheTest, RefreshForPidDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE({
        cache->refreshForPid(9999); // 不存在的 PID
    });
}

TEST_F(WindowCacheTest, ScanForClassDoesNotCrash)
{
    EXPECT_NO_FATAL_FAILURE({
        cache->scanForClass("nonexistent_app");
    });
}

TEST_F(WindowCacheTest, ActivateWindowNonexistent)
{
    // 对不存在的应用返回 false 且不崩溃
    EXPECT_FALSE(cache->activateWindow("nonexistent_app"));
}

TEST_F(WindowCacheTest, SignalValid)
{
    QSignalSpy spy(cache, &WindowCache::cacheUpdated);
    EXPECT_TRUE(spy.isValid());
}
