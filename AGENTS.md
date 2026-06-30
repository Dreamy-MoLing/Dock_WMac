# AGENTS.md

Windows-only macOS-style dock written in Qt6 + C++17. The product goal is to preserve core Windows taskbar behavior while adding macOS-style Dock animation and interaction: fisheye magnification, auto-hide, DWM blur, window previews, pinned/running app management, and portable runtime data.

## Source of Truth

- `README.md` is user-facing usage/build documentation.
- `AGENTS.md` is the current engineering handoff document.
- `docs/music-player/DEVELOPMENT.md` is the source of truth for the planned music player feature until it ships.
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
cmake --preset default -G "$env:CMAKE_GENERATOR" -DBUILD_TESTS=ON
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

## Music Player Feature

The music player is a new Windows-only GSMTC companion panel, separate from the existing Dock implementation. Keep future music code under `include/music/`, `src/music/`, and `tests/music/`, split by `core`, `system`, `ui`, and `lyrics` as described in `docs/music-player/DEVELOPMENT.md`.

Do not put GSMTC/session logic into `SysHelper`, and do not put Now Playing state into `DockManager`. Archived ChatGPT research under `docs/archive/music-player-research/` is background only; the development guide is current.

## Key Gotchas

- Binary name: `WMacDock.exe`; CMake target: `dock_wmac`.
- Portable mode prefers `./data/` beside the executable and falls back to the
  user's local app-data directory when the executable directory is not writable.
- Single-instance key: `Dock_WMac_Instance`.
- Tracked presets contain no personal machine paths. Local and CI builds supply
  `CMAKE_GENERATOR`, `VS_INSTALL_PATH`, and `QT_ROOT_DIR`; Qt baseline is 6.8.3.
- Native taskbar hiding is experimental, defaults off, and requires explicit user opt-in.
- Branch name is `master`.
- Use `codex` for experimental music-player development, review, and acceptance; merge to `master` only after validation for release.
- `resources/app.ico` must remain tracked despite image ignore rules.
- Do not treat archived Markdown as current requirements without checking source and tests.

## CI/CD

GitHub Actions installs Qt and go-task, detects the Visual Studio generator, then runs `task ci`. Packaging is CPack ZIP. Tag releases use `softprops/action-gh-release` with GitHub-generated release notes.
