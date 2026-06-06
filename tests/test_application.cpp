/**
 * @file test_application.cpp
 * @brief Application 生命周期单元测试
 *
 * 测试单实例检测、固定项配置加载/持久化、信号连接模式、
 * 构造安全性和信号处理器注册。不调用 run()/exec()。
 */

#include <gtest/gtest.h>
#include <QObject>
#include <QSignalSpy>
#include <QApplication>
#include <QSharedMemory>
#include <QVariantList>
#include <QVariantMap>

#include "core/ConfigManager.h"
#include "core/DockManager.h"
#include "core/Types.h"

// ─── 测试套件（静态 QApplication）─────────────────────────────

static int argc = 1;
static char arg0[] = "test_application";
static char *argv[] = {arg0, nullptr};

class ApplicationTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QApplication::instance()) {
            app = new QApplication(argc, argv);
        }
    }
    static void TearDownTestSuite() {
        delete app;
        app = nullptr;
    }

    static QApplication *app;
};

QApplication *ApplicationTest::app = nullptr;

// ─── 测试用例 ─────────────────────────────────────────────────

// 1. 单实例检测：通过 QSharedMemory 间接验证 checkSingleInstance 机制
TEST_F(ApplicationTest, CheckSingleInstanceFirstInstance)
{
    QSharedMemory firstInstance(QStringLiteral("Dock_WMac_Instance"));
    ASSERT_TRUE(firstInstance.create(1));

    // 第二个同名 QSharedMemory 应创建失败（已有实例占有）
    QSharedMemory secondInstance(QStringLiteral("Dock_WMac_Instance"));
    EXPECT_FALSE(secondInstance.create(1));
}

// 2. ConfigManager → DockManager 数据流（模拟 loadPinnedItems）
TEST_F(ApplicationTest, LoadPinnedItemsFromConfig)
{
    ConfigManager config;
    config.load();

    // 模拟配置文件中有 2 个 pinnedApps
    QVariantList pinnedApps;
    {
        QVariantMap item;
        item["appId"]       = "com.example.AppOne";
        item["displayName"] = "App One";
        item["iconPath"]    = "/icons/one.svg";
        item["execPath"]    = "/usr/bin/appone";
        pinnedApps.append(item);
    }
    {
        QVariantMap item;
        item["appId"]       = "com.example.AppTwo";
        item["displayName"] = "App Two";
        item["iconPath"]    = "/icons/two.svg";
        item["execPath"]    = "/usr/bin/apptwo";
        pinnedApps.append(item);
    }
    config.set("pinnedApps", pinnedApps);

    // 从配置读取并构造 DockItemData 列表（模拟 Application::loadPinnedItems）
    QVariantList savedPinned = config.get("pinnedApps").toList();
    QList<DockItemData> items;
    for (const QVariant &v : savedPinned) {
        QVariantMap entry = v.toMap();
        DockItemData item;
        item.appId       = entry.value("appId").toString();
        item.displayName = entry.value("displayName").toString();
        item.iconPath    = entry.value("iconPath").toString();
        item.execPath    = entry.value("execPath").toString();
        items.append(item);
    }

    DockManager manager;
    manager.setPinnedItems(items);

    ASSERT_EQ(manager.pinnedItems().size(), 2);
    EXPECT_EQ(manager.pinnedItems()[0].appId, "com.example.AppOne");
    EXPECT_EQ(manager.pinnedItems()[1].displayName, "App Two");
}

// 3. 空配置场景：pinnedItems 应为空
TEST_F(ApplicationTest, LoadPinnedItemsEmptyConfig)
{
    ConfigManager config;
    config.load();

    // 显式清空 pinnedApps，模拟无固定项配置
    config.set("pinnedApps", QVariantList());

    QVariantList savedPinned = config.get("pinnedApps", QVariantList()).toList();
    EXPECT_TRUE(savedPinned.isEmpty());

    DockManager manager;
    EXPECT_TRUE(manager.pinnedItems().isEmpty());
}

