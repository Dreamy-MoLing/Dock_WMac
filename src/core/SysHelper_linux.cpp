/**
 * @file SysHelper_linux.cpp
 * @brief Linux 平台系统适配实现
 *
 * 通过 .desktop 文件读取常用应用列表，使用 QIcon::fromTheme 提取图标，
 * 通过 Xlib 检测前台窗口状态，定时器轮询窗口变化。
 */

#include "core/SysHelper.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QIcon>
#include <QPixmap>
#include <QTimer>
#include <QProcess>
#include <QDebug>
#include <QRegularExpression>

#include <X11/Xlib.h>
#include <X11/Xatom.h>

// .desktop 文件搜索路径
static const QStringList kDesktopPaths = {
    QDir::homePath() + "/.local/share/applications",
    "/usr/share/applications",
    "/usr/local/share/applications"
};

// 默认推荐应用（当无固定项时显示）
static const QStringList kDefaultApps = {
    "org.gnome.Nautilus",       // 文件管理器
    "firefox",                  // 浏览器
    "org.gnome.Terminal",       // 终端
    "org.gnome.TextEditor",     // 文本编辑器
    "org.gnome.Calculator"      // 计算器
};

/**
 * @brief 解析 .desktop 文件，返回 DockItemData
 */
static DockItemData parseDesktopFile(const QString &filePath)
{
    QSettings desktop(filePath, QSettings::IniFormat);
    desktop.setIniCodec("UTF-8");

    desktop.beginGroup("Desktop Entry");

    DockItemData item;
    item.appId = QFileInfo(filePath).completeBaseName();
    item.displayName = desktop.value("Name").toString();
    item.execPath = desktop.value("Exec").toString();
    // 移除 Exec 中的 %f %F %u %U 等参数占位符
    item.execPath.replace(QRegularExpression("\\s+%[fFuU]"), "");
    item.isRunning = false;
    item.badgeCount = 0;

    // 图标：优先使用 Icon 字段（主题图标名或绝对路径）
    QString iconName = desktop.value("Icon").toString();
    if (QFileInfo::exists(iconName)) {
        item.iconPath = iconName;
    } else {
        // 使用 QIcon::fromTheme 查找，iconName 作为主题图标名
        item.iconPath = iconName;  // DockItem 会通过 fromTheme 加载
    }

    return item;
}

/**
 * @brief 检查 .desktop 文件是否应在 Dock 中显示
 */
static bool isValidDesktopEntry(QSettings &desktop)
{
    desktop.beginGroup("Desktop Entry");

    // NoDisplay / Hidden 的不显示
    if (desktop.value("NoDisplay", false).toBool()) return false;
    if (desktop.value("Hidden", false).toBool()) return false;

    // 必须是 Application 类型
    QString type = desktop.value("Type").toString();
    if (type != "Application") return false;

    // 必须有 Exec
    if (!desktop.value("Exec").isValid()) return false;

    return true;
}

QList<DockItemData> SysHelper::getPinnedItems()
{
    QList<DockItemData> items;
    QStringList seen;

    for (const QString &dirPath : kDesktopPaths) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;

        // 优先读取标记了 X-Dock-Pinned 的应用，其次读取默认推荐
        const auto entries = dir.entryInfoList({"*.desktop"}, QDir::Files);
        for (const QFileInfo &fi : entries) {
            QSettings desktop(fi.absoluteFilePath(), QSettings::IniFormat);
            desktop.setIniCodec("UTF-8");

            if (!isValidDesktopEntry(desktop)) continue;

            // 检查是否标记为固定或在默认列表中
            desktop.beginGroup("Desktop Entry");
            bool pinned = desktop.value("X-Dock-Pinned", false).toBool();
            QString baseName = fi.completeBaseName();

            if (!pinned && !kDefaultApps.contains(baseName)) continue;

            if (!seen.contains(baseName)) {
                DockItemData item = parseDesktopFile(fi.absoluteFilePath());
                if (!item.displayName.isEmpty()) {
                    items.append(item);
                    seen.append(baseName);
                }
            }
        }
    }

    // 如果没有找到任何固定项，使用硬编码默认列表
    if (items.isEmpty()) {
        for (const QString &appId : kDefaultApps) {
            for (const QString &dirPath : kDesktopPaths) {
                QString filePath = dirPath + "/" + appId + ".desktop";
                if (QFileInfo::exists(filePath)) {
                    QSettings desktop(filePath, QSettings::IniFormat);
                    desktop.setIniCodec("UTF-8");
                    if (isValidDesktopEntry(desktop)) {
                        items.append(parseDesktopFile(filePath));
                        break;
                    }
                }
            }
        }
    }

    return items;
}

