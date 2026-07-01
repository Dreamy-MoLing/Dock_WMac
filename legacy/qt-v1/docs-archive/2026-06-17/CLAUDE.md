# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

Dock_WMac — Windows 原生任务栏功能 + macOS 停靠栏外观与交互，Qt6 + C++17。鱼眼放大 + 自动隐藏 + DWM 模糊 + 亮暗主题。

## Build & Test

Qt 6.11.1 @ `C:/Qt/6.11.1/msvc2022_64`，MSVC 由 CMakePresets.json 指定，无需 Developer Command Prompt。

```bash
cmake --preset default                    # Release build
cmake --build build --config Release      # → build/WMacDock.exe

# 带测试
cmake --preset default -DBUILD_TESTS=ON
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure

# 单个测试
build/tests/Release/test_<name>.exe
```

测试 Qt DLL PATH 自动注入（`ENVIRONMENT_MODIFICATION`），Google Test v1.14.0 FetchContent 自动下载，`dock_core_objects` object library 共享编译。

## Architecture

三层：**UI → Core → System**，系统层通过 Qt 信号向上报告。

```
UI:    DockWindow, DockItem, DockAnimation, WindowPreviewPanel, OverflowPanel
Core:  Application, DockManager, ConfigManager, ProcessMonitor,
       WindowCache, ClickStateMachine, IconProvider, AppIdHelper,
       PinnedItemsReader, Logger, PathManager
System: SysHelper (Win32 + COM + DWM)
```

- **DockWindow** — 无边框置顶主窗口，布局/主题/事件/拖拽，动画委托给 DockAnimation
- **DockItem** — 图标控件，运行/激活指示器 + 角标
- **DockAnimation** — QPropertyAnimation 封装：鱼眼高斯缩放 + 图标添加/移除动画，帧级批处理布局
- **WindowPreviewPanel** — DWM 实时缩略图预览（悬停 500ms 弹出）
- **OverflowPanel** — 超限图标 "…" 弹出菜单
- **Application** — 生命周期，init: Logger → Config → SysHelper → WindowCache → DockManager → UI → ProcessMonitor
- **DockManager** — `Docked`/`Hidden` 状态机，全屏检测（显示器感知），max 16 项
- **WindowCache** — `EnumWindows` 缓存 + WinEvent 增量更新，`QReadWriteLock` 线程安全
- **ClickStateMachine** — 5 状态点击：无窗口启动 / 后台显示 / 最小化恢复 / 前台最小化 / 可见置前
- **IconProvider** — 5 级 Win32 图标回退链（Jumbo 主力）
- **SysHelper** — WinEvent 钩子 + 键盘钩子 + DWM 模糊 + 全屏/主题检测 + 任务栏控制
- **ConfigManager** — `data/config.json`，LRU 图标缓存 128，camelCase keys
- **ProcessMonitor** — `CreateToolhelp32Snapshot` 每 2s
- **PinnedItemsReader** — COM `IShellLink` 读取系统任务栏 `.lnk` 基准
- **AppIdHelper** / **PathManager** — header-only（进程名推导 / 便携路径 `./data/`）

## Key Facts

- 输出 `WMacDock.exe`，CMake target `dock_wmac`，WIN32 subsystem
- 便携模式：所有数据在 exe 同级 `data/`，惰性创建
- Single-instance: `QSharedMemory` key `"Dock_WMac_Instance"`
- 新增源文件只需加到根 `CMakeLists.txt` 的 `DOCK_CORE_SOURCES`
- 依赖: Qt6::Widgets, Qt6::Svg, dwmapi, shell32, shlwapi
- 退出时 signal handlers + atexit 恢复原生任务栏
- CI: `.github/workflows/release.yml`，Qt 6.11.1 + MSVC 动态检测 + cpack ZIP

## graphify

项目有 `graphify-out/` 知识图谱。代码库问题先用 `graphify query/explain/path`，再读源文件。改代码后 `graphify update .`。