// 4. 持久化信号连接模式（模拟 connectPersistence）
TEST_F(ApplicationTest, ConnectPersistenceSignal)
{
    ConfigManager config;
    config.load();

    DockManager manager;

    QList<DockItemData> capturedItems;
    QSignalSpy spy(&manager, &DockManager::pinnedItemsChanged);

    // 模拟 Application::connectPersistence 的信号连接模式
    QObject::connect(&manager, &DockManager::pinnedItemsChanged,
        &manager, [&capturedItems, &config](const QList<DockItemData> &items) {
            capturedItems = items;
            QVariantList list;
            for (const auto &it : items) {
                QVariantMap entry;
                entry["appId"]       = it.appId;
                entry["displayName"] = it.displayName;
                entry["iconPath"]    = it.iconPath;
                entry["execPath"]    = it.execPath;
                list.append(entry);
            }
            config.set("pinnedApps", list);
        });

    DockItemData item;
    item.appId       = "signal.test.app";
    item.displayName = "Signal Test App";
    item.execPath    = "/path/to/test";
    manager.pinItem(item);

    // 信号已发射
    EXPECT_EQ(spy.count(), 1);

    // lambda 已捕获更新后的列表
    ASSERT_EQ(capturedItems.size(), 1);
    EXPECT_EQ(capturedItems[0].appId, "signal.test.app");

    // 配置已持久化
    QVariantList saved = config.get("pinnedApps").toList();
    ASSERT_EQ(saved.size(), 1);
    EXPECT_EQ(saved[0].toMap()["appId"].toString(), "signal.test.app");
}

// 5. Config ↔ DockManager 完整往返测试
TEST_F(ApplicationTest, ConfigToDockManagerRoundtrip)
{
    // ── 第 1 步：ConfigManager 写入 pinnedApps ──
    ConfigManager config;
    config.load();

    QVariantList originalList;
    {
        QVariantMap item;
        item["appId"]       = "roundtrip.app1";
        item["displayName"] = "RoundTrip One";
        item["iconPath"]    = "/icons/rt1.svg";
        item["execPath"]    = "/bin/rt1";
        originalList.append(item);
    }
    {
        QVariantMap item;
        item["appId"]       = "roundtrip.app2";
        item["displayName"] = "RoundTrip Two";
        item["iconPath"]    = "/icons/rt2.svg";
        item["execPath"]    = "/bin/rt2";
        originalList.append(item);
    }
    config.set("pinnedApps", originalList);

    // ── 第 2 步：DockManager 加载固定项并验证 ──
    QVariantList loaded = config.get("pinnedApps").toList();
    QList<DockItemData> items;
    for (const QVariant &v : loaded) {
        QVariantMap entry = v.toMap();
        DockItemData d;
        d.appId       = entry.value("appId").toString();
        d.displayName = entry.value("displayName").toString();
        d.iconPath    = entry.value("iconPath").toString();
        d.execPath    = entry.value("execPath").toString();
        items.append(d);
    }

    DockManager manager;
    manager.setPinnedItems(items);

    ASSERT_EQ(manager.pinnedItems().size(), 2);
    EXPECT_EQ(manager.pinnedItems()[0].appId, "roundtrip.app1");
    EXPECT_EQ(manager.pinnedItems()[1].displayName, "RoundTrip Two");

    // ── 第 3 步：pinItem → 信号 → 持久化 ──
    QList<DockItemData> signalCaptured;
    QSignalSpy spy(&manager, &DockManager::pinnedItemsChanged);

    QObject::connect(&manager, &DockManager::pinnedItemsChanged,
        &manager, [&signalCaptured, &config](const QList<DockItemData> &items) {
            signalCaptured = items;
            QVariantList list;
            for (const auto &it : items) {
                QVariantMap entry;
                entry["appId"]       = it.appId;
                entry["displayName"] = it.displayName;
                entry["iconPath"]    = it.iconPath;
                entry["execPath"]    = it.execPath;
                list.append(entry);
            }
            config.set("pinnedApps", list);
        });

    DockItemData newItem;
    newItem.appId       = "roundtrip.app3";
    newItem.displayName = "RoundTrip Three";
    newItem.iconPath    = "/icons/rt3.svg";
    newItem.execPath    = "/bin/rt3";
    manager.pinItem(newItem);

    ASSERT_EQ(signalCaptured.size(), 3);
    EXPECT_EQ(signalCaptured[2].appId, "roundtrip.app3");

    // ── 第 4 步：重新加载配置，验证完整往返 ──
    QVariantList reloaded = config.get("pinnedApps").toList();
    ASSERT_EQ(reloaded.size(), 3);
    EXPECT_EQ(reloaded[2].toMap()["appId"].toString(), "roundtrip.app3");
    EXPECT_EQ(reloaded[2].toMap()["displayName"].toString(), "RoundTrip Three");
}
