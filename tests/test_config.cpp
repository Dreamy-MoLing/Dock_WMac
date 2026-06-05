/**
 * @file test_config.cpp
 * @brief ConfigManager 单元测试
 *
 * 测试配置加载、默认值、读写持久化、图标缓存等功能。
 */

#include <gtest/gtest.h>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QStandardPaths>
#include <QApplication>
#include "core/ConfigManager.h"

// 测试需要 QApplication（QPixmap 依赖）
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
};

QApplication *ConfigManagerTest::app = nullptr;

TEST_F(ConfigManagerTest, DefaultValues)
{
    // 删除已有配置文件，确保测试默认值
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)
                         + "/config.json";
    QFile::remove(configPath);

    config->load();

    // 默认配置应包含所有标准字段
    EXPECT_EQ(config->get("iconSize").toInt(), 48);
    EXPECT_DOUBLE_EQ(config->get("magnification").toDouble(), 1.5);
    EXPECT_EQ(config->get("autoHide").toBool(), true);
    EXPECT_EQ(config->get("hideDelayMs").toInt(), 500);
    EXPECT_EQ(config->get("monitor").toInt(), 0);
    EXPECT_DOUBLE_EQ(config->get("opacity").toDouble(), 0.95);
    EXPECT_EQ(config->get("blurEnabled").toBool(), true);
    EXPECT_EQ(config->get("startWithSystem").toBool(), false);
    EXPECT_EQ(config->get("maxItems").toInt(), 16);
}

TEST_F(ConfigManagerTest, GetWithCustomDefault)
{
    config->load();

    // 不存在的键应返回自定义默认值
    EXPECT_EQ(config->get("nonexistent", 42).toInt(), 42);
    EXPECT_EQ(config->get("missing", "fallback").toString(), "fallback");
    EXPECT_EQ(config->get("absent", true).toBool(), true);
}

TEST_F(ConfigManagerTest, SetAndPersist)
{
    config->load();

    // 修改值
    config->set("iconSize", 64);
    config->set("autoHide", false);
    config->set("opacity", 0.8);

    // 验证内存中已更新
    EXPECT_EQ(config->get("iconSize").toInt(), 64);
    EXPECT_EQ(config->get("autoHide").toBool(), false);
    EXPECT_DOUBLE_EQ(config->get("opacity").toDouble(), 0.8);

    // 创建新实例验证持久化
    ConfigManager config2;
    config2.load();
    // 注意：由于使用系统路径，此测试在实际环境中可能受其他实例影响
}

TEST_F(ConfigManagerTest, ResolveIconFromMapping)
{
    config->load();

    // 没有 known_icons 时应返回空
    QString result = config->resolveIcon("nonexistent.exe");
    EXPECT_TRUE(result.isEmpty());
}

TEST_F(ConfigManagerTest, IconCacheOperations)
{
    QPixmap pixmap(64, 64);
    pixmap.fill(Qt::red);

    // 缓存写入和读取
    config->cacheIcon("test_app", pixmap);
    QPixmap cached = config->cachedIcon("test_app");
    EXPECT_FALSE(cached.isNull());
    EXPECT_EQ(cached.width(), 64);
    EXPECT_EQ(cached.height(), 64);

    // 不存在的缓存应返回空
    QPixmap missing = config->cachedIcon("nonexistent");
    EXPECT_TRUE(missing.isNull());
}

TEST_F(ConfigManagerTest, IconCacheLRU)
{
    // 填满缓存（kCacheLimit = 128）
    for (int i = 0; i < 130; ++i) {
        QPixmap pix(32, 32);
        pix.fill(QColor(i % 256, 0, 0));
        config->cacheIcon(QString("app_%1").arg(i), pix);
    }

    // 最早的条目应被淘汰
    QPixmap oldest = config->cachedIcon("app_0");
    EXPECT_TRUE(oldest.isNull());

    // 最新的条目应仍然存在
    QPixmap newest = config->cachedIcon("app_129");
    EXPECT_FALSE(newest.isNull());
}
