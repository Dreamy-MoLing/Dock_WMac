#ifndef APPIDHELPER_H
#define APPIDHELPER_H

#include <QFileInfo>
#include <QSet>
#include <QString>
#include <QStringList>
#include "Types.h"

/**
 * @file AppIdHelper.h
 * @brief appId / execPath -> process identity helpers
 */

namespace AppIdHelper {

inline QString execBaseName(const QString &execPath)
{
    if (execPath.isEmpty()) return {};
    return QFileInfo(execPath).baseName();
}

inline QString normalizedKey(const QString &value)
{
    QString key = value.trimmed().toLower();
    if (key.endsWith(QStringLiteral(".exe")))
        key.chop(4);
    return key;
}

inline QString appIdToProcName(const QString &appId)
{
    if (appId.isEmpty()) return {};
    int dotIdx = appId.lastIndexOf(QLatin1Char('.'));
    return (dotIdx >= 0) ? appId.mid(dotIdx + 1) : appId;
}

inline QString deriveWmClass(const QString &execPath, const QString &appId)
{
    if (!execPath.isEmpty()) {
        QString base = execBaseName(execPath);
        if (!base.isEmpty())
            return base.toLower();
    }
    return appIdToProcName(appId).toLower();
}

inline void appendUniqueKey(QStringList &keys, QSet<QString> &seen, const QString &value)
{
    const QString key = normalizedKey(value);
    if (key.isEmpty() || seen.contains(key)) return;
    seen.insert(key);
    keys.append(key);
}

inline QStringList identityKeys(const QString &launchPath,
                                const QString &targetPath,
                                const QString &appId,
                                const QString &appUserModelId = {})
{
    QStringList keys;
    QSet<QString> seen;

    appendUniqueKey(keys, seen, execBaseName(targetPath));
    appendUniqueKey(keys, seen, execBaseName(launchPath));
    appendUniqueKey(keys, seen, appUserModelId);
    appendUniqueKey(keys, seen, appId);
    appendUniqueKey(keys, seen, appIdToProcName(appUserModelId));
    appendUniqueKey(keys, seen, appIdToProcName(appId));

    return keys;
}

inline QStringList identityKeys(const DockItemData &item)
{
    return identityKeys(item.execPath, item.targetPath, item.appId, item.appUserModelId);
}

inline QString primaryIdentityKey(const DockItemData &item)
{
    const QStringList keys = identityKeys(item);
    return keys.isEmpty() ? QString() : keys.first();
}

} // namespace AppIdHelper

#endif // APPIDHELPER_H