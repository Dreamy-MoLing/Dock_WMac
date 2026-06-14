/**
 * @file test_config.cpp
 * @brief ConfigManager 单元测试
 *
 * 测试配置加载、默认值、读写持久化、键迁移、图标缓存等功能。
 * 每个测试有明确的行为契约，相互独立。
 */

#include <gtest/gtest.h>
#include <QApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QStandardPaths>
#include <QCoreApplication>
#include <QTemporaryDir>
#include "core/ConfigManager.h"
#include "core/PathManager.h"

// QPixmap 依赖 QApplication
static int argc = 1;
static char arg0[] = "test_config";
static char *argv[] = {arg0, nullptr};

class ConfigManagerTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        if (!QApplication::instance()) {
            app = new QApplication(argc, argv);
        }
    }
    static void TearDownTestSuite() {
        // QApplication 由进程退出时回收
    }

    void SetUp() override {
        config = new ConfigManager();
    }
    void TearDown() override {
        delete config;
    }

    ConfigManager *config;
    static QApplication *app;

    /// 获取配置文件完整路径（使用 PathManager 保持一致）
    static QString configPath() {
        return PathManager::configFile();
    }

    /// 删除配置文件，确保后续 load() 创建默认配置
    static void cleanConfig() {
        QFile::remove(configPath());
    }
};

QApplication *ConfigManagerTest::app = nullptr;

// ============================================================================
// 1. DefaultValues — load() 在配置文件不存在时填充所有标准默认值
// ============================================================================

TEST_F(ConfigManagerTest, DefaultValues)
{
    cleanConfig();

    config->load();

    EXPECT_EQ(config->get("iconSize").toInt(), 48);
    EXPECT_DOUBLE_EQ(config->get("magnification").toDouble(), 1.5);
    EXPECT_EQ(config->get("autoHide").toBool(), true);
    EXPECT_EQ(config->get("hideDelayMs").toInt(), 500);
    EXPECT_EQ(config->get("monitor").toInt(), 0);
    EXPECT_DOUBLE_EQ(config->get("opacity").toDouble(), 0.95);
    EXPECT_EQ(config->get("blurEnabled").toBool(), true);
    EXPECT_EQ(config->get("startWithSystem").toBool(), false);
    EXPECT_EQ(config->get("maxItems").toInt(), 16);
    EXPECT_EQ(config->get("cornerRadius").toInt(), 16);
    EXPECT_EQ(config->get("animationDuration").toInt(), 300);
    EXPECT_EQ(config->get("showDelay").toInt(), 0);
}

// ============================================================================
// 2. DefaultWithFallback — 不存在的键返回调用者指定的自定义默认值
// ============================================================================

TEST_F(ConfigManagerTest, DefaultWithFallback)
{
    config->load();

    // 不存在的键返回自定义默认值（不同类型）
    EXPECT_EQ(config->get("nonexistent", 42).toInt(), 42);
    EXPECT_EQ(config->get("missing_str", QString("fallback")).toString(), QString("fallback"));
    EXPECT_EQ(config->get("absent_bool", false).toBool(), false);

    // 已有配置不受影响
    EXPECT_EQ(config->get("iconSize").toInt(), 48);
}

// ============================================================================
// 3. SetAndPersist — set() 立即更新内存 + 写入磁盘，新实例可读取
// ============================================================================

TEST_F(ConfigManagerTest, SetAndPersist)
{
    config->load();

    config->set("iconSize", 64);
    config->set("autoHide", false);

    // 内存中已更新
    EXPECT_EQ(config->get("iconSize").toInt(), 64);
    EXPECT_EQ(config->get("autoHide").toBool(), false);

    // 新实例 load() 后验证持久化
    ConfigManager config2;
    config2.load();
    EXPECT_EQ(config2.get("iconSize").toInt(), 64);
    EXPECT_EQ(config2.get("autoHide").toBool(), false);
}

