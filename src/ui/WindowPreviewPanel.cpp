/**
 * @file WindowPreviewPanel.cpp
 * @brief 窗口缩略图预览面板实现
 *
 * 鼠标悬停 Dock 图标 500ms 后弹出该应用所有窗口的实时缩略图。
 * 使用 DWM DwmRegisterThumbnail / DwmUpdateThumbnailProperties（任务栏同款 API），
 * 窗口列表从 WindowCache 获取（不再自 EnumWindows）。
 * DWM 不可用时（远程桌面等）回退到应用图标。
 * DWM Peek Lite：悬停缩略图 300ms 后 SetWindowPos(HWND_TOP)。
 */

#include "ui/WindowPreviewPanel.h"
#include "ui/DockItem.h"
#include "core/SysHelper.h"
#include "core/WindowCache.h"
#include "core/AppIdHelper.h"
#include "core/Types.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QGuiApplication>
#include <QCursor>
#include "core/IconProvider.h"
#include <QPainter>
#include <QMouseEvent>
#include <QDebug>
#include <QSet>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

WindowPreviewPanel::WindowPreviewPanel(QObject *parent)
    : QObject(parent)
    , m_previewTimer(new QTimer(this))
    , m_leaveTimer(new QTimer(this))
    , m_peekTimer(new QTimer(this))
{
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(500);
    connect(m_previewTimer, &QTimer::timeout,
            this, &WindowPreviewPanel::onPreviewTimerTimeout);

    m_leaveTimer->setSingleShot(true);
    m_leaveTimer->setInterval(300);
    connect(m_leaveTimer, &QTimer::timeout,
            this, &WindowPreviewPanel::onLeaveTimerTimeout);

    m_peekTimer->setSingleShot(true);
    m_peekTimer->setInterval(300);

#ifdef Q_OS_WIN
    // 按需初始化 IVirtualDesktopManager（Windows 10+，失败时保持 nullptr）
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    if (SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE) {
        CoCreateInstance(CLSID_VirtualDesktopManager, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&m_virtualDesktopManager));
    }
#endif
}

WindowPreviewPanel::~WindowPreviewPanel()
{
    clearContent();
#ifdef Q_OS_WIN
    if (m_virtualDesktopManager) {
        m_virtualDesktopManager->Release();
        m_virtualDesktopManager = nullptr;
    }
#endif
}

void WindowPreviewPanel::setSysHelper(SysHelper *helper)
{
    m_sysHelper = helper;
}

void WindowPreviewPanel::setWindowCache(WindowCache *cache)
{
    m_windowCache = cache;
}

void WindowPreviewPanel::showPreview(DockItem *item)
{
    if (!item) return;
    cancelDelayedHide();
    if (item == m_previewItem) return;

    hidePreview();
    m_previewItem = item;
    m_previewTimer->start();
}

void WindowPreviewPanel::hidePreview()
{
    m_previewTimer->stop();
    m_leaveTimer->stop();
    m_peekTimer->stop();
    stopPeek();
    m_previewItem = nullptr;
    clearContent();
    emit previewHidden();
}

void WindowPreviewPanel::startDelayedHide()
{
    if (m_previewPopup && m_previewPopup->isVisible()) {
        m_leaveTimer->start();
    }
}

void WindowPreviewPanel::cancelDelayedHide()
{
    m_leaveTimer->stop();
}

bool WindowPreviewPanel::isVisible() const
{
    return m_previewPopup != nullptr;
}

void WindowPreviewPanel::onPreviewTimerTimeout()
{
    if (!m_previewItem) return;
    buildPreviewContent(m_previewItem);
}

void WindowPreviewPanel::onLeaveTimerTimeout()
{
    if (cursorInsidePreview())
        return;
    hidePreview();
}

void WindowPreviewPanel::clearContent()
{
    // 先注销所有 DWM 缩略图（必须在 popup 销毁前调用）
    if (m_previewPopup) {
        QVariant thumbListVar = m_previewPopup->property("dwmThumbList");
        if (thumbListVar.isValid()) {
            const QVariantList list = thumbListVar.toList();
            for (const QVariant &entry : list) {
                QVariantMap map = entry.toMap();
                HTHUMBNAIL thumbId = reinterpret_cast<HTHUMBNAIL>(
                    static_cast<quintptr>(map["thumbId"].toULongLong()));
                if (thumbId) {
                    DwmUnregisterThumbnail(thumbId);
                }
            }
        }
        m_previewPopup->close();
        m_previewPopup->deleteLater();
        m_previewPopup = nullptr;
    }
}

