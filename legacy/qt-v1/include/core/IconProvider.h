#ifndef ICONPROVIDER_H
#define ICONPROVIDER_H

#include <QPixmap>
#include <QString>

/**
 * @file IconProvider.h
 * @brief 图标加载工具 — Jumbo 管道为主力
 *
 * 加载优先级：
 * 1. 绝对路径图片文件（PNG/ICO/SVG/JPG/BMP）
 * 2. UWP AppX → IShellItemImageFactory::GetImage (256×256)
 * 3. .lnk → IShellLink 解析目标 → 进入 4
 * 4. 黄金路径：SHGetFileInfo(SYSICONINDEX) → SHGetImageList(SHIL_JUMBO)
 *    → IImageList::GetIcon(ILD_TRANSPARENT)（256×256 含 alpha）
 * 5. 字母占位符回退
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

/** @brief 从给定路径加载并归一化图标（5 级回退） */
QPixmap loadIcon(const QString &iconPath, const QString &displayName);

} // namespace IconProvider

#endif // ICONPROVIDER_H
