#ifndef WINDOWPREVIEWPANEL_H
#define WINDOWPREVIEWPANEL_H

#include <QObject>
#include <QTimer>

class DockItem;
class SysHelper;

/**
 * @file WindowPreviewPanel.h
 * @brief Dock 图标悬停窗口预览面板
 *
 * 鼠标悬停 DockItem 500ms 后，弹出该应用所有窗口的缩略图。
 * 使用 Win32 EnumWindows + PrintWindow 捕获缩略图。
 * 点击缩略图可切换到对应窗口。
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

    /** @brief 开始显示指定应用的窗口预览（500ms 延迟后弹出） */
    void showPreview(DockItem *item);

    /** @brief 隐藏预览面板 */
    void hidePreview();

    /** @brief 检查面板是否正在显示 */
    bool isVisible() const;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onPreviewTimerTimeout();

private:
    void buildPreviewContent(DockItem *item);
    void clearContent();

    SysHelper *m_sysHelper = nullptr;
    QTimer *m_previewTimer;
    QWidget *m_previewPopup = nullptr;
    DockItem *m_previewItem = nullptr;
};

#endif // WINDOWPREVIEWPANEL_H
