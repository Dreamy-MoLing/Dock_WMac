/**
 * @file IconProvider.cpp
 * @brief 图标加载工具 — Win32 Shell API 重写
 *
 * 6级回退：图片文件 → UWP → exe/dll → lnk → Jumbo → 占位符
 */
#include "core/IconProvider.h"

#include <QFileInfo>
#include <QPainter>
#include <QFont>
#include <QDir>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commoncontrols.h>
#endif

#pragma comment(lib, "shell32.lib")

// ─── UWP 检测 ────────────────────────────────────────────

static bool isUwpPath(const QString &exePath)
{
    return exePath.toLower().contains("\\windowsapps\\");
}

// ─── UWP 图标提取 ────────────────────────────────────────

QPixmap IconProvider::extractUwpIcon(const QString &exePath)
{
    QPixmap result;

#ifdef Q_OS_WIN
    if (!exePath.isEmpty() && isUwpPath(exePath)) {
        SHFILEINFOW sfi = {};
        QString path = QDir::toNativeSeparators(exePath);
        DWORD_PTR ret = SHGetFileInfoW(
            reinterpret_cast<LPCWSTR>(path.utf16()),
            FILE_ATTRIBUTE_NORMAL,
            &sfi, sizeof(sfi),
            SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES);

        if (ret && sfi.hIcon) {
            IconHandle guard{sfi.hIcon};
            result = QPixmap::fromImage(
                QImage::fromHICON(sfi.hIcon)).scaled(
                    64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
#else
    Q_UNUSED(exePath);
#endif

    return result;
}

// ─── exe/dll 嵌入图标 ────────────────────────────────────

QPixmap IconProvider::extractExeIcon(const QString &exePath)
{
    QPixmap result;

#ifdef Q_OS_WIN
    if (exePath.isEmpty() || !QFileInfo::exists(exePath)) return result;

    QString nativePath = QDir::toNativeSeparators(exePath);
    HICON hIconLarge = nullptr;

    // 提取大图标（索引 0）
    int extracted = ExtractIconExW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        0,
        &hIconLarge,
        nullptr,    // 小图标不需要
        1);

    if (extracted > 0 && hIconLarge) {
        IconHandle guard{hIconLarge};
        QImage img = QImage::fromHICON(hIconLarge);
        if (!img.isNull()) {
            result = QPixmap::fromImage(img).scaled(
                64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    // 回退：SHGetFileInfo
    if (result.isNull()) {
        SHFILEINFOW sfi = {};
        DWORD_PTR ret = SHGetFileInfoW(
            reinterpret_cast<LPCWSTR>(nativePath.utf16()),
            0, &sfi, sizeof(sfi),
            SHGFI_ICON | SHGFI_LARGEICON);

        if (ret && sfi.hIcon) {
            IconHandle guard{sfi.hIcon};
            QImage img = QImage::fromHICON(sfi.hIcon);
            if (!img.isNull()) {
                result = QPixmap::fromImage(img).scaled(
                    64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
            }
        }
    }
#else
    Q_UNUSED(exePath);
#endif

    return result;
}

// ─── Jumbo 高质量图标 ────────────────────────────────────

static QPixmap extractJumboIcon(const QString &exePath)
{
    QPixmap result;

#ifdef Q_OS_WIN
    QString nativePath = QDir::toNativeSeparators(exePath);
    SHFILEINFOW sfi = {};
    DWORD_PTR ret = SHGetFileInfoW(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        0, &sfi, sizeof(sfi),
        SHGFI_SYSICONINDEX);

    if (ret) {
        IImageList *imageList = nullptr;
        if (SUCCEEDED(SHGetImageList(SHIL_JUMBO, IID_IImageList,
                                      reinterpret_cast<void **>(&imageList)))) {
            HICON hIcon = nullptr;
            if (SUCCEEDED(imageList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon))) {
                IconProvider::IconHandle guard{hIcon};
                QImage img = QImage::fromHICON(hIcon);
                if (!img.isNull()) {
                    result = QPixmap::fromImage(img).scaled(
                        64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                }
            }
            imageList->Release();
        }
    }
#else
    Q_UNUSED(exePath);
#endif

    return result;
}

// ─── 归一化 ──────────────────────────────────────────────

static QPixmap normalizePixmap(const QPixmap &source)
{
    if (source.isNull()) return source;

    QPixmap result(64, 64);
    result.fill(Qt::transparent);

    QPixmap scaled;
    if (source.width() < 48 || source.height() < 48) {
        // 源图标太小 → 放大填满
        scaled = source.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        scaled = source.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QPainter np(&result);
    np.drawPixmap((64 - scaled.width()) / 2, (64 - scaled.height()) / 2, scaled);
    np.end();
    return result;
}

// ─── 主入口 ──────────────────────────────────────────────

QPixmap IconProvider::loadIcon(const QString &iconPath, const QString &displayName)
{
    QPixmap pix;

    // 优先级1: 绝对路径图片文件（PNG/ICO/SVG/JPG/BMP）
    if (!iconPath.isEmpty() && QFileInfo::exists(iconPath)) {
        QString lower = iconPath.toLower();
        bool isImage = lower.endsWith(".png") || lower.endsWith(".ico")
                    || lower.endsWith(".svg") || lower.endsWith(".jpg")
                    || lower.endsWith(".jpeg") || lower.endsWith(".bmp");
        if (isImage) {
            pix.load(iconPath);
            if (!pix.isNull()) return normalizePixmap(pix);
        }
    }

    // 优先级2: UWP AppX
    if (!iconPath.isEmpty() && isUwpPath(iconPath)) {
        pix = IconProvider::extractUwpIcon(iconPath);
        if (!pix.isNull()) return normalizePixmap(pix);
    }

    // 优先级3: exe/dll 嵌入图标（ExtractIconEx）
    if (!iconPath.isEmpty() && QFileInfo::exists(iconPath)) {
        QString lower = iconPath.toLower();
        if (lower.endsWith(".exe") || lower.endsWith(".dll")) {
            pix = IconProvider::extractExeIcon(iconPath);
            if (!pix.isNull()) return normalizePixmap(pix);
        }
    }

    // 优先级4: .lnk 解析目标 → SHGetFileInfo
    if (!iconPath.isEmpty() && QFileInfo::exists(iconPath)) {
        if (iconPath.toLower().endsWith(".lnk")) {
            pix = IconProvider::extractExeIcon(iconPath);
            if (!pix.isNull()) return normalizePixmap(pix);
        }
    }

    // 优先级5: Jumbo 高质量图标
    if (!iconPath.isEmpty() && QFileInfo::exists(iconPath)) {
        pix = extractJumboIcon(iconPath);
        if (!pix.isNull()) return normalizePixmap(pix);
    }

    // 优先级6: 字母占位符
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
        return placeholder;
    }
}
