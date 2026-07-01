/**
 * @file WindowCache.cpp
 * @brief 窗口枚举缓存层实现
 *
 * 全量 EnumWindows → 按 wmClass 分组缓存。
 * WinEvent 回调通过 refreshForPid 增量更新。
 * 点击前通过 scanForClass 按需同步扫描。
 */
#include "core/WindowCache.h"
#include "core/AppIdHelper.h"
#include "core/SysHelper.h"

#include <algorithm>
#include <QFileInfo>
#include <QDebug>
#include <QReadLocker>
#include <QWriteLocker>
#include <shobjidl.h>
#include <propkey.h>
#include <propvarutil.h>

#pragma comment(lib, "user32.lib")

static QString windowAppUserModelId(HWND hwnd)
{
    IPropertyStore *store = nullptr;
    HRESULT hr = SHGetPropertyStoreForWindow(hwnd, IID_PPV_ARGS(&store));
    if (FAILED(hr) || !store) return {};

    PROPVARIANT value;
    PropVariantInit(&value);
    QString appId;
    if (SUCCEEDED(store->GetValue(PKEY_AppUserModel_ID, &value)) && value.vt == VT_LPWSTR && value.pwszVal)
        appId = QString::fromWCharArray(value.pwszVal).toLower();
    PropVariantClear(&value);
    store->Release();
    return appId;
}
// ─── 构造 ────────────────────────────────────────────────

WindowCache::WindowCache(QObject *parent)
    : QObject(parent)
{
}

// ─── 全量刷新 ────────────────────────────────────────────

void WindowCache::refresh()
{
    // 1. 先收集所有窗口
    struct RawWin { HWND hwnd; DWORD pid; wchar_t title[256]; };
    QList<RawWin> rawWindows;

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto *list = reinterpret_cast<QList<RawWin> *>(lParam);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == 0 || pid == 4) return TRUE; // 跳过 System/Idle

        RawWin rw;
        rw.hwnd = hwnd;
        rw.pid = pid;
        GetWindowTextW(hwnd, rw.title, 255);
        list->append(rw);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&rawWindows));

    // 2. 对每个窗口构建 CachedWindowInfo
    QWriteLocker locker(&m_lock);
    m_windowsByClass.clear();
    m_pidToExeName.clear();

    HWND foregroundHwnd = GetForegroundWindow();

    for (const auto &rw : rawWindows) {
        // 获取进程名
        QString exeName;
        if (m_pidToExeName.contains(rw.pid)) {
            exeName = m_pidToExeName[rw.pid];
        } else {
            HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, rw.pid);
            if (hProcess) {
                wchar_t pathBuf[MAX_PATH] = {0};
                DWORD bufSize = MAX_PATH;
                if (QueryFullProcessImageNameW(hProcess, 0, pathBuf, &bufSize)) {
                    exeName = QFileInfo(QString::fromWCharArray(pathBuf)).baseName().toLower();
                }
                CloseHandle(hProcess);
            }
            if (!exeName.isEmpty()) {
                m_pidToExeName[rw.pid] = exeName;
            }
        }
        if (exeName.isEmpty()) continue;

        CachedWindowInfo info;
        info.hwnd = rw.hwnd;
        info.title = QString::fromWCharArray(rw.title);
        info.pid = rw.pid;
        info.exeName = exeName;
        info.appUserModelId = windowAppUserModelId(rw.hwnd);

        LONG exStyle = GetWindowLong(rw.hwnd, GWL_EXSTYLE);
        info.isToolWindow = (exStyle & WS_EX_TOOLWINDOW) != 0;
        info.isVisible = IsWindowVisible(rw.hwnd) && !IsIconic(rw.hwnd);
        info.isMinimized = IsIconic(rw.hwnd);
        info.isForeground = (rw.hwnd == foregroundHwnd);

        if (info.isForeground) {
            info.lastActiveTime = GetTickCount();
        }

        upsertWindowIndexes(info);
    }

    locker.unlock();
    emit cacheUpdated();
}

void WindowCache::addWindowToCache(const CachedWindowInfo &info, const QString &wmClass)
{
    const QString key = wmClass.toLower();
    if (key.isEmpty()) return;

    auto &windows = m_windowsByClass[key];
    windows.erase(
        std::remove_if(windows.begin(), windows.end(),
            [info](const CachedWindowInfo &w) { return w.hwnd == info.hwnd; }),
        windows.end());
    windows.append(info);
}

void WindowCache::removeWindowFromAllIndexes(HWND hwnd, DWORD pid)
{
    for (auto it = m_windowsByClass.begin(); it != m_windowsByClass.end(); ) {
        it.value().erase(
            std::remove_if(it.value().begin(), it.value().end(),
                [hwnd, pid](const CachedWindowInfo &w) {
                    return (hwnd && w.hwnd == hwnd) || (!hwnd && pid != 0 && w.pid == pid);
                }),
            it.value().end());

        if (it.value().isEmpty())
            it = m_windowsByClass.erase(it);
        else
            ++it;
    }
}

