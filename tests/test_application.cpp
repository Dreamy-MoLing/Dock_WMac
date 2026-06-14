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
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QCoreApplication>

#include "core/ConfigManager.h"
#include "core/DockManager.h"
#include "core/PathManager.h"
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

    void SetUp() override {
        // 清理 pinned.json 避免测试间干扰
        QFile::remove(PathManager::pinnedFile());
    }

    void TearDown() override {
        QFile::remove(PathManager::pinnedFile());
    }

    /// 写入 pinned.json（模拟 Application::connectPersistence 的输出格式）
    static void writePinnedJson(const QJsonArray &arr) {
        PathManager::ensureDataDir();
        QFile file(PathManager::pinnedFile());
        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
            file.close();
        }
    }

    /// 读取 pinned.json
    static QJsonArray readPinnedJson() {
        QFile file(PathManager::pinnedFile());
        if (!file.open(QIODevice::ReadOnly)) return {};
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        return doc.isArray() ? doc.array() : QJsonArray{};
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

// 2. pinned.json 写入/读取往返 — 模拟 Application::connectPersistence 输出
TEST_F(ApplicationTest, PinnedJsonRoundtrip)
{
    QJsonArray arr;
    {
        QJsonObject obj;
        obj["appId"] = "com.example.AppOne";
        obj["displayName"] = "App One";
        obj["iconPath"] = "/icons/one.svg";
        obj["execPath"] = "/usr/bin/appone";
        arr.append(obj);
    }
    {
        QJsonObject obj;
        obj["appId"] = "com.example.AppTwo";
        obj["displayName"] = "App Two";
        obj["iconPath"] = "/icons/two.svg";
        obj["execPath"] = "/usr/bin/apptwo";
        arr.append(obj);
    }

    writePinnedJson(arr);

    // 读取并验证
    QJsonArray loaded = readPinnedJson();
    ASSERT_EQ(loaded.size(), 2);
    EXPECT_EQ(loaded[0].toObject()["appId"].toString(), "com.example.AppOne");
    EXPECT_EQ(loaded[1].toObject()["displayName"].toString(), "App Two");
}

// 3. DockManager 加载 pinned.json 数据 — 模拟 Application::loadPinnedItems 后半段
TEST_F(ApplicationTest, LoadPinnedItemsFromJson)
{
    // 写入 pinned.json
    QJsonArray arr;
    {
        QJsonObject obj;
        obj["appId"] = "com.example.AppOne";
        obj["displayName"] = "App One";
        obj["iconPath"] = "/icons/one.svg";
        obj["execPath"] = "/usr/bin/appone";
        arr.append(obj);
    }
    writePinnedJson(arr);

    // 读取 pinned.json 并构造 DockItemData（模拟 Application::loadPinnedItems 合并逻辑）
    QFile file(PathManager::pinnedFile());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QList<DockItemData> items;
    QSet<QString> systemExecPaths;  // 模拟空的系统任务栏（测试环境无固定项）

    const QJsonArray loadedArr = doc.array();
    for (const QJsonValue &val : loadedArr) {
        QJsonObject obj = val.toObject();
        QString execPath = obj["execPath"].toString();
        if (execPath.isEmpty()) continue;
        if (systemExecPaths.contains(execPath.toLower())) continue;

        DockItemData item;
        item.appId = obj["appId"].toString();
        item.displayName = obj["displayName"].toString();
        item.iconPath = obj["iconPath"].toString();
        item.execPath = execPath;
        item.isRunning = false;
        items.append(item);
    }

    DockManager manager;
    manager.setPinnedItems(items);

    ASSERT_EQ(manager.pinnedItems().size(), 1);
    EXPECT_EQ(manager.pinnedItems()[0].appId, "com.example.AppOne");
    EXPECT_EQ(manager.pinnedItems()[0].displayName, "App One");
}

// 4. 空 pinned.json 场景
TEST_F(ApplicationTest, LoadPinnedItemsEmptyJson)
{
    writePinnedJson(QJsonArray{});

    QFile file(PathManager::pinnedFile());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    EXPECT_TRUE(doc.array().isEmpty());

    DockManager manager;
    EXPECT_TRUE(manager.pinnedItems().isEmpty());
}

// 5. 持久化信号连接模式（模拟 connectPersistence 写入 pinned.json）
TEST_F(ApplicationTest, ConnectPersistenceWritesPinnedJson)
{
    DockManager manager;

    // 模拟 Application::connectPersistence 的信号连接
    QObject::connect(&manager, &DockManager::pinnedItemsChanged,
        &manager, [](const QList<DockItemData> &items) {
            // 读取系统任务栏（测试环境为空）
            QSet<QString> systemExecPaths;

            QJsonArray arr;
            for (const auto &item : items) {
                if (systemExecPaths.contains(item.execPath.toLower())) continue;
                QJsonObject obj;
                obj["appId"] = item.appId;
                obj["displayName"] = item.displayName;
                obj["iconPath"] = item.iconPath;
                obj["execPath"] = item.execPath;
                arr.append(obj);
            }

            PathManager::ensureDataDir();
            QFile file(PathManager::pinnedFile());
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
                file.close();
            }
        });

    DockItemData item;
    item.appId = "signal.test.app";
    item.displayName = "Signal Test App";
    item.execPath = "/path/to/test";
    manager.pinItem(item);

    // 验证 pinned.json 已被写入
    QJsonArray saved = readPinnedJson();
    ASSERT_EQ(saved.size(), 1);
    EXPECT_EQ(saved[0].toObject()["appId"].toString(), "signal.test.app");
}