void WindowPreviewPanel::buildPreviewContent(DockItem *item)
{
    if (!item || !m_sysHelper || !m_windowCache) return;

    DockItemData data;
    data.appId = item->appId();
    data.execPath = item->execPath();
    data.targetPath = item->targetPath();
    data.appUserModelId = item->appUserModelId();
    const QStringList identityKeys = AppIdHelper::identityKeys(data);
    if (identityKeys.isEmpty()) return;

    // 从 WindowCache 获取窗口列表（不再自 EnumWindows），并合并多个身份 key 的结果。
    WindowList windows;
    QSet<quintptr> seenWindows;
    for (const QString &key : identityKeys) {
        m_windowCache->scanForClass(key);
        const WindowList keyWindows = m_windowCache->getWindowsForPreview(key);
        for (const CachedWindowInfo &window : keyWindows) {
            const quintptr hwndValue = reinterpret_cast<quintptr>(window.hwnd);
            if (seenWindows.contains(hwndValue))
                continue;
            seenWindows.insert(hwndValue);
            windows.append(window);
        }
    }
    if (windows.isEmpty())
        return;

    // 过滤非当前虚拟桌面的窗口
    windows.erase(
        std::remove_if(windows.begin(), windows.end(),
            [this](const CachedWindowInfo &w) {
                return !isWindowOnCurrentDesktop(w.hwnd);
            }),
        windows.end());
    if (windows.isEmpty())
        return;

    int maxPreviews = qMin(windows.size(), 6);
    bool singleWindow = (windows.size() == 1);

    clearContent();
    m_previewPopup = new QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    m_previewPopup->setAttribute(Qt::WA_ShowWithoutActivating);

    int thumbW, thumbH, spacing, padding, titleH, popupW, popupH;
    if (singleWindow) {
        // 单窗口：更大的缩略图，无标题栏
        thumbW = 200; thumbH = 130; spacing = 0; padding = 8; titleH = 0;
        popupW = thumbW + 2 * padding;
        popupH = thumbH + 2 * padding;
    } else {
        // 多窗口：网格布局 + 标题栏
        thumbW = 160; thumbH = 100; spacing = 8; padding = 10; titleH = 20;
        popupW = maxPreviews * thumbW + (maxPreviews - 1) * spacing + 2 * padding;
        popupH = thumbH + titleH + 2 * padding;
    }
    m_previewPopup->setFixedSize(popupW, popupH);
    m_previewPopup->setStyleSheet("background: rgb(30, 30, 30);");
    m_previewPopup->installEventFilter(this);

    // ── 创建标题标签（仅多窗口时）──
    if (!singleWindow) {
        for (int i = 0; i < maxPreviews; ++i) {
            const auto &wi = windows[i];
            QLabel *titleLabel = new QLabel(m_previewPopup);
            QString elidedTitle = wi.title;
            if (elidedTitle.length() > 20)
                elidedTitle = elidedTitle.left(19) + "...";
            titleLabel->setText(elidedTitle);
            titleLabel->setFixedSize(thumbW, titleH);
            titleLabel->setAlignment(Qt::AlignCenter);
            titleLabel->setStyleSheet("background: transparent; color: white; font-size: 11px;");
            titleLabel->move(padding + i * (thumbW + spacing), padding + thumbH + 4);
            titleLabel->installEventFilter(this);
        }
    }

    // ── 定位弹出面板并先显示 ──
    QPoint itemCenter = item->mapToGlobal(QPoint(item->width() / 2, 0));
    int popupX = itemCenter.x() - popupW / 2;
    int popupY = item->mapToGlobal(QPoint(0, 0)).y() - popupH - 12;

    QWidget *dockWindow = item->window();
    if (dockWindow) {
        QPoint dockTopCenter = dockWindow->mapToGlobal(
            QPoint(dockWindow->width() / 2, 0));
        popupX = itemCenter.x() - popupW / 2;
        popupY = dockTopCenter.y() - popupH - 12;
    }

    QScreen *screen = QGuiApplication::screenAt(itemCenter);
    if (screen) {
        QRect geo = screen->availableGeometry();
        popupX = qBound(geo.left() + 8, popupX, geo.right() - popupW - 8);
        if (popupY < geo.top()) popupY = geo.top() + 8;
    }

    m_previewPopup->move(popupX, popupY);

    // 先锁定鱼眼再显示弹窗（避免 show() 捕获鼠标导致 leaveEvent 重置鱼眼）
    emit previewShown();

    m_previewPopup->show();

    // Windows 11 DWM 圆角
    HWND popupHwnd = reinterpret_cast<HWND>(m_previewPopup->winId());
    const int DWMWA_WINDOW_CORNER_PREFERENCE = 33;
    const int DWMWCP_ROUND = 2;
    DwmSetWindowAttribute(popupHwnd, DWMWA_WINDOW_CORNER_PREFERENCE,
                          &DWMWCP_ROUND, sizeof(DWMWCP_ROUND));

    // ── DPI 缩放因子：DWM API 使用物理像素，Qt 坐标是逻辑像素 ──
    qreal dpiScale = 1.0;
    if (auto *ps = m_previewPopup->screen()) {
        dpiScale = ps->devicePixelRatio();
    }

    // ── 注册 DWM 实时缩略图 ──
    QVariantList thumbList;
    bool anyDwmSucceeded = false;

    for (int i = 0; i < maxPreviews; ++i) {
        const auto &wi = windows[i];
        int tx = padding + i * (thumbW + spacing);
        int ty = padding;

        QVariantMap entry;
        entry["srcHwnd"] = static_cast<qulonglong>(reinterpret_cast<quintptr>(wi.hwnd));
        entry["x"] = tx;
        entry["y"] = ty;
        entry["w"] = thumbW;
        entry["h"] = thumbH;

        // 检查截图保护：如果窗口不允许捕获，跳过缩略图注册
        DWORD affinity = 0;
        bool isProtected = m_sysHelper->getWindowDisplayAffinity(wi.hwnd, affinity)
                        && affinity != 0;  // WDA_NONE = 0

        HTHUMBNAIL thumbId = nullptr;
        bool thumbnailOk = false;

        if (!isProtected) {
            HRESULT hr = DwmRegisterThumbnail(popupHwnd, wi.hwnd, &thumbId);
            if (SUCCEEDED(hr)) {
                DWM_THUMBNAIL_PROPERTIES props = {};
                props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_VISIBLE
                              | DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;
                props.fVisible = TRUE;
                props.opacity = 255;
                props.fSourceClientAreaOnly = TRUE;
                props.rcDestination = {
                    static_cast<LONG>(tx * dpiScale),
                    static_cast<LONG>(ty * dpiScale),
                    static_cast<LONG>((tx + thumbW) * dpiScale),
                    static_cast<LONG>((ty + thumbH) * dpiScale)
                };

                RECT srcRect;
                if (GetClientRect(wi.hwnd, &srcRect)) {
                    props.rcSource = srcRect;
                }
                DwmUpdateThumbnailProperties(thumbId, &props);
                thumbnailOk = true;
                anyDwmSucceeded = true;
            }
        }

        if (thumbnailOk) {
            entry["thumbId"] = static_cast<qulonglong>(reinterpret_cast<quintptr>(thumbId));
        } else {
            entry["thumbId"] = static_cast<qulonglong>(0);
            // 回退：在 popup 上画应用图标（截图保护或 DWM 失败时）
            QPixmap thumb(thumbW, thumbH);
            thumb.fill(QColor(50, 50, 50));
            QPixmap iconPix = IconProvider::loadIcon(item->execPath(), item->displayName());
            if (!iconPix.isNull()) {
                QPainter pp(&thumb);
                int s = qMin(thumbW, thumbH) - 16;
                QPixmap scaled = iconPix.scaled(s, s, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                pp.drawPixmap((thumbW - scaled.width()) / 2, (thumbH - scaled.height()) / 2, scaled);
                pp.end();
            }
            QLabel *fallbackLabel = new QLabel(m_previewPopup);
            fallbackLabel->setPixmap(thumb);
            fallbackLabel->setFixedSize(thumbW, thumbH);
            fallbackLabel->move(tx, ty);
            fallbackLabel->setStyleSheet("background: transparent;");
            fallbackLabel->setCursor(Qt::PointingHandCursor);
            fallbackLabel->setProperty("previewHwnd", reinterpret_cast<qintptr>(wi.hwnd));
            fallbackLabel->installEventFilter(this);
            fallbackLabel->show();
        }

        thumbList.append(entry);
    }

    m_previewPopup->setProperty("dwmThumbList", thumbList);
}

bool WindowPreviewPanel::isPreviewObject(QObject *obj) const
{
    if (!m_previewPopup || !obj)
        return false;
    if (obj == m_previewPopup)
        return true;

    QWidget *widget = qobject_cast<QWidget *>(obj);
    return widget && m_previewPopup->isAncestorOf(widget);
}

bool WindowPreviewPanel::cursorInsidePreview() const
{
    if (!m_previewPopup || !m_previewPopup->isVisible())
        return false;

    return m_previewPopup->rect().contains(m_previewPopup->mapFromGlobal(QCursor::pos()));
}

// ─── DWM Peek Lite ───────────────────────────────────────

void WindowPreviewPanel::startPeek(HWND targetHwnd)
{
    if (!targetHwnd || !IsWindow(targetHwnd)) return;

    // 检查前台是否全屏，如是则跳过 peek
    HWND foregroundHwnd = GetForegroundWindow();
    if (foregroundHwnd && m_sysHelper) {
        RECT fgRect;
        if (m_sysHelper->getExtendedFrameBounds(foregroundHwnd, fgRect)) {
            int fgW = fgRect.right - fgRect.left;
            int fgH = fgRect.bottom - fgRect.top;
            int screenW = GetSystemMetrics(SM_CXSCREEN);
            int screenH = GetSystemMetrics(SM_CYSCREEN);
            if (fgW >= screenW && fgH >= screenH) return; // 全屏，跳过
        }
    }

    m_peekTarget = targetHwnd;
    SetWindowPos(targetHwnd, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
}

void WindowPreviewPanel::stopPeek()
{
    m_peekTarget = nullptr;
    // 不还原 Z-order（子方案 B）
}

// ─── 虚拟桌面检测 ───────────────────────────────────────

bool WindowPreviewPanel::isWindowOnCurrentDesktop(HWND hwnd)
{
    if (!hwnd) return true;

#ifdef Q_OS_WIN
    if (m_virtualDesktopManager) {
        BOOL onCurrent = FALSE;
        HRESULT hr = m_virtualDesktopManager->IsWindowOnCurrentVirtualDesktop(hwnd, &onCurrent);
        if (SUCCEEDED(hr)) {
            return onCurrent != FALSE;
        }
    }
#endif

    // Fallback: 无法检测时视为在当前桌面（不过滤）
    return true;
}

// ─── 事件过滤 ───────────────────────────────────────────

bool WindowPreviewPanel::eventFilter(QObject *obj, QEvent *event)
{
    // ── popup Leave/Enter（离开防抖）──
    if (isPreviewObject(obj) && event->type() == QEvent::Leave) {
        startDelayedHide();
        stopPeek();
        m_peekTimer->stop();
    } else if (isPreviewObject(obj) && event->type() == QEvent::Enter) {
        cancelDelayedHide();
    }

    // ── DWM Peek：鼠标在缩略图区域移动 ──
    if (event->type() == QEvent::MouseMove && obj == m_previewPopup && m_previewPopup) {
        QMouseEvent *me = static_cast<QMouseEvent *>(event);
        QVariant thumbListVar = m_previewPopup->property("dwmThumbList");
        if (thumbListVar.isValid()) {
            bool onThumb = false;
            const QVariantList list = thumbListVar.toList();
            for (const QVariant &entry : list) {
                QVariantMap map = entry.toMap();
                QRect rect(map["x"].toInt(), map["y"].toInt(),
                           map["w"].toInt(), map["h"].toInt());
                if (rect.contains(me->pos())) {
                    HWND srcHwnd = reinterpret_cast<HWND>(
                        static_cast<quintptr>(map["srcHwnd"].toULongLong()));
                    if (srcHwnd != m_peekTarget) {
                        stopPeek();
                        m_peekTimer->disconnect();
                        connect(m_peekTimer, &QTimer::timeout, this, [this, srcHwnd]() {
                            startPeek(srcHwnd);
                        });
                        m_peekTimer->start();
                    }
                    onThumb = true;
                    break;
                }
            }
            if (!onThumb) {
                stopPeek();
                m_peekTimer->stop();
            }
        }
    }

    // ── 点击缩略图激活窗口 ──
    if (event->type() == QEvent::MouseButtonPress) {
        HWND targetHwnd = nullptr;

        // 路径 1: 回退图标 QLabel（有 previewHwnd 属性）
        QWidget *w = qobject_cast<QWidget *>(obj);
        if (w) {
            QVariant v = w->property("previewHwnd");
            if (v.isValid()) {
                targetHwnd = reinterpret_cast<HWND>(v.value<quintptr>());
            }
        }

        // 路径 2: DWM 缩略图区域 — 点击 popup 本身，按坐标匹配缩略图矩形
        if (!targetHwnd && obj == m_previewPopup && m_previewPopup) {
            QMouseEvent *me = static_cast<QMouseEvent *>(event);
            QPoint pos = me->pos();
            QVariant thumbListVar = m_previewPopup->property("dwmThumbList");
            if (thumbListVar.isValid()) {
                const QVariantList list = thumbListVar.toList();
                for (const QVariant &entry : list) {
                    QVariantMap map = entry.toMap();
                    QRect rect(map["x"].toInt(), map["y"].toInt(),
                               map["w"].toInt(), map["h"].toInt());
                    if (rect.contains(pos)) {
                        targetHwnd = reinterpret_cast<HWND>(
                            static_cast<quintptr>(map["srcHwnd"].toULongLong()));
                        break;
                    }
                }
            }
        }

        if (targetHwnd && IsWindow(targetHwnd)) {
            hidePreview();
            AllowSetForegroundWindow(ASFW_ANY);
            SetForegroundWindow(targetHwnd);
            ShowWindow(targetHwnd, SW_RESTORE);
            SetFocus(targetHwnd);
            return true;
        }
    }
    return QObject::eventFilter(obj, event);
}
