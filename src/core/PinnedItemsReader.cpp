/**
 * @file PinnedItemsReader.cpp
 * @brief 任务栏固定项读取器实现
 */

#include "core/PinnedItemsReader.h"
#include "core/AppIdHelper.h"
#include "core/SysHelper.h"

#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QSet>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>
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

namespace {
struct ShortcutInfo {
    QString targetPath;
    QString arguments;
    QString appUserModelId;
    bool loaded = false;
};

#ifdef Q_OS_WIN
ShortcutInfo readShortcutInfo(const QString &lnkPath)
{
    ShortcutInfo info;
    if (!QFileInfo::exists(lnkPath)) return info;

    HRESULT initHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool didInitCom = SUCCEEDED(initHr);
    if (FAILED(initHr) && initHr != RPC_E_CHANGED_MODE) return info;

    IShellLinkW *link = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER,
                                  IID_IShellLinkW, reinterpret_cast<void **>(&link));
    if (FAILED(hr)) {
        if (didInitCom) CoUninitialize();
        return info;
    }

    IPersistFile *persist = nullptr;
    hr = link->QueryInterface(IID_IPersistFile, reinterpret_cast<void **>(&persist));
    if (SUCCEEDED(hr)) {
        const std::wstring nativePath = QDir::toNativeSeparators(lnkPath).toStdWString();
        hr = persist->Load(nativePath.c_str(), STGM_READ);
        info.loaded = SUCCEEDED(hr);
        persist->Release();
    }

    if (info.loaded) {
        WIN32_FIND_DATAW wfd = {};
        WCHAR pathBuf[MAX_PATH] = {};
        if (SUCCEEDED(link->GetPath(pathBuf, MAX_PATH, &wfd, SLGP_RAWPATH)))
            info.targetPath = QString::fromWCharArray(pathBuf);

        WCHAR argBuf[INFOTIPSIZE] = {};
        if (SUCCEEDED(link->GetArguments(argBuf, INFOTIPSIZE)))
            info.arguments = QString::fromWCharArray(argBuf);

        IPropertyStore *store = nullptr;
        if (SUCCEEDED(link->QueryInterface(IID_IPropertyStore, reinterpret_cast<void **>(&store)))) {
            PROPVARIANT value;
            PropVariantInit(&value);
            if (SUCCEEDED(store->GetValue(PKEY_AppUserModel_ID, &value)) && value.vt == VT_LPWSTR && value.pwszVal) {
                info.appUserModelId = QString::fromWCharArray(value.pwszVal);
            }
            PropVariantClear(&value);
            store->Release();
        }
    }

    link->Release();
    if (didInitCom) CoUninitialize();
    return info;
}
#else
ShortcutInfo readShortcutInfo(const QString &lnkPath)
{
    Q_UNUSED(lnkPath);
    return {};
}
#endif
} // namespace

QList<DockItemData> PinnedItemsReader::readFromLnkFiles()
{
    QList<DockItemData> items;

    QString taskbarPath = QDir::homePath()
        + "/AppData/Roaming/Microsoft/Internet Explorer/Quick Launch/User Pinned/TaskBar/";
    QDir dir(taskbarPath);

    if (!dir.exists()) {
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
        const QString lnkPath = fi.absoluteFilePath();
        const ShortcutInfo shortcut = readShortcutInfo(lnkPath);

        DockItemData item;
        item.appUserModelId = shortcut.appUserModelId;
        item.appId = shortcut.appUserModelId.isEmpty() ? fi.completeBaseName() : shortcut.appUserModelId;
        item.displayName = fi.completeBaseName();
        item.execPath = lnkPath;
        item.targetPath = shortcut.targetPath;
        item.arguments = shortcut.arguments;
        item.iconPath = shortcut.targetPath.isEmpty() ? lnkPath : shortcut.targetPath;
        item.isRunning = false;
        item.badgeCount = 0;
        items.append(item);

        qDebug() << "固定项:" << item.appId
                 << "launch:" << item.execPath
                 << "target:" << item.targetPath
                 << "aumid:" << item.appUserModelId;
    }

    return items;
}

QList<DockItemData> PinnedItemsReader::deduplicateItems(const QList<DockItemData> &items)
{
    QSet<QString> seen;
    QList<DockItemData> deduplicated;

    for (const auto &item : items) {
        QStringList keys = AppIdHelper::identityKeys(item);
        if (keys.isEmpty())
            keys.append(item.execPath.toLower());

        bool duplicate = false;
        for (const QString &key : keys) {
            if (seen.contains(key)) {
                duplicate = true;
                break;
            }
        }

        if (duplicate) {
            qDebug() << "去重: 跳过重复项" << item.appId << "(" << item.execPath << ")";
            continue;
        }

        for (const QString &key : keys)
            seen.insert(key);
        deduplicated.append(item);
    }

    return deduplicated;
}

QString PinnedItemsReader::resolveShortcut(const QString &lnkPath)
{
    return SysHelper::resolveShortcut(lnkPath);
}