void WindowCache::upsertWindowIndexes(const CachedWindowInfo &info, const QString &extraClass)
{
    removeWindowFromAllIndexes(info.hwnd, info.pid);

    if (!info.exeName.isEmpty()) {
        m_pidToExeName[info.pid] = info.exeName;
        addWindowToCache(info, info.exeName);
    }
    if (!info.appUserModelId.isEmpty())
        addWindowToCache(info, info.appUserModelId);
    if (!extraClass.isEmpty())
        addWindowToCache(info, extraClass);
}

// ─── 增量刷新（WinEvent 触发）───

void WindowCache::refreshForPid(DWORD pid)
{
    // 先从 PID 找 exeName
    QString exeName;
    {
        QReadLocker locker(&m_lock);
        exeName = m_pidToExeName.value(pid);
    }
    if (exeName.isEmpty()) {
        // 未知 PID，做一次轻量全量刷新
        refresh();
        return;
    }

    // 移除此 PID 的旧条目
    {
        QWriteLocker locker(&m_lock);
        for (auto it = m_windowsByClass.begin(); it != m_windowsByClass.end(); ++it) {
            it.value().erase(
                std::remove_if(it.value().begin(), it.value().end(),
                    [pid](const CachedWindowInfo &w) { return w.pid == pid; }),
                it.value().end());
        }
        // 清理空列表
        for (auto it = m_windowsByClass.begin(); it != m_windowsByClass.end(); ) {
            if (it.value().isEmpty())
                it = m_windowsByClass.erase(it);
            else
                ++it;
        }
    }

    // 重新枚举此 PID 的窗口
    struct PidCtx { DWORD pid; QString exeName; QList<CachedWindowInfo> wins; };
    PidCtx ctx{pid, exeName, {}};

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto *ctx = reinterpret_cast<PidCtx *>(lParam);
        DWORD wPid = 0;
        GetWindowThreadProcessId(hwnd, &wPid);
        if (wPid != ctx->pid) return TRUE;

        CachedWindowInfo info;
        info.hwnd = hwnd;
        info.pid = wPid;
        wchar_t title[256] = {0};
        GetWindowTextW(hwnd, title, 255);
        info.title = QString::fromWCharArray(title);
        info.exeName = ctx->exeName;
        info.appUserModelId = windowAppUserModelId(hwnd);

        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        info.isToolWindow = (exStyle & WS_EX_TOOLWINDOW) != 0;
        info.isVisible = IsWindowVisible(hwnd) && !IsIconic(hwnd);
        info.isMinimized = IsIconic(hwnd);
        info.isForeground = (hwnd == GetForegroundWindow());

        ctx->wins.append(info);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    QWriteLocker locker(&m_lock);
    for (const auto &w : ctx.wins) {
        upsertWindowIndexes(w);
    }
    locker.unlock();
    emit cacheUpdated();
}

// ─── 按需同步扫描 ────────────────────────────────────────

void WindowCache::scanForClass(const QString &wmClass)
{
    QString lowerClass = wmClass.toLower();

    struct ScanCtx { QString target; QList<CachedWindowInfo> wins; };
    ScanCtx ctx{lowerClass, {}};

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto *ctx = reinterpret_cast<ScanCtx *>(lParam);
        DWORD pid = 0;
        GetWindowThreadProcessId(hwnd, &pid);
        if (pid == 0 || pid == 4) return TRUE;

        // 快速匹配：检查 PID→exeName 缓存
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess) return TRUE;

        wchar_t pathBuf[MAX_PATH] = {0};
        DWORD bufSize = MAX_PATH;
        QString exeName;
        BOOL matched = FALSE;
        if (QueryFullProcessImageNameW(hProcess, 0, pathBuf, &bufSize)) {
            exeName = QFileInfo(QString::fromWCharArray(pathBuf)).baseName().toLower();
            if (exeName == ctx->target) matched = TRUE;
        }
        CloseHandle(hProcess);
        const QString appId = windowAppUserModelId(hwnd);
        if (!matched && !appId.isEmpty() && appId == ctx->target) matched = TRUE;
        if (!matched) return TRUE;

        CachedWindowInfo info;
        info.hwnd = hwnd;
        info.pid = pid;
        wchar_t title[256] = {0};
        GetWindowTextW(hwnd, title, 255);
        info.title = QString::fromWCharArray(title);
        info.exeName = exeName;
        info.appUserModelId = appId;

        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        info.isToolWindow = (exStyle & WS_EX_TOOLWINDOW) != 0;
        info.isVisible = IsWindowVisible(hwnd) && !IsIconic(hwnd);
        info.isMinimized = IsIconic(hwnd);
        info.isForeground = (hwnd == GetForegroundWindow());

        ctx->wins.append(info);
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    // 扫描结果合并到缓存。先清除触发扫描 key 的旧条目，再按 HWND/PID
    // 移除其它 key 下的同一窗口旧状态，最后重建 exe/AppUserModelID/触发 key 索引。
    QWriteLocker locker(&m_lock);
    m_windowsByClass.remove(lowerClass);
    for (const auto &w : ctx.wins) {
        upsertWindowIndexes(w, lowerClass);
    }
    locker.unlock();
    emit cacheUpdated();
}

