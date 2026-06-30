#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QCache>
#include <QPixmap>

/**
 * @file ConfigManager.h
 * @brief JSON 配置读写与图标映射缓存
 *
 * 管理配置文件（JSON 格式）的读写、图标映射表的维护与 LRU 缓存。
 * 直接使用 QFile + QJsonDocument 读写便携模式 data/config.json。
 */

class ConfigManager : public QObject {
    Q_OBJECT
public:
    explicit ConfigManager(QObject *parent = nullptr);
    ~ConfigManager() override;

    /** @brief 加载配置文件，不存在则创建默认配置 */
    void load();

    /** @brief 保存当前配置到文件 */
    void save();

    /** @brief 兼容旧调用；当前 set() 已同步保存，无待刷写队列 */
    void flush();

    /** @brief 读取指定键的配置值 */
    QVariant get(const QString &key, const QVariant &defaultValue = QVariant()) const;

    /** @brief 设置配置值并同步保存；值未变化时不写盘 */
    void set(const QString &key, const QVariant &value);

    /** @brief 获取应用对应的高清图标路径（优先映射表，其次缓存） */
    QString resolveIcon(const QString &appId) const;

    /** @brief 从缓存中获取图标的像素图 */
    QPixmap cachedIcon(const QString &appId) const;

    /** @brief 将图标放入缓存 */
    void cacheIcon(const QString &appId, const QPixmap &pixmap);

    /** @brief 获取配置文件完整路径（公开用于测试诊断） */
    QString configFilePath() const;

    /** @brief 获取配置文件目录路径（用于卸载清理）
     *  便携模式: ./data/ */
    static QString configDir();

private:
    QJsonObject m_config;
    QCache<QString, QPixmap> m_iconCache;
    static const int kCacheLimit = 128;
};

#endif // CONFIGMANAGER_H
