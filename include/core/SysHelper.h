#ifndef SYSHELPER_H
#define SYSHELPER_H

#include <QObject>
#include <QList>
#include <QWidget>  // for WId
#include "Types.h"

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

    /** @brief 注册全局窗口钩子，监听前台窗口状态变化 */
    bool installWindowHook();

    /** @brief 注册低级键盘钩子，捕获 Win 键（仅 Hidden 状态启用） */
    bool installKeyboardHook();

    /** @brief 移除键盘钩子 */
    void uninstallKeyboardHook();

    /** @brief 返回当前前台窗口是否最大化或全屏 */
    bool getForegroundWindowState();

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

signals:
    /** @brief 前台窗口状态变化 signal */
    void foregroundWindowChanged(bool isMaximizedOrFullscreen);

    /** @brief Win 键被按下 signal */
    void winKeyPressed();
};

#endif // SYSHELPER_H
