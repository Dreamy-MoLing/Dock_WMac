# Dock_WMac

> macOS 风格应用停靠栏 — 安装即用，一次启动，永久陪伴。

[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)](https://github.com/Dreamy-MoLing/Dock_WMac)
[![Version](https://img.shields.io/badge/version-0.2.4-brightgreen)](https://github.com/Dreamy-MoLing/Dock_WMac/releases)

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
- **便携模式** — 所有数据（配置、日志、固定项）存储在程序目录 `./data/`，无需安装、无需管理员权限

---

## 使用

### 便携模式 — 下载即用

Dock_WMac v0.2.4+ 是**纯便携应用**。无需安装，解压即用。

1. 从 [Releases](https://github.com/Dreamy-MoLing/Dock_WMac/releases) 下载最新 `dock_wmac_v*.zip`
2. 解压到任意目录（建议固定的英文路径，如 `D:\Tools\Dock_WMac\`）
3. 双击 `Mac任务栏.exe`

首次运行时，程序会自动读取系统任务栏的固定项作为初始 Dock 内容。

### 目录结构

```
Dock_WMac/
├── Mac任务栏.exe          ← 主程序
├── Qt6Core.dll            ← Qt 运行时
├── Qt6Gui.dll
├── Qt6Widgets.dll
├── Qt6Svg.dll
├── platforms/             ← Qt 平台插件
├── styles/                ← Qt 样式插件
└── data/                  ← 运行数据（自动创建）
    ├── config.json         ← 用户配置
    ├── pinned.json         ← 用户固定项
    └── dock.log            ← 运行日志
```

### 卸载

**删除整个文件夹即可。** 一根毛都不留。

如果设置了开机自启，先在 Dock 右键菜单中关闭「开机自启」选项，再删除文件夹。

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
| **Application** | 应用生命周期管理。初始化顺序：日志 → 配置 → SysHelper → WindowCache → DockManager → UI → ProcessMonitor。退出时逆序清理，恢复原生任务栏。首次运行从系统任务栏导入固定项，后续从 `data/pinned.json` 合并加载 |
| **DockManager** | 核心状态机（`Docked` / `Hidden`）。管理固定项和瞬态项，全屏/最大化检测联动 |
| **ConfigManager** | JSON 配置文件读写（`data/config.json`），LRU 图标缓存（128 条），camelCase 键名，兼容旧版 snake_case 自动迁移 |
| **ProcessMonitor** | `CreateToolhelp32Snapshot` 每 2s 轮询进程状态，每 4 次扫描新窗口化应用 |
| **WindowCache** | 全量 `EnumWindows` 缓存 + WinEvent 增量更新。`QReadWriteLock` 线程安全，三档刷新（WinEvent → 2s 轮询 → 同步 `scanForClass`） |
| **ClickStateMachine** | 5 状态点击决策：NoWindows / BackgroundRunning / AllMinimized / ForegroundActive / BackgroundVisible |
| **IconProvider** | 5 级图标回退链：文件路径 → IShellItemImageFactory (UWP) → .lnk 解析 → Jumbo (`SHGetImageList(SHIL_JUMBO)`) → 字母占位图 |
| **AppIdHelper** | 进程名推导：`execPath` 文件名 → AppId 末段，用于 Win32 窗口类匹配 |
| **PinnedItemsReader** | 读取系统任务栏固定项（COM `IShellLink`，只读基准），用户额外固定项由 Application 管理并持久化到 `data/pinned.json` |
| **PathManager** | 便携路径管理（header-only），所有数据路径统一为 `./data/` 相对 exe 目录，惰性创建 |

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

### 部署为便携包

```cmd
# 1. 编译
cmake --build build --config Release

# 2. 部署 Qt 运行时（windeployqt 自动执行，或手动）
windeployqt --no-translations --no-opengl-sw --dir deploy build\Mac任务栏.exe

# 产物: deploy\ 文件夹即完整便携包，压缩分发给用户
```

### 项目结构

```
Dock_WMac/
├── include/
│   ├── core/              # 核心逻辑头（13 文件，含 PathManager.h）
│   └── ui/                # UI 组件头（4 文件）
├── src/
│   ├── core/              # 核心实现（11 文件）
│   ├── ui/                # UI 实现（4 文件）
│   └── main.cpp           # 入口
├── resources/             # .qrc 资源 + app.rc 图标
├── tests/                 # Google Test 单元测试（8 套，89 个用例）
├── .github/workflows/     # CI/CD（MSVC + Qt 6.8.2，含测试步骤）
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

- **便携优先** — 纯绿色软件，解压即用。所有数据存放在程序目录 `./data/`，不写注册表、不污染系统目录
- **无桌面快捷方式** — 设计哲学是"安装即忘的插件"，不是需要点击启动的应用
- **惰性数据创建** — `data/` 目录仅在首次写入时创建，干净启动零文件产生
- **Jumbo 图标管道** — 图标提取优先使用 `SHGetImageList(SHIL_JUMBO)` 获取高清系统图标，UWP 应用通过 `IShellItemImageFactory` 专用通道

---

## 更新日志

### v0.2.4 (2026-06-11) — 便携模式 + 图标管道重写

**破坏性变更：** 此版本从安装式应用转为便携应用，数据和配置路径已变更。

**新功能：**
- **便携模式** — 所有数据（配置、日志、固定项）存储在程序目录 `./data/`，无需安装、无需管理员权限
- **PathManager** — 新增 header-only 路径管理命名空间，统一所有数据路径

**图标管道重写：**
- Jumbo 图标 (`SHGetImageList(SHIL_JUMBO)`) 提升为主力提取通道
- UWP/AppX 图标改用 `IShellItemImageFactory`（替代旧 `SHGetFileInfo` 路径）
- 移除低质量的 `extractExeIcon`、`extractUwpIcon` 公共 API
- 图标回退链精简为 5 级（原 6 级）

**固定项管理重写：**
- 系统任务栏 `.lnk` 为只读基准，用户额外固定项持久化到 `data/pinned.json`
- 启动时自动合并：系统项 + 用户项（按 `execPath` 去重）
- 移除 `pinnedApps` 配置键和 `firstRunComplete` 标志

**移除：**
- Inno Setup 安装包构建（`installer/`）
- `uninstall.exe` 安全卸载器（`src/uninstall/`）
- PinnedItemsReader 的 `%TEMP%/dock_wmac_icons/` 图标缓存

**开发体验：**
- CI 构建流水线添加 `ctest` 测试步骤
- 测试运行时 Qt DLL PATH 自动注入（`ENVIRONMENT_MODIFICATION`）
- 项目测试覆盖：8 套可执行文件，89 个用例

### v0.2.3 已知问题（在 v0.2.4 中修复）

- **安装包臃肿** — Inno Setup 生成的 `Dock_WMac_Setup_*.exe` 包含 Qt 运行时，体积大且安装流程复杂
- **图标模糊** — `ExtractIconEx` 路径提取的 32×32 图标在高 DPI 下模糊
- **数据碎片化** — 配置文件、日志、图标缓存分散在 `%APPDATA%`、`%LOCALAPPDATA%`、`%TEMP%` 三个位置
- **无法直接移动** — 安装目录固定，不能随意移动程序文件夹

---

## 许可

[MIT](LICENSE)
