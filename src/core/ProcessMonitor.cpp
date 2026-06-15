/**
 * @file ProcessMonitor.cpp
 * @brief 应用进程状态监控器实现
 *
 * 使用 Win32 API (CreateToolhelp32Snapshot) 周期性检测进程运行状态，
 * 将变化以信号形式通知 UI 层。
 *
 * 策略：只添加有顶层可见窗口的用户应用程序，过滤系统进程和后台服务。
 */

#include "core/ProcessMonitor.h"
#include "core/AppIdHelper.h"
#include <QFileInfo>
#include <QSet>
#include <QDebug>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tlhelp32.h>

// 用于 EnumWindows 回调的结构体
struct WindowProcessInfo {
    QSet<DWORD> pidsWithWindows;  // 有可见顶层窗口的进程 ID
};

// EnumWindows 回调函数
static BOOL CALLBACK EnumWindowsCallback(HWND hwnd, LPARAM lParam)
{
    auto *info = reinterpret_cast<WindowProcessInfo *>(lParam);

    // 只考虑可见的顶层窗口
    if (!IsWindowVisible(hwnd)) return TRUE;

    // 跳过工具窗口和不可激活的弹出窗口
    LONG style = GetWindowLong(hwnd, GWL_STYLE);
    LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
    if (!(style & WS_CAPTION)) return TRUE;
    if (exStyle & WS_EX_TOOLWINDOW) return TRUE;  // 工具窗口（系统托盘等）
    if (exStyle & WS_EX_NOACTIVATE) return TRUE;  // 不可激活窗口（触摸键盘等）

    // 跳过无标题或标题过短的窗口
    wchar_t title[256] = {0};
    int titleLen = GetWindowTextW(hwnd, title, 255);
    if (titleLen < 2) return TRUE;

    // 跳过系统管理窗口（通常无意义标题）
    LONG_PTR owner = GetWindowLongPtr(hwnd, GWLP_HWNDPARENT);
    if (owner != 0 && !IsWindowVisible(reinterpret_cast<HWND>(owner))) {
        // 有不可见父窗口的子窗口通常是对话框/内部面板
        return TRUE;
    }

    // 获取进程 ID
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0 || pid == 4) return TRUE;  // 跳过 System (PID 4) 和无效 PID

    info->pidsWithWindows.insert(pid);
    return TRUE;
}

// 获取有顶层窗口的进程 ID 集合
static QSet<DWORD> getPidsWithWindows()
{
    WindowProcessInfo info;
    EnumWindows(EnumWindowsCallback, reinterpret_cast<LPARAM>(&info));
    return info.pidsWithWindows;
}

// 通过 PID 获取进程名（小写，无 .exe 后缀）
static QString getProcessNameByPid(DWORD pid)
{
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return QString();

    PROCESSENTRY32W pe;
    pe.dwSize = sizeof(pe);
    QString name;

    if (Process32FirstW(snapshot, &pe)) {
        do {
            if (pe.th32ProcessID == pid) {
                name = QString::fromWCharArray(pe.szExeFile).toLower();
                if (name.endsWith(".exe")) name.chop(4);
                break;
            }
        } while (Process32NextW(snapshot, &pe));
    }

    CloseHandle(snapshot);
    return name;
}

