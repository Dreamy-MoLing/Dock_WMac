#ifndef WINDOWCACHE_H
#define WINDOWCACHE_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QReadWriteLock>
#include <QString>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

/**
 * @file WindowCache.h
 * @brief 窗口枚举缓存层
 *
 * 一次性 EnumWindows 全量缓存，按 wmClass 索引。
 * 供 ClickStateMachine、WindowPreviewPanel、DockWindow 查询。
 * 合并原 WindowManager 的 getWindowCount/activateWindow/showWindowPicker。
 *
 * 线程安全：QReadWriteLock 保护读写。
 * 刷新策略：WinEvent 主驱动 + ProcessMonitor 2s 兜底 + 点击按需同步扫描。
 */

struct CachedWindowInfo
{
    HWND   hwnd = nullptr;
    QString title;
    DWORD  pid = 0;
    QString exeName;            // 可执行文件名 basename（无 .exe 后缀，小写）
    bool   isVisible = false;   // IsWindowVisible && !IsIconic
    bool   isMinimized = false; // IsIconic
    bool   isForeground = false;// GetForegroundWindow() == hwnd
    bool   isToolWindow = false;// WS_EX_TOOLWINDOW
    DWORD  lastActiveTime = 0;  // GetTickCount() 上次激活时间
};

using WindowList = QList<CachedWindowInfo>;

class WindowCache : public QObject
{
    Q_OBJECT
public:
    explicit WindowCache(QObject *parent = nullptr);

    // ── 查询接口 ──
    bool hasVisibleWindows(const QString &wmClass);
    bool hasMinimizedWindows(const QString &wmClass);
    bool hasHiddenWindows(const QString &wmClass);
    bool isForegroundApp(const QString &wmClass);
    int getWindowCount(const QString &wmClass);
    WindowList getWindowsForPreview(const QString &wmClass);
    HWND getLastActiveHwnd(const QString &wmClass);

    // ── 动作接口（原 WindowManager）──
    bool activateWindow(const QString &wmClass);
    void showWindowPicker();

    // ── 刷新接口 ──
    /** @brief 全量刷新缓存（EnumWindows） */
    void refresh();

    /** @brief 针对单个 PID 的增量刷新（WinEvent 回调触发） */
    void refreshForPid(DWORD pid);

    /** @brief 针对单个 wmClass 的同步扫描（点击判定前） */
    void scanForClass(const QString &wmClass);

signals:
    /** @brief 缓存更新后发出（DockWindow 刷新图标状态） */
    void cacheUpdated();

private:
    void addWindowToCache(const CachedWindowInfo &info, const QString &wmClass);

    QHash<QString, WindowList> m_windowsByClass; // wmClass → 窗口列表
    QHash<DWORD, QString>      m_pidToExeName;   // PID → exe basename
    QReadWriteLock              m_lock;
};

#endif // WINDOWCACHE_H
