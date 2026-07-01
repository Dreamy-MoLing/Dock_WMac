# Code Review: Dock_WMac

## 数据竞争 / 设计矛盾

### DockWindow::enterEvent 是伪逻辑
`DockWindow::enterEvent` 里写「Hidden 状态唤醒 dock」，但 dock `hide()` 后 mouse event 根本到不了这个 widget。Hidden 的实际唤醒靠 `m_bottomEdgeTimer`（200ms 轮询鼠标位置），enterEvent 这段永远跑不到。死代码，删。

### updateWindowCounts 每 2 秒全量 EnumWindows
`m_windowCountTimer` → `m_windowCache->refresh()`，每 2 秒全量枚举一次所有窗口。WinEvent 已经提供增量驱动，轮询完全多余。用户全屏打游戏时还在跑这个。删掉轮询只靠事件，或间隔拉到 10s+。

### hideNativeTaskbar 漏了副屏任务栏
只 `FindWindow(L"Shell_TrayWnd")`。Windows 10+ 多显示器还有 `Shell_SecondaryTrayWnd`。

## 代码异味

### .lnk 解析代码复制了两份
`IconProvider::resolveShortcut()` 和 `PinnedItemsReader::resolveShortcut()` 是完全一样的 COM IShellLink 解析代码，整块复制粘贴。抽到公共模块。

### ClickStateMachine 和 WindowCache 跳过 SysHelper 直接调 Win32 API
两个文件都 `#include <windows.h>` 直接调 `ShowWindow`/`SetForegroundWindow`。设计说「SysHelper 是唯一系统层」，实际系统调用散落在三个文件。架构边界没落地。

### WindowCache::showWindowPicker 用废弃 API
`keybd_event()` 模拟 Win+Tab（MSDN 标记 deprecated），应改用 `SendInput()`。且 Win+Tab 在安全软件/游戏模式下可能被拦截。

### ConfigManager 每次 set() 都写盘
`set()` → 直接 `save()` → 写 JSON。用户连续调整多个配置时每改一项刷一次盘。加 debounce timer。

## 边界/鲁棒性

### UWP 路径过滤写错了
`ProcessMonitor::scanTransientApps`：`lower.contains("\\\\windowsapp\\\\")`，真实 UWP 目录是 `C:\Program Files\WindowsApps\`（带 s）。少了个 s，过滤条件形同虚设。

### DWM Peek 用 SetWindowPos(HWND_TOP) 太粗暴
直接把目标窗口提到 Z-order 最顶层，不是真正 peek（临时显示不破坏交互）。在全屏看视频时预览聊天窗口会把聊天窗口怼到最前面。

### OverflowPanel 弹窗没有 Windows 11 圆角
`WindowPreviewPanel` 有 `DwmSetWindowAttribute(DWMWA_WINDOW_CORNER_PREFERENCE)`，溢出菜单的 popup 没有，风格不一致。

## 测试覆盖缺口

现有的 8 个测试覆盖了 core 层，缺了：
- `test_icon_provider.cpp` — 5 级回退链各边界条件
- `test_dock_item.cpp` — paint 逻辑（运行指示灯、通知闪烁、窗口堆叠）
- `test_dock_window.cpp` — UI 事件流
- `test_window_preview_panel.cpp` — DWM 缩略图注册/注销/清理

## 低优先级

- `nativeEvent` 里的 `xcb_generic_event_t` 分支，Windows-only 项目，死代码
- `DockWindow::onItemAdded` 里 `relayoutItems()` 后又调了 `updatePosition()`，但 relayoutItems 内部已调了一次，重复
- `ConfigManager` 注释写 `%APPDATA%/Dock_WMac/`，实际走便携模式 `./data/`，注释过期
- DWM 缩略图指针 `HTHUMBNAIL` 通过 `QVariant::setProperty` 存为 `qulonglong`，类型不安全，应改用 `QHash` 管理生命周期