ProcessMonitor::ProcessMonitor(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
    , m_tickCount(0)
{
    connect(m_timer, &QTimer::timeout, this, &ProcessMonitor::onTick);
}

void ProcessMonitor::start(int intervalMs)
{
    m_tickCount = 0;
    m_timer->start(intervalMs);
}

void ProcessMonitor::stop()
{
    m_timer->stop();
}

void ProcessMonitor::registerApp(const QString &appId, const QString &execPath)
{
    m_registeredApps.insert(appId);
    if (!execPath.isEmpty()) {
        m_registeredExecPaths[appId] = execPath;
    }
}

void ProcessMonitor::registerApps(const QList<DockItemData> &items)
{
    for (const auto &item : items) {
        m_registeredApps.insert(item.appId);
        if (!item.execPath.isEmpty()) {
            m_registeredExecPaths[item.appId] = item.execPath;
        }
    }
}

void ProcessMonitor::unregisterApp(const QString &appId)
{
    m_registeredApps.remove(appId);
    m_runningCache.remove(appId);
}

/**
 * @brief 获取当前运行中的进程名集合
 *
 * 使用 CreateToolhelp32Snapshot 枚举所有进程，
 * 返回小写的可执行文件名（不含 .exe）集合。
 */
static QSet<QString> getRunningProcessNames()
{
    QSet<QString> names;
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return names;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    if (Process32First(hSnap, &pe)) {
        do {
            QString exeName = QString::fromWCharArray(pe.szExeFile).toLower();
            // 移除 .exe 后缀
            if (exeName.endsWith(".exe")) {
                exeName.chop(4);
            }
            names.insert(exeName);
        } while (Process32Next(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return names;
}

void ProcessMonitor::onTick()
{
    if (m_registeredApps.isEmpty()) {
        m_tickCount++;
        return;
    }

    // 每 4 次 tick 扫描一次新应用
    if (m_tickCount % 4 == 0) {
        scanTransientApps();
    }
    m_tickCount++;

    // 获取当前运行中的进程名
    QSet<QString> runningNames = getRunningProcessNames();

    // 检测已注册应用的运行状态
    for (const auto &appId : m_registeredApps) {
        QString processName = AppIdHelper::deriveWmClass(
            m_registeredExecPaths.value(appId), appId);
        bool isRunning = runningNames.contains(processName);

        // 如果 appId 推导名未匹配，尝试用可执行文件 basename
        // 解决 appId="Visual Studio Code" execPath="Code.exe" 这类不匹配
        if (!isRunning && m_registeredExecPaths.contains(appId)) {
            QFileInfo fi(m_registeredExecPaths[appId]);
            QString exeBaseName = fi.baseName().toLower();
            if (!exeBaseName.isEmpty() && exeBaseName != processName) {
                isRunning = runningNames.contains(exeBaseName);
            }
        }

        bool wasRunning = m_runningCache.value(appId, false);

        if (isRunning != wasRunning) {
            m_runningCache[appId] = isRunning;
            emit appRunningStateChanged(appId, isRunning);
        }
    }
}

void ProcessMonitor::scanTransientApps()
{
    // 策略：只添加有顶层可见窗口的应用程序
    // 这比维护一个系统进程列表更可靠
    QSet<DWORD> pidsWithWindows = getPidsWithWindows();

    // 构建已注册应用的进程名集合（用于匹配）
    // 同时收集已注册应用的 exe 路径，用于更精确的匹配
    QSet<QString> registeredProcessNames;
    QSet<QString> registeredExePaths;
    for (const auto &appId : m_registeredApps) {
        registeredProcessNames.insert(
            AppIdHelper::deriveWmClass(m_registeredExecPaths.value(appId), appId));
        // 如果有 execPath，也添加到匹配集合
        if (m_registeredExecPaths.contains(appId)) {
            QFileInfo fi(m_registeredExecPaths[appId]);
            registeredExePaths.insert(fi.baseName().toLower());
        }
    }

    // 遍历有窗口的进程，添加新应用
    QList<DockItemData> newItems;
    QSet<QString> currentRunning;

    for (DWORD pid : pidsWithWindows) {
        QString procName = getProcessNameByPid(pid);
        if (procName.isEmpty()) continue;

        currentRunning.insert(procName);

        // 跳过已注册的（固定项或已有临时项）
        if (registeredProcessNames.contains(procName)) continue;
        if (registeredExePaths.contains(procName)) continue;
        if (m_detectedRunningApps.contains(procName)) continue;

        // 获取进程路径（用于图标）
        QString exePath;
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProcess) {
            wchar_t pathBuf[MAX_PATH] = {0};
            DWORD bufSize = MAX_PATH;
            if (QueryFullProcessImageNameW(hProcess, 0, pathBuf, &bufSize)) {
                exePath = QString::fromWCharArray(pathBuf);
            }
            CloseHandle(hProcess);
        }

        // 过滤系统/后台进程：跳过 Windows 系统目录和 UWP 容器中的进程
        if (!exePath.isEmpty()) {
            QString lower = exePath.toLower();
            if (lower.startsWith("c:\\windows\\system32") ||
                lower.startsWith("c:\\windows\\syswow64") ||
                lower.startsWith("c:\\windows\\systemapps") ||
                lower.contains("\\windows\\")) {
                continue;
            }
            // 跳过没有用户界面的系统进程
            if (lower.contains("\\windowsapps\\") ||       // UWP 运行时容器
                lower.contains("\\windows.old\\") ||
                lower.endsWith("svchost.exe") ||
                lower.endsWith("dllhost.exe") ||
                lower.endsWith("conhost.exe") ||
                lower.endsWith("runtimebroker.exe") ||
                lower.endsWith("shellexperiencehost.exe") ||
                lower.endsWith("searchhost.exe") ||
                lower.endsWith("startmenuexperiencehost.exe") ||
                lower.endsWith("textinputhost.exe") ||
                lower.endsWith("ctfmon.exe")) {
                continue;
            }
        }

        // 新应用出现
        DockItemData item;
        item.appId = procName;
        item.displayName = procName;
        item.execPath = exePath;
        item.iconPath = exePath;
        item.isRunning = true;

        m_detectedRunningApps.insert(procName);
        newItems.append(item);
    }

    // 批量发送新应用信号
    if (!newItems.isEmpty()) {
        for (const auto &item : newItems) {
            emit newRunningAppDetected(item);
        }
    }

    // 清理已退出的临时应用
    QSet<QString> exited = m_detectedRunningApps - currentRunning;
    for (const auto &appId : exited) {
        m_detectedRunningApps.remove(appId);
        emit runningAppExited(appId);
    }

    // 调试日志（每次扫描输出，scanTransientApps 本身由 onTick 每 4 tick 调用一次）
    qInfo() << "ProcessMonitor: 有窗口进程=" << pidsWithWindows.size()
            << "新增=" << newItems.size()
            << "临时项总数=" << m_detectedRunningApps.size();
}
