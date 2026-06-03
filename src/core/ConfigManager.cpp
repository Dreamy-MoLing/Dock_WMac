/**
 * @file ConfigManager.cpp
 * @brief JSON 配置读写与图标映射缓存实现
 *
 * 使用 QJsonDocument 直接读写 JSON 配置文件，
 * QCache 实现图标 LRU 缓存，自动管理内存。
 */

#include "core/ConfigManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_iconCache(kCacheLimit)
{
}

QString ConfigManager::configFilePath() const
{
    // Windows: %APPDATA%/Dock_WMac/config.json
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return configDir + "/config.json";
}

void ConfigManager::load()
{
    QFile file(configFilePath());
    if (!file.exists()) {
        // 创建默认配置
        m_config["position"] = QString("bottom");
        m_config["iconSize"] = 48;
        m_config["magnification"] = 1.5;
        m_config["autoHide"] = true;
        m_config["hideDelayMs"] = 500;
        m_config["monitor"] = 0;
        m_config["opacity"] = 0.95;
        m_config["blurEnabled"] = true;
        m_config["startWithSystem"] = false;
        m_config["corner_radius"] = 16;
        m_config["animation_duration"] = 300;
        m_config["show_delay"] = 0;
        save();
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (doc.isObject()) {
        m_config = doc.object();
    }
}

void ConfigManager::save()
{
    QDir().mkpath(QFileInfo(configFilePath()).absolutePath());

    QFile file(configFilePath());
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QJsonDocument doc(m_config);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

QVariant ConfigManager::get(const QString &key, const QVariant &defaultValue) const
{
    if (!m_config.contains(key)) {
        return defaultValue;
    }
    return m_config.value(key).toVariant();
}

void ConfigManager::set(const QString &key, const QVariant &value)
{
    m_config[key] = QJsonValue::fromVariant(value);
    save();
}

QString ConfigManager::resolveIcon(const QString &appId) const
{
    QJsonObject knownIcons = m_config["known_icons"].toObject();
    if (knownIcons.contains(appId)) {
        return knownIcons[appId].toString();
    }
    return {};
}

QPixmap ConfigManager::cachedIcon(const QString &appId) const
{
    QPixmap *pix = m_iconCache.object(appId);
    if (pix) return *pix;
    return {};
}

void ConfigManager::cacheIcon(const QString &appId, const QPixmap &pixmap)
{
    m_iconCache.insert(appId, new QPixmap(pixmap));
}
