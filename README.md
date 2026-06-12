# Dock_WMac

> macOS 风格应用停靠栏 — 替代 Windows 原生任务栏，安装即忘的桌面插件。

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

- **低资源占用** — Win32 事件驱动 + `QReadWriteLock` 线程安全缓存
- **静默运行** — 无弹窗、无通知、无打扰，开机自启
- **便携模式** — 所有数据（配置、日志、固定项）存于程序目录 `./data/`，无需安装、无需管理员权限

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

### 卸载

**删除整个文件夹即可。** 如设置了开机自启，先右键菜单关闭该选项再删除。

---

## 开发

### 环境要求

| 组件 | 要求 |
|------|------|
| 操作系统 | Windows 10 / 11 |
| 编译器 | Visual Studio 2026（MSVC v143） |
| Qt | 6.11.1（MSVC 2022 64-bit） |
| CMake | 3.20+ |

### 快速构建

```bash
cmake --preset default
cmake --build build --config Release
# 产物: build\WMacDock.exe
```

CMakePresets.json 已配置好 VS 生成器和 Qt 路径，无需额外设置环境变量。

### 运行测试

```bash
cmake --preset default -DBUILD_TESTS=ON
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure
```

8 套测试，89 个用例：

```
test_config              test_process_monitor     test_window_cache
test_dock_manager        test_sys_helper          test_click_state_machine
test_pinned_items_reader test_application
```

### 部署便携包

```bash
cmake --build build --config Release
windeployqt --no-translations --no-opengl-sw --compiler-runtime --dir deploy build\WMacDock.exe
# deploy\ 即完整便携包，压缩分发给用户
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

> 完整开发文档见仓库内 [CLAUDE.md](./CLAUDE.md)（面向 AI 编码助手）。开发者也可参考其中的架构细节、设计决策和重构历史。

---

## 更新日志

版本发布说明见 [GitHub Releases](https://github.com/Dreamy-MoLing/Dock_WMac/releases)。

---

## 许可

[MIT](LICENSE)
