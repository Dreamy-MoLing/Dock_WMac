/**
 * @file WindowPreviewPanel.cpp
 * @brief 窗口缩略图预览面板实现
 *
 * 从 DockWindow 提取 Win32 EnumWindows + PrintWindow 逻辑。
 */

#include "ui/WindowPreviewPanel.h"
#include "ui/DockItem.h"
#include "core/SysHelper.h"
#include "core/AppIdHelper.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QScreen>
#include <QGuiApplication>
#include <QFileInfo>
#include <QFileIconProvider>
#include <QPainter>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

WindowPreviewPanel::WindowPreviewPanel(QObject *parent)
    : QObject(parent)
    , m_previewTimer(new QTimer(this))
{
    m_previewTimer->setSingleShot(true);
    m_previewTimer->setInterval(500);
    connect(m_previewTimer, &QTimer::timeout,
            this, &WindowPreviewPanel::onPreviewTimerTimeout);
}

WindowPreviewPanel::~WindowPreviewPanel()
{
    clearContent();
}

void WindowPreviewPanel::setSysHelper(SysHelper *helper)
{
    m_sysHelper = helper;
}

void WindowPreviewPanel::showPreview(DockItem *item)
{
    if (!item || item == m_previewItem) return;

    hidePreview();
    m_previewItem = item;
    m_previewTimer->start();
}

