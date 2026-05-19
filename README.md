# Dock_WMac

> 一个 macOS 风格的应用 Dock，使用 Qt6 和 C++17 构建，支持 Windows 和 Linux。

[![License](https://img.shields.io/badge/license-MIT-blue)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey)](https://github.com/Dreamy-MoLing/Dock_WMac)

---

## 功能特性 / Features

| 中文 | English |
|------|---------|
| macOS 风格图标 Dock，带鱼眼放大动画 | macOS-style icon dock with fisheye magnification animation |
| 自动隐藏（不使用时隐藏） | Auto-hide when not in use |
| 键盘快捷键切换应用（类似 Alt+Tab） | Keyboard shortcut for app switching (Alt+Tab-like) |
| 可配置的图标大小和位置 | Configurable icon size and position |
| IPC 进程间通信 | IPC-based inter-process communication |
| 低资源占用 | Minimal resource usage |

---

## 构建 / Building

### Linux（原生构建）

```bash
# 安装依赖
sudo apt install qt6-base-dev libqt6svg6-dev cmake g++ make

# 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)

# 测试
QT_QPA_PLATFORM=offscreen ctest --test-dir build
```

### Windows（Docker 交叉编译）

```bash
# 构建 Docker 镜像（首次）
./scripts/docker-build.sh build

# 交叉编译 dock_wmac.exe
./scripts/docker-build.sh compile

# 产物在 build_win/dock_wmac.exe
```

---

## 架构 / Architecture

三层分层设计，调用链：**UI 层 → 核心层 → 系统层**，系统层通过 Qt 信号上报事件。

```
┌─────────────────────────┐
│     UI 层 (Qt6)         │  DockWindow, DockItem
├─────────────────────────┤
│   核心逻辑层             │  DockManager, ConfigManager, Application
├─────────────────────────┤
│   系统适配层             │  SysHelper（Win32/Linux 抽象）
└─────────────────────────┘
```

---

## 项目结构 / Structure

```
Dock_WMac/
├── include/              # 头文件
│   ├── core/             #   核心逻辑（Application, DockManager, ConfigManager...）
│   └── ui/               #   UI 组件（DockWindow, DockItem）
├── src/
│   ├── core/             # 核心实现
│   └── ui/               # UI 实现
├── resources/            # 资源文件（图标、QRC、desktop 文件）
├── tests/                # 单元测试 + 集成测试
├── cmake/                # CMake 配置和交叉编译工具链
├── scripts/              # 构建脚本
└── .github/workflows/    # CI/CD 流程
```

---

## 依赖 / Dependencies

| 环境 | 依赖 |
|------|------|
| **Linux** | Qt6::Widgets + Qt6::Svg (`apt: qt6-base-dev libqt6svg6-dev`) |
| **Windows 交叉编译** | Docker (openscad/mxe-x86_64-gui 镜像, MXE + Qt6 预编译) |
| **构建工具** | CMake 3.20+, C++17, GCC/Clang |
| **测试（可选）** | Google Test (`BUILD_TESTS=ON`) |

---

## CI/CD

| Job | 触发条件 | 操作 |
|-----|---------|------|
| Linux (Ubuntu) | PR + tag push | 构建 + 测试 |
| Windows (MXE cross-compile) | tag push | Docker 交叉编译 → Release |

---

## 许可 / License

[MIT](LICENSE)
