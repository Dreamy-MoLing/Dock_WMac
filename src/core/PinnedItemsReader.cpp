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
        item.iconPath = execPath;  // 图标由 IconProvider 在渲染时统一提取
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

