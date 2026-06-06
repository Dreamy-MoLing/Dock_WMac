#ifndef APPIDHELPER_H
#define APPIDHELPER_H

#include <QString>
#include <QFileInfo>

/**
 * @file AppIdHelper.h
 * @brief appId / execPath → 进程名 / WM_CLASS 推导工具
 *
 * 统一所有需要从 appId 或 execPath 推导进程名的逻辑。
 * 规则：优先使用 execPath 的 basename（去扩展名），
 * 其次取 appId 最后一段（处理反向 DNS 格式如 com.google.Chrome）。
 */

namespace AppIdHelper {

/** @brief 从完整路径提取可执行文件 basename（去 .exe） */
inline QString execBaseName(const QString &execPath)
{
    if (execPath.isEmpty()) return {};
    return QFileInfo(execPath).baseName(); // QFileInfo::baseName 去扩展名
}

/** @brief 从 appId 推导进程名（取最后一段，处理反向 DNS）
 *  "com.google.Chrome" → "Chrome"
 *  "Visual Studio Code" → "Visual Studio Code"
 */
inline QString appIdToProcName(const QString &appId)
{
    if (appId.isEmpty()) return {};
    int dotIdx = appId.lastIndexOf(QLatin1Char('.'));
    return (dotIdx >= 0) ? appId.mid(dotIdx + 1) : appId;
}

/** @brief 统一推导 WM_CLASS（优先 execPath basename，其次 appId 最后一段）
 *  @return 小写的进程名，用于 Win32 窗口匹配
 */
inline QString deriveWmClass(const QString &execPath, const QString &appId)
{
    // 优先：可执行文件名 basename（最精确）
    if (!execPath.isEmpty()) {
        QString base = execBaseName(execPath);
        if (!base.isEmpty())
            return base.toLower();
    }
    // 回退：appId 最后一段
    return appIdToProcName(appId).toLower();
}

} // namespace AppIdHelper

#endif // APPIDHELPER_H