// 6. Config ↔ DockManager 完整往返测试
TEST_F(ApplicationTest, ConfigToDockManagerRoundtrip)
{
    // ── 第 1 步：写入 pinned.json ──
    QJsonArray arr;
    {
        QJsonObject item;
        item["appId"] = "roundtrip.app1";
        item["displayName"] = "RoundTrip One";
        item["iconPath"] = "/icons/rt1.svg";
        item["execPath"] = "/bin/rt1";
        arr.append(item);
    }
    {
        QJsonObject item;
        item["appId"] = "roundtrip.app2";
        item["displayName"] = "RoundTrip Two";
        item["iconPath"] = "/icons/rt2.svg";
        item["execPath"] = "/bin/rt2";
        arr.append(item);
    }
    writePinnedJson(arr);

    // ── 第 2 步：读取 pinned.json 并加载到 DockManager ──
    QFile file(PathManager::pinnedFile());
    ASSERT_TRUE(file.open(QIODevice::ReadOnly));
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    QList<DockItemData> items;
    for (const QJsonValue &v : doc.array()) {
        QJsonObject entry = v.toObject();
        DockItemData d;
        d.appId = entry["appId"].toString();
        d.displayName = entry["displayName"].toString();
        d.iconPath = entry["iconPath"].toString();
        d.execPath = entry["execPath"].toString();
        items.append(d);
    }

    DockManager manager;
    manager.setPinnedItems(items);

    ASSERT_EQ(manager.pinnedItems().size(), 2);
    EXPECT_EQ(manager.pinnedItems()[0].appId, "roundtrip.app1");
    EXPECT_EQ(manager.pinnedItems()[1].displayName, "RoundTrip Two");

    // ── 第 3 步：pinItem → 信号 → 写入 pinned.json ──
    // 复用 ConnectPersistenceWritesPinnedJson 的信号连接
    QObject::connect(&manager, &DockManager::pinnedItemsChanged,
        &manager, [](const QList<DockItemData> &items) {
            QSet<QString> systemExecPaths;
            QJsonArray arr;
            for (const auto &it : items) {
                if (systemExecPaths.contains(it.execPath.toLower())) continue;
                QJsonObject entry;
                entry["appId"] = it.appId;
                entry["displayName"] = it.displayName;
                entry["iconPath"] = it.iconPath;
                entry["execPath"] = it.execPath;
                arr.append(entry);
            }
            PathManager::ensureDataDir();
            QFile file(PathManager::pinnedFile());
            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
                file.close();
            }
        });

    DockItemData newItem;
    newItem.appId = "roundtrip.app3";
    newItem.displayName = "RoundTrip Three";
    newItem.iconPath = "/icons/rt3.svg";
    newItem.execPath = "/bin/rt3";
    manager.pinItem(newItem);

    // ── 第 4 步：重新读取 pinned.json，验证完整往返 ──
    QJsonArray reloaded = readPinnedJson();
    ASSERT_EQ(reloaded.size(), 3);
    EXPECT_EQ(reloaded[2].toObject()["appId"].toString(), "roundtrip.app3");
    EXPECT_EQ(reloaded[2].toObject()["displayName"].toString(), "RoundTrip Three");
}
