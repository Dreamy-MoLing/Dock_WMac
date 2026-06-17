#ifndef WINDOWPREVIEWPANEL_H
#define WINDOWPREVIEWPANEL_H

#include <QObject>
#include <QTimer>
#include <QList>
#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shobjidl.h>  // IVirtualDesktopManager
#endif

class DockItem;
class SysHelper;
class WindowCache;

/**
 * @file WindowPreviewPanel.h
 * @brief Dock 图标悬停窗口预览面板
 *
 * 鼠标悬停 DockItem 500ms 后，弹出该应用所有窗口的缩略图。
 * 使用 WindowCache 获取窗口列表 + DWM DwmRegisterThumbnail 实时渲染缩略图。
 * DWM 不可用时回退到应用图标；点击缩略图可切换到对应窗口。
 *
 * DWM Peek Lite：鼠标悬停缩略图 300ms 后 SetWindowPos(HWND_TOP) 临时置顶。
 * 离开防抖：popup Leave 事件启动 200ms 定时器，鼠标回到 Dock 可取消。
 *
 * 由 DockWindow 创建并持有，通过 showPreview(item) / hidePreview() 控制显示。
 */

class WindowPreviewPanel : public QObject
{
    Q_OBJECT
public:
    explicit WindowPreviewPanel(QObject *parent = nullptr);
    ~WindowPreviewPanel() override;

    /** @brief 设置 SysHelper（用于窗口操作） */
    void setSysHelper(SysHelper *helper);

    /** @brief 设置 WindowCache（替代独立 EnumWindows） */
    void setWindowCache(WindowCache *cache);

    /** @brief 开始显示指定应用的窗口预览（500ms 延迟后弹出） */
    void showPreview(DockItem *item);

    /** @brief 隐藏预览面板 */
    void hidePreview();

    /** @brief 延迟隐藏预览面板（给鼠标移动到预览窗口的时间） */
    void startDelayedHide();

    /** @brief 检查面板是否正在显示 */
    bool isVisible() const;

signals:
    /** @brief 预览窗显示（通知 DockWindow 锁定鱼眼） */
    void previewShown();

    /** @brief 预览窗隐藏（通知 DockWindow 释放鱼眼） */
    void previewHidden();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onPreviewTimerTimeout();
    void onLeaveTimerTimeout();       // 离开防抖超时

private:
    void buildPreviewContent(DockItem *item);
    void clearContent();
    void startPeek(HWND targetHwnd);   // DWM peek lite
    void stopPeek();                    // 取消 peek

    /** @brief 检查窗口是否在当前虚拟桌面（Windows 10+），Windows 7/8 返回 true */
    bool isWindowOnCurrentDesktop(HWND hwnd);

    SysHelper    *m_sysHelper = nullptr;
    WindowCache  *m_windowCache = nullptr;
    QTimer       *m_previewTimer;
    QTimer       *m_leaveTimer;        // 离开防抖定时器
    QWidget      *m_previewPopup = nullptr;
    DockItem     *m_previewItem = nullptr;
    HWND          m_peekTarget = nullptr;  // 当前 peek 目标窗口
    QTimer       *m_peekTimer = nullptr;   // peek 防抖定时器
#ifdef Q_OS_WIN
    IVirtualDesktopManager *m_virtualDesktopManager = nullptr;  // 虚拟桌面管理器（Windows 10+）
#endif
};

#endif // WINDOWPREVIEWPANEL_H
