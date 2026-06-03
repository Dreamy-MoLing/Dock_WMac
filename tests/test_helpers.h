/**
 * @file test_helpers.h
 * @brief 测试辅助工具函数
 *
 * 提供测试中常用的辅助函数，如进程名称规范化等。
 */

#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <QString>

/**
 * @brief 将 appId 转换为进程名（模拟 ProcessMonitor::appIdToProcessName）
 *
 * 规则：
 * - 取最后一个 '.' 之后的部分
 * - 转换为小写
 *
 * 示例：
 * - "org.gnome.Nautilus" → "nautilus"
 * - "chrome" → "chrome"
 * - "com.example.MyApp" → "myapp"
 */
inline QString normalizeProcessName(const QString &appId)
{
    QString name = appId;
    int dotIdx = name.lastIndexOf('.');
    if (dotIdx >= 0) {
        name = name.mid(dotIdx + 1);
    }
    return name.toLower();
}

/**
 * @brief 比较两个进程名是否匹配（忽略大小写和路径）
 *
 * 规则：
 * - 移除路径部分（取最后一个 '/' 或 '\' 之后的部分）
 * - 移除 .exe 后缀
 * - 转换为小写比较
 *
 * 示例：
 * - "C:/Program Files/Chrome/chrome.exe" → "chrome"
 * - "chrome.exe" → "chrome"
 * - "CHROME" → "chrome"
 */
inline QString extractBaseName(const QString &processPath)
{
    QString name = processPath;

    // 移除路径部分
    int lastSlash = name.lastIndexOf('/');
    int lastBackslash = name.lastIndexOf('\\');
    int separator = qMax(lastSlash, lastBackslash);
    if (separator >= 0) {
        name = name.mid(separator + 1);
    }

    // 移除 .exe 后缀
    if (name.toLower().endsWith(".exe")) {
        name.chop(4);
    }

    return name.toLower();
}

/**
 * @brief 比较两个进程路径是否指向同一个进程
 */
inline bool isSameProcess(const QString &path1, const QString &path2)
{
    return extractBaseName(path1) == extractBaseName(path2);
}

#endif // TEST_HELPERS_H
