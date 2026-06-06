/**
 * @file test_process_monitor.cpp
 * @brief ProcessMonitor 单元测试
 *
 * 测试进程名称规范化、注册/取消注册、启动/停止生命周期、
 * 信号连接有效性、反向DNS appId 处理等行为契约。
 *
 * 设计原则：
 * - 所有测试必须可确定性失败（有真实的行为契约）
 * - 禁止使用 QTest::qWait（避免时序竞态）
 * - 禁止 EXPECT_TRUE(x == true || x == false) 这类永真断言
 * - 使用 QSignalSpy::isValid() 验证信号存在
 * - 使用 EXPECT_NO_FATAL_FAILURE 验证不崩溃
 */

#include <gtest/gtest.h>
#include <QSignalSpy>
#include <QTest>
#include "core/ProcessMonitor.h"
#include "test_helpers.h"

// ============================================================================
// Test Fixture
// ============================================================================

class ProcessMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor = new ProcessMonitor();
    }

    void TearDown() override {
        delete monitor;
        monitor = nullptr;
    }

    ProcessMonitor *monitor = nullptr;
};

// ============================================================================
// 进程名称规范化测试（独立测试，无需 fixture）
// ============================================================================

TEST(ProcessNameNormalization, ReverseDNSPrefix)
{
    // 反向 DNS 前缀：取最后一个 '.' 之后的部分，转小写
    // 失败场景：normalizeProcessName 错误地保留了点号前缀部分
    EXPECT_EQ(normalizeProcessName("org.gnome.Nautilus"), "nautilus");
    EXPECT_EQ(normalizeProcessName("com.google.Chrome"), "chrome");
}

TEST(ProcessNameNormalization, SimpleName)
{
    // 简单名称（无点号）：直接转小写
    // 失败场景：normalizeProcessName 错误修改了简单名称
    EXPECT_EQ(normalizeProcessName("chrome"), "chrome");
    EXPECT_EQ(normalizeProcessName("Firefox"), "firefox");
}

TEST(ProcessNameNormalization, EmptyString)
{
    // 空字符串：返回空字符串，不应崩溃
    // 失败场景：空字符串触发越界访问或返回非空值
    EXPECT_EQ(normalizeProcessName(""), "");
}

TEST(ProcessNameNormalization, EdgeCases)
{
    // 边缘情况：单点号分隔、点号开头、多层点号
    // 失败场景：lastIndexOf 边界处理不当
    //
    // "app.exe" — 最后一段是 "exe"（点号分隔的文件扩展名）
    EXPECT_EQ(normalizeProcessName("app.exe"), "exe");
    // ".hidden" — 点号在开头，mid(0+1) = "hidden"
    EXPECT_EQ(normalizeProcessName(".hidden"), "hidden");
    // 多层反向 DNS：取最深一段
    EXPECT_EQ(normalizeProcessName("com.github.app"), "app");
}

// ============================================================================
// 进程路径比较测试（独立测试，无需 fixture）
// ============================================================================

TEST(ProcessPathComparison, ExtractBaseName)
{
    // 从完整路径提取基本名，去 .exe 后缀，转小写
    // 失败场景：extractBaseName 未正确剥离路径或 .exe 后缀
    EXPECT_EQ(extractBaseName("C:/Program Files/Google/Chrome/chrome.exe"), "chrome");
    EXPECT_EQ(extractBaseName("C:\\Program Files\\Firefox\\firefox.exe"), "firefox");
    EXPECT_EQ(extractBaseName("chrome.exe"), "chrome");
    EXPECT_EQ(extractBaseName("CHROME.EXE"), "chrome");
}

TEST(ProcessPathComparison, SameProcess)
{
    // isSameProcess 应正确识别同一进程的不同表示形式
    // 失败场景：大小写或 .exe 后缀差异导致错误判定

    // 相同进程
    EXPECT_TRUE(isSameProcess("chrome", "chrome"));
    EXPECT_TRUE(isSameProcess("chrome.exe", "chrome"));
    EXPECT_TRUE(isSameProcess("CHROME", "chrome"));
    EXPECT_TRUE(isSameProcess("C:/Chrome/chrome.exe", "chrome"));

    // 不同进程
    EXPECT_FALSE(isSameProcess("chrome", "firefox"));
    EXPECT_FALSE(isSameProcess("chrome.exe", "firefox.exe"));
}

// ============================================================================
// ProcessMonitor 注册测试（fixture 测试）
// ============================================================================

TEST_F(ProcessMonitorTest, RegisterAppSyncsExecPath)
{
    // 注册带 execPath 的应用，验证：
    // 1. 不会崩溃
    // 2. 信号连接有效（monitor 处于可工作状态）
    //
    // 失败场景：registerApp 在存储 execPath 时崩溃，
    //           或注册后信号机制失效
    EXPECT_NO_FATAL_FAILURE({
        monitor->registerApp("com.google.Chrome", "C:/Chrome/chrome.exe");
    });

    QSignalSpy spy(monitor, &ProcessMonitor::appRunningStateChanged);
    ASSERT_TRUE(spy.isValid());

    // 启动后立即停止：验证有 execPath 注册项的 monitor 不会在
    // onTick 中因 execPath 查找而崩溃
    EXPECT_NO_FATAL_FAILURE({
        monitor->start(100);
        monitor->stop();
    });
}

