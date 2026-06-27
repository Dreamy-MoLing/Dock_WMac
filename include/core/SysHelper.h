#ifndef SYSHELPER_H
#define SYSHELPER_H

#include <QObject>
#include <QList>
#include <QWidget>  // for WId
#include <QTimer>
#include <QStringList>
#include "Types.h"

#ifdef Q_OS_WIN
#include <windows.h>
#endif

/**
 * @file SysHelper.h
 * @brief 系统适配层抽象接口
 *
 * 封装所有底层系统交互，包括任务栏固定项读取、窗口状态监听、全局热键等。
 * 使用 Win32 API 实现。
 */

class SysHelper : public QObject {
    Q_OBJECT
public:
    explicit SysHelper(QObject *parent = nullptr);
    ~SysHelper() override;

    /** @brief 注册全局窗口钩子，监听前台窗口状态变化 */
    bool installWindowHook();

    /** @brief 移除全局窗口钩子 */
    void uninstallWindowHook();

    /** @brief 注册低级键盘钩子，捕获 Win 键（仅 Hidden 状态启用） */
    bool installKeyboardHook();

    /** @brief 移除键盘钩子 */
    void uninstallKeyboardHook();

    /** @brief 返回当前前台窗口是否最大化或全屏 */
    bool getForegroundWindowState();

    /**
     * @brief 检查主屏幕（或指定屏幕）上是否存在最大化或全屏的窗口
     * @param monitorIndex -1 表示主屏幕，0+ 表示指定显示器
     * @return true 如果存在任何最大化/全屏窗口
     */
    bool hasMaximizedOrFullscreenWindowOnMonitor(int monitorIndex = -1);

    /** @brief 设置开机自启（注册表） */
    bool setAutoStart(bool enabled);

    /** @brief 检查是否已设置开机自启 */
    bool isAutoStartEnabled() const;

    /** @brief 为窗口启用 DWM 毛玻璃模糊效果（全窗口） */
    void enableBlurBehindWindow(WId winId);

    /** @brief 为窗口指定区域启用 DWM 毛玻璃模糊（与 WA_TranslucentBackground 兼容） */
    void enableBlurBehindWindow(WId winId, const QRect &blurRect, int cornerRadius);

    /** @brief 检查系统是否支持 DWM 模糊效果 */
    bool isBlurSupported() const;

    /** @brief 触发全屏状态防抖检测（供 WinEventProc 静态回调调用） */
    void triggerFullscreenDebounce();

    /** @brief 检测当前系统是否为亮色主题 */
    bool isLightTheme() const;

    /** @brief 检测系统任务栏自动隐藏是否开启 */
    bool isTaskbarAutoHideEnabled() const;

    /** @brief 获取当前鼠标全局位置 */
    QPoint cursorPos() const;

    /** @brief 隐藏原生任务栏 */
    void hideNativeTaskbar();

    /** @brief 恢复原生任务栏显示 */
    void restoreNativeTaskbar();

    /**
     * @brief 获取窗口的扩展帧边界（物理像素，DPI 感知）
     * @param hwnd 目标窗口句柄
     * @param outRect 输出矩形（物理像素）
     * @return true 如果 DwmGetWindowAttribute 成功，false 时回退到 GetWindowRect
     */
    bool getExtendedFrameBounds(HWND hwnd, RECT &outRect);

    /**
     * @brief 检测窗口是否被 Cloaked（Virtual Desktop 隐藏等）
     * @param hwnd 目标窗口句柄
     * @return true 如果窗口被 cloaked（Windows 8+），Windows 7 始终返回 false
     */
    static bool isWindowCloaked(HWND hwnd);

    /**
     * @brief 获取窗口的显示亲和性（截图保护状态）
     * @param hwnd 目标窗口句柄
     * @param outAffinity 输出亲和性值（WDA_NONE=0, WDA_MONITOR=1, WDA_EXCLUDEFROMCAPTURE=0x11）
     * @return true 如果 API 成功，false 时 outAffinity 设为 WDA_NONE
     */
    static bool getWindowDisplayAffinity(HWND hwnd, DWORD &outAffinity);

    /** @brief 解析 .lnk 快捷方式文件，返回目标路径 */
    static QString resolveShortcut(const QString &lnkPath);

    /** @brief 使用 Windows Shell 启动 .exe/.lnk/shell 目标 */
    static bool launchPath(const QString &path, const QStringList &arguments = {});

signals:
    /** @brief 前台窗口状态变化 signal */
    void foregroundWindowChanged(bool isMaximizedOrFullscreen);

    /**
     * @brief 主屏幕最大化/全屏状态变化 signal
     * @param anyMaximizedOrFullscreen 主屏幕是否有任何窗口最大化或全屏
     */
    void fullscreenStateChanged(bool anyMaximizedOrFullscreen);

    /** @brief Win 键被按下 signal */
    void winKeyPressed();

    /** @brief 窗口事件发生（CREATE/DESTROY/SHOW/HIDE）
     *  @param pid 窗口所属进程 ID */
    void windowEventOccurred(DWORD pid);

    /** @brief 窗口从隐藏变为可见（EVENT_OBJECT_SHOW），用于触发 DockItem 绿色光点 */
    void windowShowOccurred(DWORD pid);

private:
    /** @brief 全屏状态防抖定时器（避免高频事件触发重复扫描） */
    QTimer *m_fullscreenDebounceTimer = nullptr;
#ifdef Q_OS_WIN
    QList<HWINEVENTHOOK> m_windowHooks;
#endif
};

#endif // SYSHELPER_H
