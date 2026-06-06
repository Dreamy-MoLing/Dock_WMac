#ifndef ICONPROVIDER_H
#define ICONPROVIDER_H

#include <QPixmap>
#include <QString>

/**
 * @file IconProvider.h
 * @brief 图标加载工具
 *
 * 加载优先级：
 * 1. 绝对路径图片文件（PNG/ICO/SVG）
 * 2. 可执行文件的嵌入图标（.exe/.dll/.lnk，通过 QFileIconProvider）
 * 3. QIcon::fromTheme 系统图标
 * 4. 生成字母占位符（取 displayName 首字母）
 *
 * 返回结果统一归一化为 64×64 QPixmap。
 */

namespace IconProvider {

/** @brief 从给定路径加载并归一化图标
 *  @param iconPath    图标路径（文件路径、exe/dll 路径、或主题名）
 *  @param displayName 显示名称（用于生成占位符）
 *  @return 64×64 QPixmap
 */
QPixmap loadIcon(const QString &iconPath, const QString &displayName);

} // namespace IconProvider

#endif // ICONPROVIDER_H
