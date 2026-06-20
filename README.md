# Dock_WMac

> 原生、便携、低干扰的 Windows 应用 Dock。

Dock_WMac 提供 macOS 风格的应用启动和窗口切换体验。它不是完整的 Windows
任务栏或 Shell 替代品：目前不提供开始菜单、通知区域、时钟和快速设置。

[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%2F11-blue)](https://github.com/Dreamy-MoLing/Dock_WMac)
[![Version](https://img.shields.io/badge/version-0.2.4-brightgreen)](https://github.com/Dreamy-MoLing/Dock_WMac/releases)

---

## 功能特性

### 核心体验

| 特性 | 说明 |
|------|------|
| 🧲 **鱼眼放大** | 鼠标悬停时图标等比放大，邻近图标连带缩放（`QPropertyAnimation`） |
| 🙈 **自动隐藏** | 无操作时滑出屏幕边缘，鼠标触底唤醒 |
| 🔍 **窗口预览** | 悬停 500ms 弹出 DWM 实时缩略图，再悬停触发 Peek 置顶 |
| 🌓 **亮暗主题** | 每 5 秒检测系统主题色，自动跟随切换 |
| 🧊 **毛玻璃模糊** | 全窗口半透明 + 圆角 DWM 模糊背景 |

### 交互

| 特性 | 说明 |
|------|------|
| 🖱️ **智能点击** | 5 状态点击机：无窗口→启动 / 后台→显示 / 最小化→恢复 / 前台→最小化 / 可见→置前 |
| 🔄 **双击启动** | 双击图标强制启动新实例 |
| ↔️ **拖拽排序** | 拖拽图标调整顺序，实时保存 |
| 📋 **溢出面板** | 超过上限（默认 16 个）的图标进入 "…" 弹出菜单 |
| 🔴 **运行指示器** | 图标下方绿色圆点标记运行中的应用 |
| 🔢 **角标** | 红色角标显示未读计数 |

### 设计

- **原生窗口集成** — Win32 事件 + DWM 预览，窗口缓存使用 `QReadWriteLock`
- **低干扰运行** — 无广告、无默认遥测；开机自启由用户主动开启
- **便携优先** — 数据优先写入程序目录 `./data/`；目录不可写时自动回退到用户本地应用数据目录

---

## 下载与使用

### 即下即用（便携模式）

1. 从 [Releases](https://github.com/Dreamy-MoLing/Dock_WMac/releases) 下载最新 `dock_wmac_v*.zip`
2. 解压到任意目录（建议固定英文路径，如 `D:\Tools\Dock_WMac\`）
3. 双击 `WMacDock.exe`

首次运行自动导入系统任务栏固定项作为初始内容。

### 目录结构

```
Dock_WMac/
├── WMacDock.exe          ← 主程序
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

如果程序所在目录不可写，数据回退到 `%LOCALAPPDATA%\Dock_WMac\data\`。
“隐藏原生任务栏”是默认关闭的实验选项，必须从 Dock 背景右键菜单主动开启。

### 卸载

先在右键菜单关闭开机自启并退出，然后删除程序目录。若曾使用数据目录回退，
可按需再删除 `%LOCALAPPDATA%\Dock_WMac\data\`。

---

## 开发

### 环境要求

| 组件 | 要求 |
|------|------|
| 操作系统 | Windows 10 / 11 |
| 编译器 | Visual Studio 2022 或 2026（MSVC） |
| Qt | 6.8.3（MSVC 2022 64-bit） |
| CMake | 3.20+ |
| Task | go-task 3.x |

### 快速构建

```powershell
$env:CMAKE_GENERATOR = "Visual Studio 17 2022"
$env:VS_INSTALL_PATH = "C:\Program Files\Microsoft Visual Studio\2022\Community"
$env:QT_ROOT_DIR = "C:\Qt\6.8.3\msvc2022_64"

task configure
task build
# 产物: build\Release\WMacDock.exe
```

也可以复制 `CMakeUserPresets.json.example` 为不入库的 `CMakeUserPresets.json`，
按本机路径修改后执行 `cmake --preset local`。仓库不再保存个人机器路径。

### 运行测试

```powershell
cmake --preset default -G "$env:CMAKE_GENERATOR" -DBUILD_TESTS=ON
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

12 个测试可执行文件，包含 Win32/DWM 实际窗口集成检查。测试构建会禁止修改
HKCU 开机项和隐藏系统任务栏：

```
test_config              test_process_monitor       test_window_cache
test_dock_manager        test_sys_helper            test_click_state_machine
test_pinned_items_reader test_application           test_dwm_state
test_window_cloaked      test_display_affinity      test_windows_integration
```

### 部署便携包

```bash
task package
# 产物: build\Dock_WMac_0.2.4_x64.zip
```

CPack 会调用 `windeployqt` 并加入 MSVC 运行库；不要只分发裸 exe。

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
├── .github/workflows/     # CI/CD
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

---

## 架构

三层设计：**UI → Core → System**，系统层通过 Qt 信号向上报告事件。

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

- **UI 层** — DockWindow（无边框置顶主窗口，鱼眼动画、拖拽排序）、DockItem（图标控件，运行/激活指示器）、WindowPreviewPanel（DWM 实时缩略图预览）、OverflowPanel（超限弹出菜单）
- **Core 层** — Application（生命周期）、DockManager（`Docked`/`Hidden` 状态机 + 全屏检测）、WindowCache（`EnumWindows` 缓存 + WinEvent 增量更新）、ClickStateMachine（5 状态点击决策）、IconProvider（5 级 Win32 图标回退链）、ConfigManager（JSON 配置 + LRU 图标缓存）、ProcessMonitor（`CreateToolhelp32Snapshot` 每 2s 轮询）、PinnedItemsReader（COM `IShellLink` 读取系统任务栏固定项）、AppIdHelper（进程名推导）、PathManager（便携路径 header-only）
- **System 层** — SysHelper：WinEvent 窗口钩子、`WH_KEYBOARD_LL` 键盘钩子、DWM 模糊、全屏检测、主题检测（注册表）、任务栏控制、开机自启

> 当前开发与接手说明见仓库内 [AGENTS.md](./AGENTS.md)。历史规划和诊断文档已归档到 `docs/archive/`，以 README、AGENTS 和当前源码为准。

真实用户验证方法、继续/转向/归档阈值和 Windows 人工验收矩阵见
[VALIDATION.md](./VALIDATION.md)。项目默认不收集遥测。

---

## 更新日志

版本发布说明见 [GitHub Releases](https://github.com/Dreamy-MoLing/Dock_WMac/releases)。

---

## 许可

[MIT](LICENSE)

