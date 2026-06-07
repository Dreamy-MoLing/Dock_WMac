# Dock_WMac

> macOS 风格应用 Dock，Qt6 + C++17，Windows 原生构建。

[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows-blue)](https://github.com/Dreamy-MoLing/Dock_WMac)

Dock_WMac 替代 Windows 原生任务栏，提供 macOS 风格 Dock 体验：鱼眼放大动画、自动隐藏、DWM 毛玻璃模糊、窗口实时预览、亮暗主题跟随。

---

## 功能特性

- **macOS 风格 Dock** — 图标居中排列，鱼眼放大动画（鼠标悬停时邻近图标等比放大）
- **自动隐藏** — 不使用时自动隐藏到屏幕边缘，鼠标触底/触边唤醒
- **DWM 毛玻璃模糊** — 全窗口半透明 + 圆角模糊背景
- **窗口预览** — 悬停 500ms 弹出 DWM 实时缩略图面板，支持 Peek 预览
- **亮暗主题** — 每 5s 检测系统主题色，自动切换图标和背景
- **智能点击** — 5 状态点击机：启动 / 显示隐藏窗口 / 恢复最小化 / 最小化 / 置前
- **双击启动** — 双击图标强制启动新实例
- **拖拽排序** — 拖拽图标调整固定项顺序
- **溢出面板** — 超过上限的图标进入 "…" 弹出菜单
- **低资源占用** — Win32 事件驱动 + `QReadWriteLock` 缓存，非轮询密集

---

## 快速开始

### 环境要求

| 组件 | 最低版本 |
|------|---------|
| Windows | Windows 10 / 11 |
| Visual Studio | 2022（含 MSVC v143 工具链） |
| Qt | 6.8.0+（MSVC 2022 64-bit） |
| CMake | 3.20+ |
| Ninja | 任意新版 |

### 构建

```powershell
# 一键构建（自动发现 VS + Qt + CMake）
.\build_now.ps1

# 带测试
.\build_now.ps1 -Tests

# Debug 模式
.\build_now.ps1 -Debug

# 清理重建
.\build_now.ps1 -Clean
```

产物：`build\Release\dock_wmac.exe`

### 运行测试

```powershell
.\build_now.ps1 -Tests
cd build && ctest -C Release
```

或单独运行：

```powershell
build\tests\Release\test_config.exe
build\tests\Release\test_dock_manager.exe
build\tests\Release\test_process_monitor.exe
build\tests\Release\test_sys_helper.exe
build\tests\Release\test_pinned_items_reader.exe
build\tests\Release\test_application.exe
build\tests\Release\test_window_cache.exe
build\tests\Release\test_click_state_machine.exe
```

---

## 架构

三层分层设计：**UI → Core → System**，系统层通过 Qt 信号上报事件。

```
┌─────────────────────────────┐
│     UI 层 (Qt6)             │  DockWindow, DockItem,
│                             │  WindowPreviewPanel, OverflowPanel
├─────────────────────────────┤
│     Core 逻辑层              │  Application, DockManager, ConfigManager,
│                             │  ProcessMonitor, PinnedItemsReader,
│                             │  WindowCache, ClickStateMachine,
│                             │  AppIdHelper, IconProvider, Logger
├─────────────────────────────┤
│     System 系统层            │  SysHelper（Win32 + COM + DWM）
└─────────────────────────────┘
```

### 关键模块

| 模块 | 职责 |
|------|------|
| `DockWindow` | 无边框置顶窗口，手动布局 + 鱼眼动画 (`QPropertyAnimation`) |
| `DockItem` | 图标控件，运行指示器、角标、窗口堆叠指示器 |
| `WindowPreviewPanel` | 悬停预览：`DwmRegisterThumbnail` + Peek (`SetWindowPos`) |
| `OverflowPanel` | 超限项 "…" 弹出菜单 |
| `DockManager` | 状态机 (`Docked` / `Hidden`)，管理固定项和瞬态项 |
| `WindowCache` | `EnumWindows` 缓存，`QReadWriteLock` 线程安全，WinEvent 增量更新 |
| `ClickStateMachine` | 5 状态点击机：无窗口→启动 / 后台→显示 / 最小化→恢复 / 前台→最小化 / 可见→置前 |
| `IconProvider` | 6 级 Win32 Shell API 图标回退：文件路径 → UWP → `ExtractIconEx` → `.lnk` → `SHGetImageList` → 字母占位 |
| `ProcessMonitor` | `CreateToolhelp32Snapshot` 每 2s 轮询，每 4 次扫描瞬态应用 |
| `ConfigManager` | JSON 配置 (`%APPDATA%/Dock_WMac/config.json`)，LRU 图标缓存 (128 条) |
| `SysHelper` | Win32 封装：窗口钩子、键盘钩子、DWM 模糊、全屏检测、主题检测、任务栏隐藏/恢复 |

---

## 项目结构

```
Dock_WMac/
├── include/
│   ├── core/             # 核心逻辑头文件
│   └── ui/               # UI 组件头文件
├── src/
│   ├── core/             # 核心实现
│   └── ui/               # UI 实现
├── resources/            # QRC 资源文件
├── tests/                # Google Test 单元测试（8 套）
├── .github/workflows/    # CI/CD
├── build_now.ps1         # 一键构建脚本
├── setup_msvc.sh         # MSVC 环境注入（bash 用）
└── build/                # CMake 构建输出
```

---

## 依赖

| 依赖 | 用途 |
|------|------|
| Qt6::Widgets | UI 框架 |
| Qt6::Svg | SVG 图标渲染 |
| dwmapi | DWM 模糊 + 缩略图 |
| shell32 | COM Shell 接口（图标、.lnk 解析） |
| shlwapi | 注册表操作 |
| user32 | Win32 窗口 API |

测试：Google Test v1.14.0（CMake `FetchContent` 自动下载）

---

## 许可

[MIT](LICENSE)
