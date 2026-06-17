# AGENTS.md

Windows-only macOS-style dock written in Qt6 + C++17. The product goal is to preserve core Windows taskbar behavior while adding macOS-style Dock animation and interaction: fisheye magnification, auto-hide, DWM blur, window previews, pinned/running app management, and portable runtime data.

## Source of Truth

- `README.md` is user-facing usage/build documentation.
- `AGENTS.md` is the current engineering handoff document.
- `docs/archive/` contains historical or agent-generated plans and may be stale.
- `.claude/`, `.mimocode/`, and `graphify-out/` are external agent/plugin outputs, not authoritative source code.

## Build & Test

Use go-task for local and CI parity:

```bash
task configure   # cmake --preset default
task build       # build Release
task test        # build + ctest
task package     # test + CPack ZIP
task local-ci    # configure + test + package
task ci          # CI path; requires CMAKE_GENERATOR, VS_INSTALL_PATH, QT_ROOT_DIR
```

Direct fallback:

```bash
cmake --preset default -DBUILD_TESTS=ON
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure
```

Qt DLLs are injected into test PATH via CMake `ENVIRONMENT_MODIFICATION`.

## Architecture

Three layers: **UI -> Core -> System**.

- UI: `DockWindow`, `DockItem`, `DockAnimation`, `WindowPreviewPanel`, `OverflowPanel`
- Core: `Application`, `DockManager`, `ConfigManager`, `ProcessMonitor`, `WindowCache`, `ClickStateMachine`, `IconProvider`, `AppIdHelper`, `PinnedItemsReader`, `Logger`, `PathManager`
- System: `SysHelper` wraps Win32/COM/DWM behavior

Keep new code on the correct side of the boundary. UI collects events and renders. Core owns state and decisions. System owns direct Win32/COM/DWM calls. Existing direct Win32 calls in `ClickStateMachine`, `WindowCache`, and `WindowPreviewPanel` are known debt to be reduced, not copied.

## Module Boundaries

`DockWindow.cpp` should stay a coordinator: construction, dependency injection, signal wiring, and shared layout. Put focused behavior in sibling implementation files:

- `DockWindow_position.cpp` for screen positioning, DPI, and native setting events
- `DockWindow_input.cpp` for mouse, click, drag, and hit-test behavior
- `DockWindow_menu.cpp` for Dock background context menu behavior
- `DockWindow_itemmanager.cpp` for DockItem lifecycle and overflow item management
- `DockWindow_theme.cpp` for painting, blur, and theme updates
- `DockWindow_transition.cpp` for show/hide animations

Pinned item ordering must flow through `DockManager::reorderPinnedItems(...)` so UI order and `pinned.json` persistence stay aligned.

## Key Gotchas

- Binary name: `WMacDock.exe`; CMake target: `dock_wmac`.
- Portable mode: runtime data lives in `./data/` beside the executable.
- Single-instance key: `Dock_WMac_Instance`.
- Default preset hardcodes local Visual Studio and Qt paths; CI uses the `ci` preset with env var overrides.
- Branch name is `master`.
- `resources/app.ico` must remain tracked despite image ignore rules.
- Do not treat archived Markdown as current requirements without checking source and tests.

## CI/CD

GitHub Actions installs Qt and go-task, detects the Visual Studio generator, then runs `task ci`. Packaging is CPack ZIP. Tag releases use `softprops/action-gh-release` with GitHub-generated release notes.
