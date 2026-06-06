/**
 * @file PinnedItemsReader.cpp
 * @brief 任务栏固定项读取器实现
 *
 * 从 Windows 任务栏 .lnk 文件目录读取固定项，
 * 解析快捷方式目标路径并提取应用图标。
 */

#include "core/PinnedItemsReader.h"

#include <QDir>
#include <QFileInfo>
#include <QPixmap>
#include <QImage>
#include <QSet>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

PinnedItemsReader::PinnedItemsReader(QObject *parent)
    : QObject(parent)
{
}

QList<DockItemData> PinnedItemsReader::getAllPinnedItems()
{
    QList<DockItemData> items = readFromLnkFiles();
    QList<DockItemData> deduplicated = deduplicateItems(items);
    qInfo() << "固定项读取完成，共" << deduplicated.size() << "个";
    return deduplicated;
}

QList<DockItemData> PinnedItemsReader::readFromLnkFiles()
{
    QList<DockItemData> items;

    // Windows 10/11 任务栏固定项路径
    QString taskbarPath = QDir::homePath()
        + "/AppData/Roaming/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar/";
    QDir dir(taskbarPath);

    if (!dir.exists()) {
        // 回退: Windows 7 风格路径
        taskbarPath = QDir::homePath()
            + "/AppData/Roaming/Microsoft/Internet Explorer/Quick Launch/";
        dir.setPath(taskbarPath);
    }

    if (!dir.exists()) {
        qWarning() << "任务栏固定项目录不存在:" << taskbarPath;
        return items;
    }

    const auto entries = dir.entryInfoList({"*.lnk"}, QDir::Files, QDir::Name);
    for (const QFileInfo &fi : entries) {
        QString execPath = resolveShortcut(fi.absoluteFilePath());
        if (execPath.isEmpty()) {
            qWarning() << "无法解析快捷方式:" << fi.absoluteFilePath();
            continue;
        }

        // 过滤无效快捷方式（非 .exe 文件）
        if (!execPath.toLower().endsWith(".exe")) {
            qDebug() << "跳过非应用程序快捷方式:" << fi.absoluteFilePath();
            continue;
        }

        DockItemData item;
        item.appId = fi.completeBaseName();
        item.displayName = fi.completeBaseName();
        item.execPath = execPath;
        // 优先使用提取的图标缓存，如果失败则使用 exe 路径（由 DockItem 提取）
        QString cachedIcon = extractAppIcon(execPath);
        item.iconPath = cachedIcon.isEmpty() ? execPath : cachedIcon;
        item.isRunning = false;
        item.badgeCount = 0;
        items.append(item);
        qDebug() << "固定项:" << item.appId << "execPath:" << execPath << "iconPath:" << item.iconPath;
    }

    return items;
}

QList<DockItemData> PinnedItemsReader::deduplicateItems(const QList<DockItemData> &items)
{
    QSet<QString> seenPaths;
    QList<DockItemData> deduplicated;

    for (const auto &item : items) {
        QString key = item.execPath.toLower();
        if (!seenPaths.contains(key)) {
            seenPaths.insert(key);
            deduplicated.append(item);
        } else {
            qDebug() << "去重: 跳过重复项" << item.appId << "(" << item.execPath << ")";
        }
    }

    return deduplicated;
}

QString PinnedItemsReader::resolveShortcut(const QString &lnkPath)
{
    if (!QFileInfo::exists(lnkPath)) return {};

#ifdef Q_OS_WIN
    // 使用 IShellLink COM 接口解析 .lnk
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

    // 加载 .lnk 文件
    WCHAR wszPath[MAX_PATH];
    lnkPath.toWCharArray(wszPath);
    wszPath[lnkPath.length()] = 0;
    hr = ppf->Load(wszPath, STGM_READ);

    QString execPath;
    if (SUCCEEDED(hr)) {
        WIN32_FIND_DATA wfd;
        WCHAR szPath[MAX_PATH];
        hr = psl->GetPath(szPath, MAX_PATH, &wfd, SLGP_RAWPATH);
        if (SUCCEEDED(hr)) {
            execPath = QString::fromWCharArray(szPath);
        }
    }

    ppf->Release();
    psl->Release();
    if (hr != RPC_E_CHANGED_MODE) CoUninitialize();
    return execPath;
#else
    Q_UNUSED(lnkPath);
    return {};
#endif
}