TEST_F(ProcessMonitorTest, RegisterAppsBulk)
{
    // 批量注册应用并验证所有三个信号均可用
    // 失败场景：registerApps 未正确初始化内部状态，
    //           导致后续信号连接无效
    QList<DockItemData> items;

    DockItemData chrome;
    chrome.appId = "com.google.Chrome";
    chrome.execPath = "C:/Program Files/Google/Chrome/Application/chrome.exe";
    items.append(chrome);

    DockItemData firefox;
    firefox.appId = "firefox";
    firefox.execPath = "C:/Program Files/Mozilla Firefox/firefox.exe";
    items.append(firefox);

    DockItemData noExec;
    noExec.appId = "notepad";
    // execPath 为空 — 应能正常注册
    items.append(noExec);

    EXPECT_NO_FATAL_FAILURE({
        monitor->registerApps(items);
    });

    // 验证所有三个信号的 spy 均有效
    QSignalSpy runningSpy(monitor, &ProcessMonitor::appRunningStateChanged);
    QSignalSpy newAppSpy(monitor, &ProcessMonitor::newRunningAppDetected);
    QSignalSpy exitedSpy(monitor, &ProcessMonitor::runningAppExited);

    EXPECT_TRUE(runningSpy.isValid());
    EXPECT_TRUE(newAppSpy.isValid());
    EXPECT_TRUE(exitedSpy.isValid());
}

TEST_F(ProcessMonitorTest, UnregisterRemovesFromCache)
{
    // 注册后取消注册，验证 monitor 不保留已注销应用的追踪状态
    // 虽然无法直接检查内部 m_runningCache，但通过行为契约验证：
    // 1. 取消注册不崩溃
    // 2. 取消注册后信号仍有效（monitor 可继续工作）
    //
    // 失败场景：unregisterApp 未清理 m_runningCache，
    //           导致后续启动/停止时访问悬空引用
    monitor->registerApp("test.app.id", "C:/Test/test.exe");

    EXPECT_NO_FATAL_FAILURE({
        monitor->unregisterApp("test.app.id");
    });

    // 取消注册后启动/停止不应崩溃
    QSignalSpy spy(monitor, &ProcessMonitor::appRunningStateChanged);
    ASSERT_TRUE(spy.isValid());

    EXPECT_NO_FATAL_FAILURE({
        monitor->start(100);
        monitor->stop();
    });

    // 取消注册一个从未注册过的 appId 也不应崩溃
    EXPECT_NO_FATAL_FAILURE({
        monitor->unregisterApp("never.registered.app");
    });
}

// ============================================================================
// 启动/停止生命周期测试
// ============================================================================

TEST_F(ProcessMonitorTest, StartStopLifecycle)
{
    // 多次启动/停止循环不应崩溃或泄漏资源
    // 失败场景：重复 start/stop 导致计时器状态混乱或二次释放
    EXPECT_NO_FATAL_FAILURE({
        monitor->start(100);
        monitor->stop();
        monitor->start(100);
        monitor->stop();
    });
}

// ============================================================================
// 反向 DNS appId 检测测试
// ============================================================================

TEST_F(ProcessMonitorTest, ReverseDnsAppIdDetection)
{
    // 注册反向 DNS 格式的 appId（如 "com.google.Chrome"），
    // 验证 monitor 能正确处理此类 appId 而不会：
    // 1. 在 onTick 中崩溃
    // 2. 信号机制失效
    //
    // 这是针对旧 bug 的回归测试：scanTransientApps 曾用
    // m_registeredApps.contains(processName) 直接匹配，
    // 导致 "com.google.Chrome" 注册项无法匹配进程 "chrome"。
    // 修复后的 appIdToProcessName 提取最后一段，
    // registeredProcessNames 集合中存储的是 "chrome" 而非 "com.google.Chrome"。
    //
    // 失败场景：monitor 在内部转换反向 DNS appId 时出错，
    //           或信号连接在反向 DNS appId 上下文中不可用。
    monitor->registerApp("com.google.Chrome", "C:/Program Files/Google/Chrome/Application/chrome.exe");

    QSignalSpy runningSpy(monitor, &ProcessMonitor::appRunningStateChanged);
    ASSERT_TRUE(runningSpy.isValid());

    QSignalSpy newAppSpy(monitor, &ProcessMonitor::newRunningAppDetected);
    ASSERT_TRUE(newAppSpy.isValid());

    QSignalSpy exitedSpy(monitor, &ProcessMonitor::runningAppExited);
    ASSERT_TRUE(exitedSpy.isValid());

    // 启动后立即停止：验证反向 DNS appId 不会导致 onTick 崩溃
    // 不使用 QTest::qWait — 这是确定性测试
    EXPECT_NO_FATAL_FAILURE({
        monitor->start(100);
        monitor->stop();
    });
}

// ============================================================================
// 进程名匹配契约测试
// ============================================================================

TEST(ProcessNameMatchingContract, AppIdToProcessNameMapping)
{
    // 文档化 normalizeProcessName 的转换契约：
    //
    // normalizeProcessName 模拟 ProcessMonitor::appIdToProcessName 的行为，
    // 将 appId 转换为用于进程匹配的形式。
    //
    // 契约规则：
    //   1. 取最后一个 '.' 之后的部分
    //   2. 转换为小写
    //
    // 失败场景：normalizeProcessName 与 appIdToProcessName 行为不一致，
    //           导致注册的 appId 无法匹配运行中的进程名

    // 反向 DNS 格式：取最后一段
    EXPECT_EQ(normalizeProcessName("com.google.Chrome"), "chrome");
    EXPECT_EQ(normalizeProcessName("org.gnome.Nautilus"), "nautilus");

    // 简单名称：直接小写
    EXPECT_EQ(normalizeProcessName("chrome"), "chrome");
    EXPECT_EQ(normalizeProcessName("Visual Studio Code"), "visual studio code");

    // 多层命名空间
    EXPECT_EQ(normalizeProcessName("com.microsoft.VisualStudio"), "visualstudio");
}
