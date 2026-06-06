/**
 * @file IconProvider.cpp
 * @brief 图标加载工具实现
 *
 * 4级回退加载，归一化到 64×64。
 */
#include "core/IconProvider.h"

#include <QFileInfo>
#include <QFileIconProvider>
#include <QIcon>
#include <QPainter>
#include <QFont>

QPixmap IconProvider::loadIcon(const QString &iconPath, const QString &displayName)
{
    QPixmap pix;

    // 优先级1: 绝对路径图片文件
    if (!iconPath.isEmpty() && QFileInfo::exists(iconPath)) {
        pix.load(iconPath);
        if (!pix.isNull()) goto normalize;
    }

    // 优先级2: exe/dll/lnk 嵌入图标
    if (!iconPath.isEmpty() && QFileInfo::exists(iconPath)) {
        QString lower = iconPath.toLower();
        if (lower.endsWith(".exe") || lower.endsWith(".dll") || lower.endsWith(".lnk")) {
            QFileIconProvider provider;
            QIcon fileIcon = provider.icon(QFileInfo(iconPath));
            if (!fileIcon.isNull()) {
                pix = fileIcon.pixmap(64, 64);
                if (!pix.isNull()) goto normalize;
            }
        }
    }

    // 优先级3: QIcon::fromTheme 系统图标
    if (!iconPath.isEmpty()) {
        QIcon themedIcon = QIcon::fromTheme(iconPath);
        if (!themedIcon.isNull()) {
            pix = themedIcon.pixmap(64, 64);
            if (!pix.isNull()) goto normalize;
        }
    }

    // 优先级4: 字母占位符
    {
        QPixmap placeholder(64, 64);
        placeholder.fill(QColor(80, 80, 80));
        QPainter p(&placeholder);
        p.setPen(Qt::white);
        QFont font = p.font();
        font.setPixelSize(32);
        font.setBold(true);
        p.setFont(font);
        p.drawText(placeholder.rect(), Qt::AlignCenter,
                   displayName.isEmpty() ? "?" : displayName.left(1).toUpper());
        p.end();
        return placeholder;  // 已 64×64，无需 normalize
    }

normalize:
    // 统一归一化到 64×64
    {
        QPixmap result(64, 64);
        result.fill(Qt::transparent);
        QPixmap scaled = pix.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        QPainter np(&result);
        np.drawPixmap((64 - scaled.width()) / 2, (64 - scaled.height()) / 2, scaled);
        np.end();
        return result;
    }
}
