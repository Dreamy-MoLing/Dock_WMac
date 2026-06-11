/**
 * @file IconProvider.cpp
 * @brief 图标加载工具 — Jumbo 管道重写
 *
 * 5级回退：图片文件 → IShellItemImageFactory (UWP) → .lnk解析 → Jumbo → 占位符
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
#include <shobjidl.h>
#include <commoncontrols.h>
#endif

#pragma comment(lib, "shell32.lib")

// ─── UWP 检测 ────────────────────────────────────────────

static bool isUwpPath(const QString &exePath)
{
    return exePath.toLower().contains("\\windowsapps\\");
}

// ─── .lnk 解析（从 PinnedItemsReader 迁移）───────────────

static QString resolveShortcut(const QString &lnkPath)
{
#ifdef Q_OS_WIN
    if (!QFileInfo::exists(lnkPath)) return {};

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return {};

    IShellLink *psl = nullptr;
    hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                          IID_IShellLink, reinterpret_cast<void **>(&psl));
    if (FAILED(hr)) {
        if (hr != RPC_E_CHANGED_MODE) CoUninitialize();
        return {};
    }

    IPersistFile *ppf = nullptr;
    hr = psl->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&ppf));
    if (FAILED(hr)) {
        psl->Release();
        if (hr != RPC_E_CHANGED_MODE) CoUninitialize();
        return {};
    }

    WCHAR wszPath[MAX_PATH] = {};
    lnkPath.toWCharArray(wszPath);
    hr = ppf->Load(wszPath, STGM_READ);

    QString targetPath;
    if (SUCCEEDED(hr)) {
        WIN32_FIND_DATA wfd;
        WCHAR szPath[MAX_PATH] = {};
        hr = psl->GetPath(szPath, MAX_PATH, &wfd, SLGP_RAWPATH);
        if (SUCCEEDED(hr)) {
            targetPath = QString::fromWCharArray(szPath);
        }
    }

    ppf->Release();
    psl->Release();
    if (hr != RPC_E_CHANGED_MODE) CoUninitialize();
    return targetPath;
#else
    Q_UNUSED(lnkPath);
    return {};
#endif
}

// ─── 优先级 2: UWP/AppX → IShellItemImageFactory ─────────

static QPixmap extractViaShellImageFactory(const QString &exePath)
{
    QPixmap result;
#ifdef Q_OS_WIN
    QString nativePath = QDir::toNativeSeparators(exePath);
    IShellItemImageFactory *pFactory = nullptr;
    HRESULT hr = SHCreateItemFromParsingName(
        reinterpret_cast<LPCWSTR>(nativePath.utf16()),
        nullptr, IID_PPV_ARGS(&pFactory));

    if (SUCCEEDED(hr) && pFactory) {
        HBITMAP hBitmap = nullptr;
        SIZE size = {256, 256};
        hr = pFactory->GetImage(size, SIIGBF_ICONONLY, &hBitmap);
        if (SUCCEEDED(hr) && hBitmap) {
            QImage img = QImage::fromHBITMAP(hBitmap);
            if (!img.isNull()) {
                result = QPixmap::fromImage(img);
            }
            DeleteObject(hBitmap);
        }
        pFactory->Release();
    }
#else
    Q_UNUSED(exePath);
#endif
    return result;
}

// ─── 优先级 4: 黄金路径 — Jumbo 管道 ─────────────────────

static QPixmap extractJumboIcon(const QString &filePath)
{
    QPixmap result;
#ifdef Q_OS_WIN
    QString nativePath = QDir::toNativeSeparators(filePath);
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
                QImage img = QImage::fromHICON(hIcon);
                if (!img.isNull()) {
                    result = QPixmap::fromImage(img);
                }
                DestroyIcon(hIcon);
            }
            imageList->Release();
        }
    }
#else
    Q_UNUSED(filePath);
#endif
    return result;
}

// ─── 归一化（保持不变）───────────────────────────────────

static QPixmap normalizePixmap(const QPixmap &source)
{
    if (source.isNull()) return source;

    QPixmap result(64, 64);
    result.fill(Qt::transparent);

    QPixmap scaled;
    if (source.width() < 48 || source.height() < 48) {
        scaled = source.scaled(60, 60, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    } else {
        scaled = source.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }

    QPainter np(&result);
    np.drawPixmap((64 - scaled.width()) / 2, (64 - scaled.height()) / 2, scaled);
    np.end();
    return result;
}

// ─── 主入口（重排优先级）─────────────────────────────────

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

    // 优先级2: UWP AppX → IShellItemImageFactory (256×256)
    if (!iconPath.isEmpty() && isUwpPath(iconPath)) {
        pix = extractViaShellImageFactory(iconPath);
        if (!pix.isNull()) return normalizePixmap(pix);
    }

    // 优先级3: .lnk → 解析目标 → 进入优先级4
    QString resolvedPath = iconPath;
    if (!iconPath.isEmpty() && iconPath.toLower().endsWith(".lnk")) {
        QString target = resolveShortcut(iconPath);
        if (!target.isEmpty()) {
            resolvedPath = target;
        }
    }

    // 优先级4: 黄金路径 — Jumbo 管道（256×256 含 alpha）
    if (!resolvedPath.isEmpty() && QFileInfo::exists(resolvedPath)) {
        pix = extractJumboIcon(resolvedPath);
        if (!pix.isNull()) return normalizePixmap(pix);
    }

    // 优先级5: 字母占位符
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
