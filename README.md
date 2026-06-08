# Dock_WMac

> macOS 风格应用停靠栏 — 安装即用，一次启动，永久陪伴。

[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)](https://github.com/Dreamy-MoLing/Dock_WMac)
[![Version](https://img.shields.io/badge/version-0.2.3-brightgreen)](https://github.com/Dreamy-MoLing/Dock_WMac/releases)

Dock_WMac 是 Windows 桌面上的 macOS 风格停靠栏（Dock），**替代原生任务栏**。它不是一个需要频繁关注的桌面应用，而是一个**安装即忘的插件** —— 安装一次，开机自启，然后你就可以忘记它的存在。

---

## 目录

- [功能特性](#功能特性)
- [安装](#安装)
- [卸载](#卸载)
- [架构](#架构)
- [开发](#开发)
- [许可](#许可)

---

## 功能特性

### 核心体验

| 特性 | 说明 |
|------|------|
| 🧲 **鱼眼放大** | 鼠标悬停时图标等比放大动画（`QPropertyAnimation`），邻近图标连带放大 |
| 🙈 **自动隐藏** | 无操作时自动滑出屏幕边缘，鼠标触底唤醒 |
| 🔍 **窗口预览** | 悬停 500ms 弹出 DWM 实时缩略图，再悬停触发 Peek 置顶预览 |
| 🌓 **亮暗主题** | 每 5 秒检测系统主题色，图标和背景自动跟随切换 |
| 🧊 **毛玻璃模糊** | 全窗口半透明 + 圆角 DWM 模糊背景 |

### 交互

| 特性 | 说明 |
|------|------|
| 🖱️ **智能点击** | 5 状态点击机：无窗口→启动 / 后台→显示 / 最小化→恢复 / 前台→最小化 / 可见→置前 |
| 🔄 **双击启动** | 双击图标强制启动新实例 |
| ↔️ **拖拽排序** | 拖拽图标调整固定项顺序，实时保存 |
| 📋 **溢出面板** | 超过上限（默认 16 个）的图标进入 "…" 弹出菜单 |
| 🔴 **运行指示器** | 图标下方绿色圆点标记当前运行的窗口 |
| 🔢 **角标** | 红色角标显示未读计数 |

### 设计哲学

- **低资源占用** — Win32 事件驱动 + `QReadWriteLock` 线程安全缓存，非轮询密集
- **静默运行** — 无弹窗、无通知、无打扰。开机自启，安静工作
- **用户数据隔离** — 所有数据（配置、日志、图标缓存）存储在用户目录，无需管理员权限

---

## 安装

### 1. 下载安装包

从 [Releases](https://github.com/Dreamy-MoLing/Dock_WMac/releases) 下载最新 `Dock_WMac_Setup_v*.exe`。

### 2. 运行安装程序

双击安装包，选择安装目录（默认 `%LOCALAPPDATA%\Programs\Dock_WMac\`），一路下一步即可。

> **注意事项：**
> - 安装包**不需要管理员权限**，所有文件安装在用户目录下
> - 安装完成后会自动启动，同时注册开机自启
> - **不会创建桌面快捷方式** — 你不需要看到它，它一直在

### 3. 安装过程做了什么

| 步骤 | 说明 |
|------|------|
| 拷贝文件 | 将 `Mac任务栏.exe`、Qt 运行时 DLL、`uninstall.exe` 安装到目标目录 |
| 开始菜单 | 创建 "Dock_WMac" 程序组（含启动快捷方式和卸载入口） |
| 注册表 | 写入 `HKCU\...\Run\Dock_WMac` 实现开机自启 |

### 4. 安装后目录结构

```
%LOCALAPPDATA%\Programs\Dock_WMac\
├── Mac任务栏.exe          ← 主程序
├── uninstall.exe          ← 安全卸载器（独立，无 Qt 依赖）
├── Qt6Core.dll            ← Qt 运行时
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── Qt6Svg.dll
├── platforms\             ← Qt 平台插件
├── styles\                ← Qt 样式插件
├── unins000.exe           ← Inno Setup 卸载程序
└── unins000.dat
```

---

## 卸载

我们相信 **"一根毛都不留"** 的卸载体验。

### 方式一：开始菜单（推荐）

打开开始菜单 → `Dock_WMac` → `卸载 Mac任务栏`

### 方式二：Windows 设置

设置 → 应用 → 已安装的应用 → 搜索 "Mac任务栏" → 卸载

### 卸载流程做了什么

卸载分三步执行，确保彻底无残留：

**第 1 步：终止进程** — `taskkill` 结束运行中的 `Mac任务栏.exe`

**第 2 步：安全卸载器** — `uninstall.exe /SILENT` 清理所有运行时代数据：

| 清理目标 | 路径 | 来源 |
|---------|------|------|
| 配置文件 | `%APPDATA%\Dock_WMac\` | `config.json`，固定项列表 |
| 日志文件 | `%LOCALAPPDATA%\Dock_WMac\` | `dock.log`，运行日志 |
| 图标缓存 | `%TEMP%\dock_wmac_icons\` | 从系统图标提取的 PNG 副本（非原始图标） |
| 注册表 | `HKCU\...\Run\Dock_WMac` | 开机自启键 |

**第 3 步：删除安装目录** — Inno Setup 移除 `%LOCALAPPDATA%\Programs\Dock_WMac\` 及自身

### 安全保证

`uninstall.exe` 是独立于主程序的**纯 Win32 安全卸载器**（无 Qt 依赖，源码 ~370 行）。每条删除路径经过**三层安全验证**，任一失败即拒绝删除：

1. **目录名白名单** — 必须是 `Dock_WMac` 或 `dock_wmac_icons`
2. **父目录匹配** — 父目录必须与系统路径一致（如 `%APPDATA%`、`%LOCALAPPDATA%`、`%TEMP%`）
3. **符号链接检测** — 拒绝删除 junction / symlink（防止重定向攻击）

所有操作日志写入 `%TEMP%\Dock_WMac_uninstall.log`，可随时审计。

---

## 架构

### 三层设计

```
┌──────────────────────────────────────┐
│  UI 层 (Qt6)                         │
│  DockWindow · DockItem               │
│  WindowPreviewPanel · OverflowPanel  │
├──────────────────────────────────────┤
│  Core 逻辑层                          │
│  Application · DockManager           │
│  ConfigManager · ProcessMonitor      │
│  WindowCache · ClickStateMachine     │
│  IconProvider · AppIdHelper          │
│  PinnedItemsReader · Logger          │
├──────────────────────────────────────┤
│  System 系统层                        │
│  SysHelper (Win32 + COM + DWM)       │
└──────────────────────────────────────┘
```

### UI 层（`src/ui/`）

| 组件 | 职责 |
|------|------|
| **DockWindow** | 无边框置顶主窗口。手动布局 + 鱼眼放大动画，单/双击事件分发，拖拽排序，右键菜单 |
| **DockItem** | 单个图标控件。图标渲染、运行指示器（绿点）、红色角标、窗口堆叠指示器、前景激活指示器 |
| **WindowPreviewPanel** | 悬停预览面板。500ms 延迟弹出，`DwmRegisterThumbnail` 实时缩略图，300ms Peek 置顶，200ms 离开防抖 |
| **OverflowPanel** | 超限弹出菜单。超过 `maxItems` 上限的图标进入"…"按钮的上弹面板 |

### Core 层（`src/core/`）

| 组件 | 职责 |
|------|------|
| **Application** | 应用生命周期管理。初始化顺序：日志 → 配置 → SysHelper → WindowCache → DockManager → UI → ProcessMonitor。退出时逆序清理，恢复原生任务栏 |
| **DockManager** | 核心状态机（`Docked` / `Hidden`）。管理固定项和瞬态项，全屏/最大化检测联动 |
| **ConfigManager** | JSON 配置文件读写（`%APPDATA%\Dock_WMac\config.json`），LRU 图标缓存（128 条） |
| **ProcessMonitor** | `CreateToolhelp32Snapshot` 每 2s 轮询进程状态，每 4 次扫描新窗口化应用 |
| **WindowCache** | 全量 `EnumWindows` 缓存 + WinEvent 增量更新。`QReadWriteLock` 线程安全，三档刷新（WinEvent → 2s 轮询 → 同步 `scanForClass`） |
| **ClickStateMachine** | 5 状态点击决策：NoWindows / BackgroundRunning / AllMinimized / ForegroundActive / BackgroundVisible |
| **IconProvider** | 6 级 Win32 图标回退链：文件路径 → UWP AUMID → `ExtractIconEx` → `.lnk` IShellLink → `SHGetImageList` → 字母占位图 |
| **AppIdHelper** | 进程名推导：`execPath` 文件名 → AppId 末段，用于 Win32 窗口类匹配 |
| **PinnedItemsReader** | 读取系统任务栏固定项（COM `IShellLink`），导出图标到 `%TEMP%\dock_wmac_icons\` |
| **Logger** | `qInstallMessageHandler` 日志系统，写入 `%LOCALAPPDATA%\Dock_WMac\dock.log`，5MB 自动轮转 |

### System 层（`src/core/SysHelper.cpp`）

| 功能 | 实现 |
|------|------|
| 窗口事件钩子 | `SetWinEventHook` → `windowEventOccurred(DWORD pid)` 信号 |
| 键盘钩子 | `WH_KEYBOARD_LL` Win 键触发 Dock 显示 |
| DWM 模糊 | `DwmEnableBlurBehindWindow`（全窗口 / 区域 + 圆角） |
| 全屏检测 | 桌面 bounds 与前景窗口 rect 对比（10px 容差） |
| 主题检测 | 注册表 `AppsUseLightTheme` / `SystemUsesLightTheme` |
| 任务栏控制 | `FindWindow("Shell_TrayWnd")` 隐藏/恢复 |
| 开机自启 | 注册表 `HKCU\...\Run` 读写 |

---

## 开发

### 环境要求

| 组件 | 要求 |
|------|------|
| 操作系统 | Windows 10 / 11 |
| 编译器 | Visual Studio 2022（MSVC v143） |
| Qt | 6.8.0+（MSVC 2022 64-bit） |
| CMake | 3.20+ |
| 构建工具 | Ninja |

### 快速构建

从 **VS Developer Command Prompt** 运行：

```cmd
# 配置并构建
cmake --preset default
cmake --build build --config Release

# 产物
#   build\Mac任务栏.exe     — 主程序
#   build\uninstall.exe     — 安全卸载器
```

### 运行测试

```cmd
cmake --preset default -DBUILD_TESTS=ON
cmake --build build --config Release
cd build && ctest -C Release
```

8 套测试：

```
test_config              — 配置管理器
test_dock_manager        — Dock 状态机
test_process_monitor     — 进程监控
test_sys_helper          — 系统层
test_pinned_items_reader — 固定项读取
test_application         — 应用生命周期
test_window_cache        — 窗口缓存
test_click_state_machine — 点击状态机
```

### 构建安装包（本地）

```cmd
# 1. 编译
cmake --build build --config Release

# 2. 部署 Qt 运行时
windeployqt --no-translations --no-opengl-sw --dir deploy build\Mac任务栏.exe

# 3. 复制卸载器
copy build\uninstall.exe deploy\

# 4. 构建安装包（需安装 Inno Setup）
iscc installer\setup.iss

# 产物: dist\Dock_WMac_Setup_v0.2.3.exe
```

### 项目结构

```
Dock_WMac/
├── include/
│   ├── core/              # 核心逻辑头（12 文件）
│   └── ui/                # UI 组件头（4 文件）
├── src/
│   ├── core/              # 核心实现（11 文件）
│   ├── ui/                # UI 实现（4 文件）
│   ├── uninstall/         # 安全卸载器（1 文件，纯 Win32）
│   └── main.cpp           # 入口
├── resources/             # .qrc 资源 + app.rc 图标
├── installer/             # Inno Setup 安装脚本
├── tests/                 # Google Test 单元测试（8 套）
├── .github/workflows/     # CI/CD（MSVC + Qt 6.8.2）
├── CMakePresets.json      # CMake 预设
└── CMakeLists.txt         # CMake 构建定义
```

### 技术栈

| 依赖 | 用途 |
|------|------|
| Qt6::Widgets | UI 框架 |
| Qt6::Svg | SVG 图标渲染 |
| dwmapi | DWM 模糊 + 缩略图 |
| shell32 | COM Shell（图标、`.lnk` 解析） |
| shlwapi | 注册表操作 |
| user32 | Win32 窗口 API、钩子 |
| Google Test v1.14.0 | 单元测试（FetchContent 自动下载） |

### 设计决策

- **无桌面快捷方式** — 设计哲学是"安装即忘的插件"，不是需要点击启动的应用
- **纯英文安装路径** — 避免 Windows 路径编码冲突
- **独立卸载器** — `uninstall.exe` 不依赖 Qt，仅链接 `user32`/`shell32`/`advapi32`，最大化可审计性
- **用户级安装** — 不需要管理员权限，所有文件在 `%LOCALAPPDATA%`

---

## 许可

[MIT](LICENSE)