QString PinnedItemsReader::extractAppIcon(const QString &appId)
{
#ifdef Q_OS_WIN
    // 检查缓存是否已存在
    QString cacheDir = QDir::tempPath() + "/dock_wmac_icons";
    QString cachePath = cacheDir + "/" + QFileInfo(appId).baseName() + ".png";
    if (QFileInfo::exists(cachePath)) {
        return cachePath;
    }

    // 使用 SHGetFileInfo 提取大图标
    SHFILEINFO shfi;
    ZeroMemory(&shfi, sizeof(shfi));
    DWORD_PTR result = SHGetFileInfo(
        reinterpret_cast<const wchar_t *>(appId.utf16()),
        FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi),
        SHGFI_ICON | SHGFI_LARGEICON | SHGFI_USEFILEATTRIBUTES);

    if (!result || !shfi.hIcon) {
        // 回退：直接尝试文件图标
        result = SHGetFileInfo(
            reinterpret_cast<const wchar_t *>(appId.utf16()),
            FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi),
            SHGFI_ICON | SHGFI_LARGEICON);
        if (!result || !shfi.hIcon) return {};
    }

    // HICON → QPixmap（使用 Qt 的 QGuiApplication::instance() 检查）
    QPixmap pix;
    if (shfi.hIcon) {
        // 使用 Windows API 获取图标尺寸
        ICONINFO iconInfo;
        if (GetIconInfo(shfi.hIcon, &iconInfo)) {
            BITMAP bm;
            if (iconInfo.hbmColor && GetObject(iconInfo.hbmColor, sizeof(bm), &bm)) {
                int w = bm.bmWidth;
                int h = bm.bmHeight;
                if (w > 0 && h > 0) {
                    // 创建设备上下文和位图
                    HDC hdc = GetDC(nullptr);
                    HDC memDC = CreateCompatibleDC(hdc);
                    HBITMAP hBitmap = CreateCompatibleBitmap(hdc, w, h);
                    HBITMAP hOld = (HBITMAP)SelectObject(memDC, hBitmap);

                    // 绘制图标到设备上下文
                    DrawIconEx(memDC, 0, 0, shfi.hIcon, w, h, 0, nullptr, DI_NORMAL);

                    // 转换为 QImage
                    QImage img(w, h, QImage::Format_ARGB32);
                    BITMAPINFO bmi;
                    memset(&bmi, 0, sizeof(bmi));
                    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                    bmi.bmiHeader.biWidth = w;
                    bmi.bmiHeader.biHeight = -h; // 顶部开始
                    bmi.bmiHeader.biPlanes = 1;
                    bmi.bmiHeader.biBitCount = 32;
                    bmi.bmiHeader.biCompression = BI_RGB;
                    GetDIBits(hdc, hBitmap, 0, h, img.bits(), &bmi, DIB_RGB_COLORS);

                    pix = QPixmap::fromImage(img);

                    // 清理
                    SelectObject(memDC, hOld);
                    DeleteObject(hBitmap);
                    DeleteDC(memDC);
                    ReleaseDC(nullptr, hdc);
                }
            }
            if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
            if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
        }
        DestroyIcon(shfi.hIcon);
    }

    if (pix.isNull()) return {};

    QDir().mkpath(cacheDir);
    pix.save(cachePath, "PNG");
    return cachePath;
#else
    Q_UNUSED(appId);
    return {};
#endif
}