QString SysHelper::extractAppIcon(const QString &appId)
{
    // 尝试从主题加载图标
    QIcon icon = QIcon::fromTheme(appId);
    if (!icon.isNull()) {
        // 保存到临时缓存路径
        QPixmap pix = icon.pixmap(64, 64);
        if (!pix.isNull()) {
            QString cachePath = QDir::tempPath() + "/dock_wmac_icons/" + appId + ".png";
            QDir().mkpath(QFileInfo(cachePath).absolutePath());
            pix.save(cachePath, "PNG");
            return cachePath;
        }
    }
    return {};
}

/**
 * @brief 通过 Xlib 获取前台窗口的窗口管理器状态
 *
 * 读取 _NET_WM_STATE 属性，检查是否有最大化或全屏状态原子。
 */
bool SysHelper::getForegroundWindowState()
{
    Display *display = XOpenDisplay(nullptr);
    if (!display) return false;

    // 获取前台窗口
    Atom netActiveWindow = XInternAtom(display, "_NET_ACTIVE_WINDOW", True);
    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    unsigned char *prop = nullptr;

    Window root = DefaultRootWindow(display);
    if (XGetWindowProperty(display, root, netActiveWindow, 0, 1, False,
                           XA_WINDOW, &actualType, &actualFormat,
                           &nItems, &bytesAfter, &prop) != Success || !prop) {
        XCloseDisplay(display);
        return false;
    }

    Window activeWindow = *reinterpret_cast<Window *>(prop);
    XFree(prop);

    if (activeWindow == None) {
        XCloseDisplay(display);
        return false;
    }

    // 读取 _NET_WM_STATE
    Atom netWmState = XInternAtom(display, "_NET_WM_STATE", True);
    Atom netMaximizedH = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", True);
    Atom netMaximizedV = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", True);
    Atom netFullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", True);

    bool maximized = false;
    bool fullscreen = false;

    if (XGetWindowProperty(display, activeWindow, netWmState, 0, 1024, False,
                           XA_ATOM, &actualType, &actualFormat,
                           &nItems, &bytesAfter, &prop) == Success && prop) {
        Atom *states = reinterpret_cast<Atom *>(prop);
        for (unsigned long i = 0; i < nItems; ++i) {
            if (states[i] == netMaximizedH || states[i] == netMaximizedV) {
                maximized = true;
            }
            if (states[i] == netFullscreen) {
                fullscreen = true;
            }
        }
        XFree(prop);
    }

    XCloseDisplay(display);
    return maximized || fullscreen;
}

bool SysHelper::installWindowHook()
{
    // Linux 使用定时器轮询窗口状态变化（每 500ms 检查一次）
    static QTimer *s_timer = nullptr;
    if (s_timer) return true;  // 已安装

    s_timer = new QTimer(this);
    connect(s_timer, &QTimer::timeout, this, [this]() {
        bool maximized = getForegroundWindowState();
        emit foregroundWindowChanged(maximized);
    });
    s_timer->start(500);
    return true;
}

bool SysHelper::installKeyboardHook()
{
    // Linux 下全局键盘钩子需要 root 权限或 XInput2
    // 此处为存根实现，后续可通过 XGrabKey 实现
    qWarning() << "SysHelper_linux: 全局键盘钩子暂未实现";
    return false;
}

void SysHelper::uninstallKeyboardHook()
{
    // 存根
}
