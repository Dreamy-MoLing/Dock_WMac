#ifndef PATHMANAGER_H
#define PATHMANAGER_H

#include <QString>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

/**
 * @file PathManager.h
 * @brief 便携模式路径管理 — 所有数据路径基于 .exe 所在目录
 *
 * 惰性创建：只读操作不创建 data/ 目录。
 * 仅在用户主动写入（pin/改设置）时才调用 ensureDataDir()。
 */

namespace PathManager {

inline QString dataDir()
{
    const QString overrideDir = qEnvironmentVariable("DOCK_WMAC_DATA_DIR");
    if (!overrideDir.isEmpty()) {
        return QDir::cleanPath(overrideDir);
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    const QString portableDir = appDir + QStringLiteral("/data");
    const QFileInfo portableInfo(portableDir);
    if ((portableInfo.exists() && portableInfo.isDir() && portableInfo.isWritable())
        || (!portableInfo.exists() && QFileInfo(appDir).isWritable())) {
        return portableDir;
    }

    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation)
        + QStringLiteral("/data");
}

inline QString configFile()
{
    return dataDir() + QStringLiteral("/config.json");
}

inline QString pinnedFile()
{
    return dataDir() + QStringLiteral("/pinned.json");
}

inline QString logFile()
{
    return dataDir() + QStringLiteral("/dock.log");
}

inline void ensureDataDir()
{
    QDir dir(dataDir());
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }
}

} // namespace PathManager

#endif // PATHMANAGER_H
