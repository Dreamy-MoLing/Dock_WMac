#ifndef ICONPROVIDER_H
#define ICONPROVIDER_H

#include <QPixmap>
#include <QString>

/**
 * @file IconProvider.h
 * @brief 图标加载工具 — Win32 Shell API 实现
 *
 * 加载优先级：
 * 1. 绝对路径图片文件（PNG/ICO/SVG/JPG/BMP）
 * 2. UWP AppX → AUMID + SHGetFileInfoW + SHGFI_USEFILEATTRIBUTES
 * 3. .exe/.dll → ExtractIconEx（嵌入图标）
 * 4. .lnk → IShellLink 解析目标 → SHGetFileInfo
 * 5. SHGetImageList(SHIL_JUMBO) 高质量图标
 * 6. 字母占位符回退
 *
 * 返回结果统一归一化为 64×64 QPixmap。
 */

#ifdef Q_OS_WIN
#ifndef WINVER
#include <windows.h>
#endif
#endif

namespace IconProvider {

#ifdef Q_OS_WIN
/** @brief RAII HICON 包装 */
struct IconHandle {
    HICON handle = nullptr;
    ~IconHandle() { if (handle) DestroyIcon(handle); }
    operator HICON() const { return handle; }
};
#endif

/** @brief 从给定路径加载并归一化图标（6 级回退） */
QPixmap loadIcon(const QString &iconPath, const QString &displayName);

/** @brief 从 .exe/.dll 提取嵌入图标（使用 Win32 ExtractIconEx） */
QPixmap extractExeIcon(const QString &exePath);

/** @brief 从 UWP AppX 应用提取图标 */
QPixmap extractUwpIcon(const QString &exePath);

} // namespace IconProvider

#endif // ICONPROVIDER_H
