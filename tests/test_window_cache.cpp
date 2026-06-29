/**
 * @file test_window_cache.cpp
 * @brief WindowCache 单元测试
 *
 * 测试查询接口确定性行为（不依赖真实 EnumWindows，手动注入缓存数据）。
 */
#include "core/WindowCache.h"

#include <gtest/gtest.h>
#include <QFileInfo>
#include <QSignalSpy>

#include <algorithm>

#ifdef Q_OS_WIN
static QString currentExeKey()
{
    wchar_t path[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, path, MAX_PATH);
    return QFileInfo(QString::fromWCharArray(path)).baseName().toLower();
}
#endif

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
// 4. 窗口数量/激活/选择器调用不崩溃

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

TEST_F(WindowCacheTest, WindowCountNonNegative)
{
    // getWindowCount() 对任意进程名应返回 >= 0 的值
    int count = cache->getWindowCount("explorer");
    EXPECT_GE(count, 0);
}

TEST_F(WindowCacheTest, ActivateWindowDoesNotCrash)
{
    // activateWindow("nonexistent_app") 应返回 bool 且不崩溃
    EXPECT_NO_FATAL_FAILURE({
        bool result = cache->activateWindow("nonexistent_app");
        (void)result;
    });
}

TEST_F(WindowCacheTest, ShowWindowPickerNoCrash)
{
    // showWindowPicker() 在任何环境下不崩溃
    EXPECT_NO_FATAL_FAILURE({
        cache->showWindowPicker();
    });
}

TEST_F(WindowCacheTest, SignalValid)
{
    QSignalSpy spy(cache, &WindowCache::cacheUpdated);
    EXPECT_TRUE(spy.isValid());
}

TEST_F(WindowCacheTest, ActivationQueryIncludesHiddenUntitledWindowExcludedFromPreview)
{
#ifdef Q_OS_WIN
    const wchar_t className[] = L"DockWMacActivationCacheTestWindow";
    WNDCLASSW wc = {};
    wc.lpfnWndProc = DefWindowProcW;
    wc.hInstance = GetModuleHandleW(nullptr);
    wc.lpszClassName = className;
    RegisterClassW(&wc);

    HWND hwnd = CreateWindowExW(
        0, className, L"", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 100, 100,
        nullptr, nullptr, wc.hInstance, nullptr);
    ASSERT_NE(hwnd, nullptr);

    const QString key = currentExeKey();
    cache->scanForClass(key);

    const WindowList previewWindows = cache->getWindowsForPreview(key);
    EXPECT_FALSE(std::any_of(previewWindows.begin(), previewWindows.end(),
        [hwnd](const CachedWindowInfo &w) { return w.hwnd == hwnd; }));

    const WindowList activationWindows = cache->getWindowsForActivation(key);
    EXPECT_TRUE(std::any_of(activationWindows.begin(), activationWindows.end(),
        [hwnd](const CachedWindowInfo &w) { return w.hwnd == hwnd; }));

    DestroyWindow(hwnd);
    UnregisterClassW(className, wc.hInstance);
#else
    GTEST_SKIP() << "Windows-specific activation filtering test";
#endif
}
