# Dock_WMac

A macOS-style application dock for Windows and Linux, built with Qt6 and C++17.

![License](https://img.shields.io/badge/license-MIT-blue)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-lightgrey)

## Features

- macOS-style icon dock with magnification (fisheye) animation
- Auto-hide when not needed
- Keyboard shortcut (Alt+Tab-like) for app switching
- Configurable icon size and position
- IPC-based process communication
- Minimal resource usage

## Building

### Linux (native)

```bash
# Install dependencies
sudo apt install qt6-base-dev libqt6svg6-dev cmake g++ make

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel $(nproc)

# Test
QT_QPA_PLATFORM=offscreen ctest --test-dir build
```

### Windows (cross-compile with Docker)

```bash
# Build Docker image (once)
./scripts/docker-build.sh build

# Cross-compile dock_wmac.exe
./scripts/docker-build.sh compile

# Binary is at build_win/dock_wmac.exe
```

## Architecture

Three-layer design:

```
┌─────────────────────────┐
│     UI Layer (Qt6)      │  DockWindow, DockItem
├─────────────────────────┤
│   Core Logic Layer      │  DockManager, ConfigManager, Application
├─────────────────────────┤
│   System Adapter Layer  │  SysHelper (Win32/Linux abstraction)
└─────────────────────────┘
```

## License

MIT