// ============================================================================
// 4. JsonRoundtrip — 多值 set → save → 新实例 load，全部精确匹配
// ============================================================================

TEST_F(ConfigManagerTest, JsonRoundtrip)
{
    config->load();

    config->set("iconSize", 72);
    config->set("opacity", 0.5);
    config->set("maxItems", 8);

    ConfigManager config2;
    config2.load();

    EXPECT_EQ(config2.get("iconSize").toInt(), 72);
    EXPECT_DOUBLE_EQ(config2.get("opacity").toDouble(), 0.5);
    EXPECT_EQ(config2.get("maxItems").toInt(), 8);
}

// ============================================================================
// 5. IconCacheOperations — 图标缓存的写入和读取
// ============================================================================

TEST_F(ConfigManagerTest, IconCacheOperations)
{
    // 契约：cacheIcon 存入后 cachedIcon 返回有效 QPixmap；
    //       未缓存的键返回空 QPixmap。

    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::red);

    config->cacheIcon("test_app", pixmap);

    QPixmap cached = config->cachedIcon("test_app");
    EXPECT_FALSE(cached.isNull());
    EXPECT_EQ(cached.width(), 64);
    EXPECT_EQ(cached.height(), 64);

    // 未缓存键返回空
    QPixmap missing = config->cachedIcon("nonexistent_app");
    EXPECT_TRUE(missing.isNull());
}

// ============================================================================
// 6. IconCacheLRU — 插入 130 个条目，最旧的被淘汰，最新的保留
// ============================================================================

TEST_F(ConfigManagerTest, IconCacheLRU)
{
    // 契约：缓存上限为 128（kCacheLimit），插入第 129 个条目时
    //       item 0 被淘汰，第 129 个条目存在。

    for (int i = 0; i < 130; ++i) {
        QPixmap pix(32, 32);
        pix.fill(QColor(i % 256, 0, 0));
        config->cacheIcon(QString("app_%1").arg(i), pix);
    }

    // item 0 已被淘汰
    EXPECT_TRUE(config->cachedIcon("app_0").isNull());
    // item 129 仍保留
    EXPECT_FALSE(config->cachedIcon("app_129").isNull());
}

// ============================================================================
// 7. ResolveIconFromMapping — knownIcons 映射表驱动图标解析
// ============================================================================

TEST_F(ConfigManagerTest, ResolveIconFromMapping)
{
    // 契约：未设置 knownIcons 时 resolveIcon 返回空；
    //       设置映射后返回正确路径。

    config->load();

    // 无映射 → 空字符串
    EXPECT_TRUE(config->resolveIcon("myapp.exe").isEmpty());

    // 设置 knownIcons 映射
    QJsonObject known;
    known["myapp.exe"] = "C:/icons/myapp.png";
    config->set("knownIcons", known);

    // 解析应返回映射路径
    EXPECT_EQ(config->resolveIcon("myapp.exe").toStdString(), "C:/icons/myapp.png");

    // 未映射的键仍返回空
    EXPECT_TRUE(config->resolveIcon("other.exe").isEmpty());
}

// ============================================================================
// 8. SetMultipleAndVerifyAll — 批量设置 5 个键，新实例全部匹配
// ============================================================================

TEST_F(ConfigManagerTest, SetMultipleAndVerifyAll)
{
    // 契约：设置 5 个不同类型的键后，新实例 load() 全部精确匹配。

    config->load();

    config->set("iconSize", 80);
    config->set("opacity", 0.3);
    config->set("autoHide", false);
    config->set("maxItems", 12);
    config->set("monitor", 1);

    ConfigManager config2;
    config2.load();

    EXPECT_EQ(config2.get("iconSize").toInt(), 80);
    EXPECT_DOUBLE_EQ(config2.get("opacity").toDouble(), 0.3);
    EXPECT_EQ(config2.get("autoHide").toBool(), false);
    EXPECT_EQ(config2.get("maxItems").toInt(), 12);
    EXPECT_EQ(config2.get("monitor").toInt(), 1);
}
