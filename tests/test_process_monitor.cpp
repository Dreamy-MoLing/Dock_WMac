/**
 * @file test_process_monitor.cpp
 * @brief ProcessMonitor 单元测试
 *
 * 测试进程名称规范化、注册/取消注册、状态检测等功能。
 */

#include <gtest/gtest.h>
#include <QSignalSpy>
#include "core/ProcessMonitor.h"
#include "test_helpers.h"

class ProcessMonitorTest : public ::testing::Test {
protected:
    void SetUp() override {
        monitor = new ProcessMonitor();
    }
    void TearDown() override {
        delete monitor;
    }
    ProcessMonitor *monitor;
};

// ========== 进程名称规范化测试 ==========

TEST(ProcessNameNormalization, ReverseDNSPrefix)
{
    // 反向 DNS 前缀应提取最后一段
    EXPECT_EQ(normalizeProcessName("org.gnome.Nautilus"), "nautilus");
    EXPECT_EQ(normalizeProcessName("com.example.MyApp"), "myapp");
    EXPECT_EQ(normalizeProcessName("com.microsoft.VisualStudio"), "visualstudio");
}

TEST(ProcessNameNormalization, SimpleName)
{
    // 简单名称应直接转小写
    EXPECT_EQ(normalizeProcessName("chrome"), "chrome");
    EXPECT_EQ(normalizeProcessName("Firefox"), "firefox");
    EXPECT_EQ(normalizeProcessName("EXPLORER"), "explorer");
}

TEST(ProcessNameNormalization, EmptyString)
{
    // 空字符串应返回空字符串
    EXPECT_EQ(normalizeProcessName(""), "");
}

TEST(ProcessNameNormalization, SingleDot)
{
    // 单个点分隔的情况
    EXPECT_EQ(normalizeProcessName("app.exe"), "exe");
    EXPECT_EQ(normalizeProcessName(".hidden"), "hidden");
}

TEST(ProcessNameNormalization, MultipleDots)
{
    // 多个点的情况
    EXPECT_EQ(normalizeProcessName("com.github.desktop.app"), "app");
    EXPECT_EQ(normalizeProcessName("org.kde.plasma.desktop"), "desktop");
}

// ========== 进程路径比较测试 ==========

TEST(ProcessPathComparison, ExtractBaseName)
{
    // 完整路径应提取基本名称
    EXPECT_EQ(extractBaseName("C:/Program Files/Google/Chrome/chrome.exe"), "chrome");
    EXPECT_EQ(extractBaseName("C:\\Program Files\\Firefox\\firefox.exe"), "firefox");
    EXPECT_EQ(extractBaseName("chrome.exe"), "chrome");
    EXPECT_EQ(extractBaseName("CHROME.EXE"), "chrome");
}

TEST(ProcessPathComparison, SameProcessDetection)
{
    // 同一进程的不同表示应匹配
    EXPECT_TRUE(isSameProcess("chrome", "chrome"));
    EXPECT_TRUE(isSameProcess("chrome.exe", "chrome"));
    EXPECT_TRUE(isSameProcess("CHROME", "chrome"));
    EXPECT_TRUE(isSameProcess("C:/Chrome/chrome.exe", "chrome"));

    // 不同进程不应匹配
    EXPECT_FALSE(isSameProcess("chrome", "firefox"));
    EXPECT_FALSE(isSameProcess("chrome.exe", "firefox.exe"));
}

TEST(ProcessPathComparison, NoExtension)
{
    // 没有 .exe 后缀的情况
    EXPECT_EQ(extractBaseName("chrome"), "chrome");
    EXPECT_EQ(extractBaseName("CHROME"), "chrome");
}

// ========== ProcessMonitor 注册测试 ==========

TEST_F(ProcessMonitorTest, RegisterApp)
{
    // 注册应用后应能正常工作
    monitor->registerApp("test_app");
    // 不应崩溃或抛出异常
}

TEST_F(ProcessMonitorTest, RegisterMultipleApps)
{
    // 批量注册应用
    QList<DockItemData> items;
    DockItemData item1;
    item1.appId = "app1";
    items.append(item1);

    DockItemData item2;
    item2.appId = "app2";
    items.append(item2);

    monitor->registerApps(items);
    // 不应崩溃或抛出异常
}

TEST_F(ProcessMonitorTest, UnregisterApp)
{
    // 注册后取消注册
    monitor->registerApp("test_app");
    monitor->unregisterApp("test_app");
    // 不应崩溃或抛出异常
}

TEST_F(ProcessMonitorTest, UnregisterNonexistentApp)
{
    // 取消注册未注册的应用不应崩溃
    monitor->unregisterApp("nonexistent_app");
}

// ========== ProcessMonitor 信号测试 ==========

TEST_F(ProcessMonitorTest, SignalsExist)
{
    // 验证信号存在且可连接
    QSignalSpy runningSpy(monitor, &ProcessMonitor::appRunningStateChanged);
    QSignalSpy newAppSpy(monitor, &ProcessMonitor::newRunningAppDetected);
    QSignalSpy exitedSpy(monitor, &ProcessMonitor::runningAppExited);

    EXPECT_TRUE(runningSpy.isValid());
    EXPECT_TRUE(newAppSpy.isValid());
    EXPECT_TRUE(exitedSpy.isValid());
}

// ========== ProcessMonitor 启动/停止测试 ==========

TEST_F(ProcessMonitorTest, StartStop)
{
    // 启动和停止不应崩溃
    monitor->start(100);  // 短间隔用于测试
    monitor->stop();
}

TEST_F(ProcessMonitorTest, StartStopMultiple)
{
    // 多次启动停止不应崩溃
    monitor->start(100);
    monitor->stop();
    monitor->start(100);
    monitor->stop();
}

TEST_F(ProcessMonitorTest, StopBeforeStart)
{
    // 在启动前停止不应崩溃
    monitor->stop();
}