// ─── 查询接口 ────────────────────────────────────────────

bool WindowCache::hasVisibleWindows(const QString &wmClass)
{
    QReadLocker locker(&m_lock);
    auto it = m_windowsByClass.find(wmClass.toLower());
    if (it == m_windowsByClass.end()) return false;
    return std::any_of(it->begin(), it->end(),
        [](const CachedWindowInfo &w) { return w.isVisible; });
}

bool WindowCache::hasMinimizedWindows(const QString &wmClass)
{
    QReadLocker locker(&m_lock);
    auto it = m_windowsByClass.find(wmClass.toLower());
    if (it == m_windowsByClass.end()) return false;
    return std::any_of(it->begin(), it->end(),
        [](const CachedWindowInfo &w) { return w.isMinimized; });
}

bool WindowCache::hasHiddenWindows(const QString &wmClass)
{
    QReadLocker locker(&m_lock);
    auto it = m_windowsByClass.find(wmClass.toLower());
    if (it == m_windowsByClass.end()) return false;
    return std::any_of(it->begin(), it->end(),
        [](const CachedWindowInfo &w) { return !w.isVisible && !w.isMinimized && !w.isToolWindow; });
}

bool WindowCache::isForegroundApp(const QString &wmClass)
{
    QReadLocker locker(&m_lock);
    auto it = m_windowsByClass.find(wmClass.toLower());
    if (it == m_windowsByClass.end()) return false;
    return std::any_of(it->begin(), it->end(),
        [](const CachedWindowInfo &w) { return w.isForeground; });
}

int WindowCache::getWindowCount(const QString &wmClass)
{
    QReadLocker locker(&m_lock);
    auto it = m_windowsByClass.find(wmClass.toLower());
    if (it == m_windowsByClass.end()) return 0;
    return static_cast<int>(std::count_if(it->begin(), it->end(),
        [](const CachedWindowInfo &w) { return !w.isToolWindow; }));
}

WindowList WindowCache::getWindowsForPreview(const QString &wmClass)
{
    QReadLocker locker(&m_lock);
    auto it = m_windowsByClass.find(wmClass.toLower());
    if (it == m_windowsByClass.end()) return {};
    WindowList result;
    std::copy_if(it->begin(), it->end(), std::back_inserter(result),
        [](const CachedWindowInfo &w) {
            return !w.isToolWindow
                && !w.title.trimmed().isEmpty()
                && (w.isVisible || w.isMinimized)
                && IsWindow(w.hwnd)
                && !SysHelper::isWindowCloaked(w.hwnd);  // 排除 Virtual Desktop 隐藏的窗口
        });
    return result;
}

WindowList WindowCache::getWindowsForActivation(const QString &wmClass)
{
    QReadLocker locker(&m_lock);
    auto it = m_windowsByClass.find(wmClass.toLower());
    if (it == m_windowsByClass.end()) return {};

    WindowList result;
    std::copy_if(it->begin(), it->end(), std::back_inserter(result),
        [](const CachedWindowInfo &w) {
            return !w.isToolWindow && IsWindow(w.hwnd);
        });
    return result;
}

HWND WindowCache::getLastActiveHwnd(const QString &wmClass)
{
    const WindowList windows = getWindowsForActivation(wmClass);

    HWND best = nullptr;
    DWORD bestTime = 0;
    for (const auto &w : windows) {
        if (!best || w.lastActiveTime > bestTime) {
            best = w.hwnd;
            bestTime = w.lastActiveTime;
        }
    }
    return best;
}

QString WindowCache::getAppIdForPid(DWORD pid)
{
    QReadLocker locker(&m_lock);
    auto it = m_pidToExeName.find(pid);
    if (it == m_pidToExeName.end()) return {};
    return it.value();
}

// ─── 动作接口 ────────────────────────────────────────────

bool WindowCache::activateWindow(const QString &wmClass)
{
    HWND hwnd = getLastActiveHwnd(wmClass);
    if (!hwnd || !IsWindow(hwnd)) return false;

    AllowSetForegroundWindow(ASFW_ANY);
    SetForegroundWindow(hwnd);
    ShowWindow(hwnd, SW_RESTORE);
    SetFocus(hwnd);
    return true;
}

void WindowCache::showWindowPicker()
{
    INPUT inputs[4] = {};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_LWIN;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = VK_TAB;
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = VK_TAB;
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_LWIN;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(4, inputs, sizeof(INPUT));
}

