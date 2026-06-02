/**
 * @file SysHelper_linux.cpp
 * @brief Linux 平台系统适配实现
 *
 * 通过 .desktop 文件读取常用应用列表，使用 QIcon::fromTheme 提取图标，
 * 通过 Xlib 检测前台窗口状态，定时器轮询窗口变化。
 */

#include "core/SysHelper.h"
#include "core/IPCHelper.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QIcon>
#include <QPixmap>
#include <QTimer>
#include <QProcess>
#include <QDebug>
#include <QRegularExpression>
#include <QCoreApplication>

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
    // IPC 后端优先
    if (m_ipcHelper) {
        QJsonObject resp = m_ipcHelper->scanDesktopFiles();
        if (resp.value("status").toString() == "ok") {
            QList<DockItemData> items;
            QJsonArray arr = resp.value("data").toObject().value("items").toArray();
            for (const QJsonValue &val : arr) {
                QJsonObject obj = val.toObject();
                DockItemData item;
                item.appId = obj["appId"].toString();
                item.displayName = obj["displayName"].toString();
                item.execPath = obj["execPath"].toString();
                item.iconPath = obj["iconPath"].toString();
                item.isRunning = false;
                item.badgeCount = 0;
                items.append(item);
            }
            return items;
        }
    }

    // 回退：传统文件解析
    QList<DockItemData> items;
    QStringList seen;

    for (const QString &dirPath : kDesktopPaths) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;

        // 优先读取标记了 X-Dock-Pinned 的应用，其次读取默认推荐
        const auto entries = dir.entryInfoList({"*.desktop"}, QDir::Files);
        for (const QFileInfo &fi : entries) {
            QSettings desktop(fi.absoluteFilePath(), QSettings::IniFormat);

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

bool SysHelper::setAutoStart(bool enabled)
{
    // Linux: 写入 ~/.config/autostart/dock-wmac.desktop
    QString autostartDir = QDir::homePath() + "/.config/autostart";
    QString desktopPath = autostartDir + "/dock-wmac.desktop";

    if (!enabled) {
        // 移除自启文件
        return QFile::remove(desktopPath);
    }

    // 创建自启目录
    QDir().mkpath(autostartDir);

    // 获取当前可执行文件路径
    QString execPath = QCoreApplication::applicationFilePath();

    // 写入 .desktop 文件
    QString content = QStringLiteral(
        "[Desktop Entry]\n"
        "Type=Application\n"
        "Name=Dock_WMac\n"
        "Comment=macOS 风格桌面 Dock 栏\n"
        "Exec=%1\n"
        "Hidden=false\n"
        "NoDisplay=false\n"
        "X-GNOME-Autostart-enabled=true\n"
    ).arg(execPath);

    QFile file(desktopPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "无法写入自启文件:" << desktopPath;
        return false;
    }
    file.write(content.toUtf8());
    file.close();

    qInfo() << "已设置开机自启:" << desktopPath;
    return true;
}

bool SysHelper::isAutoStartEnabled() const
{
    QString desktopPath = QDir::homePath() + "/.config/autostart/dock-wmac.desktop";
    return QFile::exists(desktopPath);
}

// ─── 窗口管理 ──────────────────────────────────────────────

/**
 * @brief 通过 Xlib 遍历所有窗口，按 WM_CLASS 匹配计数
 */
static QList<Window> findWindowsByClass(Display *display, const QString &wmClass)
{
    QList<Window> result;
    Window root = DefaultRootWindow(display);

    Atom netClientList = XInternAtom(display, "_NET_CLIENT_LIST", True);
    if (netClientList == None) return result;

    Atom actualType;
    int actualFormat;
    unsigned long nItems, bytesAfter;
    unsigned char *prop = nullptr;

    if (XGetWindowProperty(display, root, netClientList, 0, 1024, False,
                           XA_WINDOW, &actualType, &actualFormat,
                           &nItems, &bytesAfter, &prop) != Success || !prop) {
        return result;
    }

    Window *windows = reinterpret_cast<Window *>(prop);
    Atom wmClassAtom = XInternAtom(display, "WM_CLASS", True);

    for (unsigned long i = 0; i < nItems; ++i) {
        unsigned char *classProp = nullptr;
        if (XGetWindowProperty(display, windows[i], wmClassAtom, 0, 1024, False,
                               XA_STRING, &actualType, &actualFormat,
                               &nItems, &bytesAfter, &classProp) == Success && classProp) {
            // WM_CLASS 包含两个 null 分隔的字符串：实例名和类名
            QString instanceName = QString::fromUtf8(reinterpret_cast<char *>(classProp));
            QString className;
            int len = strlen(reinterpret_cast<char *>(classProp));
            if (len + 1 < (int)nItems) {
                className = QString::fromUtf8(reinterpret_cast<char *>(classProp) + len + 1);
            }
            // 匹配类名或实例名（不区分大小写）
            if (className.toLower() == wmClass.toLower() ||
                instanceName.toLower() == wmClass.toLower()) {
                result.append(windows[i]);
            }
            XFree(classProp);
        }
    }

    XFree(prop);
    return result;
}

int SysHelper::getWindowCount(const QString &wmClass)
{
    Display *display = XOpenDisplay(nullptr);
    if (!display) return 0;

    QList<Window> windows = findWindowsByClass(display, wmClass);
    XCloseDisplay(display);
    return windows.size();
}

bool SysHelper::activateWindow(const QString &wmClass)
{
    Display *display = XOpenDisplay(nullptr);
    if (!display) return false;

    QList<Window> windows = findWindowsByClass(display, wmClass);
    if (windows.isEmpty()) {
        XCloseDisplay(display);
        return false;
    }

    Window target = windows.first();
    Window root = DefaultRootWindow(display);

    // 发送 _NET_ACTIVE_WINDOW 请求
    Atom netActiveWindow = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    XEvent event;
    memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.serial = 0;
    event.xclient.send_event = True;
    event.xclient.display = display;
    event.xclient.window = target;
    event.xclient.message_type = netActiveWindow;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 2;  // 来自任务栏
    event.xclient.data.l[1] = CurrentTime;

    XSendEvent(display, root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XFlush(display);
    XCloseDisplay(display);
    return true;
}

void SysHelper::showWindowPicker()
{
    // GNOME：通过 dbus 触发 Activities 概览
    QProcess::startDetached("dbus-send", {
        "--session", "--type=method_call", "--dest=org.gnome.Shell",
        "/org/gnome/Shell", "org.gnome.Shell.Eval",
        "string:Main.overview.toggle();"
    });
}

// ─── Linux 存根：DWM 效果 / 任务栏管理（仅 Windows 实现） ────

void SysHelper::enableBlurBehindWindow(WId)
{
    // Linux 下无 DWM，使用 Qt 原生半透明背景即可
}

bool SysHelper::isBlurSupported() const
{
    return false;  // Linux 下不使用 DWM 模糊
}

bool SysHelper::isLightTheme() const
{
    // 检测 GTK 主题偏好（简化判断）
    QProcess proc;
    proc.start("gsettings", {"get", "org.gnome.desktop.interface", "color-scheme"});
    proc.waitForFinished(1000);
    QString scheme = proc.readAllStandardOutput().trimmed().toLower();
    return !scheme.contains("dark");
}

void SysHelper::hideNativeTaskbar()
{
    // Linux 下不隐藏系统面板（各桌面环境差异大）
}

void SysHelper::restoreNativeTaskbar()
{
    // Linux 下无操作
}