void WindowPreviewPanel::hidePreview()
{
    m_previewTimer->stop();
    m_previewItem = nullptr;
    clearContent();
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

void WindowPreviewPanel::clearContent()
{
    if (m_previewPopup) {
        m_previewPopup->close();
        m_previewPopup->deleteLater();
        m_previewPopup = nullptr;
    }
}

void WindowPreviewPanel::buildPreviewContent(DockItem *item)
{
    if (!item || !m_sysHelper) return;

    QString wmClass = AppIdHelper::deriveWmClass(item->execPath(), item->appId());
    if (wmClass.isEmpty()) return;

    QString lowerClass = wmClass.toLower();

    // 枚举匹配进程的可见窗口
    struct WinInfo { HWND hwnd; QString title; };
    struct EnumCtx {
        QString targetClass;
        QList<WinInfo> windows;
    } ctx{lowerClass, {}};

    EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
        auto *ctx = reinterpret_cast<EnumCtx *>(lParam);
        if (!IsWindowVisible(hwnd) || IsIconic(hwnd)) return TRUE;

        LONG exStyle = GetWindowLong(hwnd, GWL_EXSTYLE);
        if (exStyle & WS_EX_TOOLWINDOW) return TRUE;

        wchar_t title[256] = {0};
        GetWindowTextW(hwnd, title, 255);
        if (wcslen(title) < 2) return TRUE;

        DWORD pid;
        GetWindowThreadProcessId(hwnd, &pid);
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (!hProcess) return TRUE;

        wchar_t exeName[MAX_PATH] = {0};
        DWORD size = MAX_PATH;
        BOOL matched = FALSE;
        if (QueryFullProcessImageNameW(hProcess, 0, exeName, &size)) {
            QString exe = QFileInfo(QString::fromWCharArray(exeName)).baseName().toLower();
            if (exe == ctx->targetClass) matched = TRUE;
        }
        CloseHandle(hProcess);
        if (!matched) return TRUE;

        ctx->windows.append({hwnd, QString::fromWCharArray(title)});
        return TRUE;
    }, reinterpret_cast<LPARAM>(&ctx));

    if (ctx.windows.isEmpty()) return;

    int maxPreviews = qMin(ctx.windows.size(), 6);

    clearContent();
    m_previewPopup = new QWidget(nullptr, Qt::Popup | Qt::FramelessWindowHint);
    m_previewPopup->setAttribute(Qt::WA_TranslucentBackground);
    m_previewPopup->setAttribute(Qt::WA_ShowWithoutActivating);

    int thumbW = 160, thumbH = 100, spacing = 8, padding = 10, titleH = 20;
    int popupW = maxPreviews * thumbW + (maxPreviews - 1) * spacing + 2 * padding;
    int popupH = thumbH + titleH + 2 * padding;
    m_previewPopup->setFixedSize(popupW, popupH);
    m_previewPopup->setStyleSheet("background: rgba(30, 30, 30, 220); border-radius: 10px;");

    for (int i = 0; i < maxPreviews; ++i) {
        const auto &wi = ctx.windows[i];

        QPixmap thumb(thumbW, thumbH);
        thumb.fill(QColor(50, 50, 50));
        bool captured = false;

        // PrintWindow 捕获缩略图
        DWORD_PTR result = 0;
        LRESULT checkOk = SendMessageTimeoutW(wi.hwnd, WM_NULL, 0, 0,
            SMTO_ABORTIFHUNG | SMTO_BLOCK, 80, &result);
        if (checkOk) {
            RECT rect;
            if (GetWindowRect(wi.hwnd, &rect)) {
                int winW = rect.right - rect.left;
                int winH = rect.bottom - rect.top;
                if (winW > 0 && winH > 0) {
                    int capW = qMin(winW, 640), capH = qMin(winH, 400);
                    HDC hdcWindow = GetDC(wi.hwnd);
                    HDC hdcMem = CreateCompatibleDC(hdcWindow);
                    HBITMAP hBitmap = CreateCompatibleBitmap(hdcWindow, capW, capH);
                    HBITMAP hOld = (HBITMAP)SelectObject(hdcMem, hBitmap);

                    if (PrintWindow(wi.hwnd, hdcMem, PW_CLIENTONLY)) {
                        BITMAPINFO bmi = {};
                        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                        bmi.bmiHeader.biWidth = capW;
                        bmi.bmiHeader.biHeight = -capH;
                        bmi.bmiHeader.biPlanes = 1;
                        bmi.bmiHeader.biBitCount = 32;
                        bmi.bmiHeader.biCompression = BI_RGB;

                        QImage img(capW, capH, QImage::Format_ARGB32);
                        GetDIBits(hdcMem, hBitmap, 0, capH, img.bits(), &bmi, DIB_RGB_COLORS);
                        thumb = QPixmap::fromImage(img).scaled(thumbW, thumbH,
                            Qt::KeepAspectRatio, Qt::SmoothTransformation);
                        captured = true;

                        if (thumb.width() < thumbW || thumb.height() < thumbH) {
                            QPixmap centered(thumbW, thumbH);
                            centered.fill(QColor(50, 50, 50));
                            QPainter pp(&centered);
                            pp.drawPixmap((thumbW - thumb.width()) / 2,
                                          (thumbH - thumb.height()) / 2, thumb);
                            pp.end();
                            thumb = centered;
                        }
                    }
                    SelectObject(hdcMem, hOld);
                    DeleteObject(hBitmap);
                    DeleteDC(hdcMem);
                    ReleaseDC(wi.hwnd, hdcWindow);
                }
            }
        }

        if (!captured && !item->execPath().isEmpty()) {
            QFileIconProvider provider;
            QIcon appIcon = provider.icon(QFileInfo(item->execPath()));
            if (!appIcon.isNull()) {
                QPixmap iconPix = appIcon.pixmap(48, 48);
                QPainter pp(&thumb);
                pp.drawPixmap((thumbW - 48) / 2, (thumbH - 48) / 2, iconPix);
                pp.end();
            }
        }

        QLabel *thumbLabel = new QLabel(m_previewPopup);
        thumbLabel->setPixmap(thumb);
        thumbLabel->setFixedSize(thumbW, thumbH);
        thumbLabel->setStyleSheet("background: transparent;");
        thumbLabel->move(padding + i * (thumbW + spacing), padding);

        QLabel *titleLabel = new QLabel(m_previewPopup);
        QString elidedTitle = wi.title;
        if (elidedTitle.length() > 20)
            elidedTitle = elidedTitle.left(19) + "...";
        titleLabel->setText(elidedTitle);
        titleLabel->setFixedSize(thumbW, titleH);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("background: transparent; color: white; font-size: 11px;");
        titleLabel->move(padding + i * (thumbW + spacing), padding + thumbH + 4);

        HWND targetHwnd = wi.hwnd;
        thumbLabel->setCursor(Qt::PointingHandCursor);
        thumbLabel->installEventFilter(this);
        thumbLabel->setProperty("previewHwnd", reinterpret_cast<qintptr>(targetHwnd));
    }

    // 定位弹出面板：在 dock 窗口上方居中
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
    m_previewPopup->show();
}

bool WindowPreviewPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QWidget *w = qobject_cast<QWidget *>(obj);
        if (w) {
            QVariant v = w->property("previewHwnd");
            if (v.isValid()) {
                HWND hwnd = reinterpret_cast<HWND>(v.value<qintptr>());
                if (hwnd && IsWindow(hwnd)) {
                    hidePreview();
                    AllowSetForegroundWindow(ASFW_ANY);
                    SetForegroundWindow(hwnd);
                    ShowWindow(hwnd, SW_RESTORE);
                    SetFocus(hwnd);
                }
                return true;
            }
        }
    }
    return QObject::eventFilter(obj, event);
}
