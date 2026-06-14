# AGENTS.md

Windows-only macOS-style dock (Qt6 + C++17). Replaces native taskbar with fisheye magnification, auto-hide, DWM blur.

## Build & Test

```bash
cmake --preset default -DBUILD_TESTS=ON
cmake --build build --config Release
cd build && ctest -C Release --output-on-failure
```

Single test: `build/tests/Release/test_<name>.exe`

Qt DLLs auto-injected into test PATH via CMake `ENVIRONMENT_MODIFICATION` — no manual setup needed.

## Key Gotchas

- **MSVC generator**: Visual Studio 18 2026, hardcoded in `CMakePresets.json`. No Developer Command Prompt needed.
- **Qt path**: `C:/Qt/6.11.1/msvc2022_64` (hardcoded in presets). CI uses Qt 6.11.1 — same version.
- **Binary name**: `WMacDock.exe`, CMake target: `dock_wmac`
- **New source files**: Add to `DOCK_CORE_SOURCES` in root `CMakeLists.txt` only — tests link via `dock_core_objects` object library.
- **Header-only modules**: `AppIdHelper.h`, `PathManager.h` — no .cpp files.
- **Portable mode**: All runtime data in `./data/` (config.json, pinned.json, dock.log). Lazy-created.
- **WIN32 subsystem**: `add_executable(dock_wmac WIN32 ...)` — no console window on launch.
- **Resources**: `resources/resources.qrc` + `resources/app.rc` linked in CMakeLists.txt. `.ico` must be in git (`.gitignore` has `!` exception).

## CI Lessons Learned

5 fix commits in a row on CI alone — don't repeat these:

- **Deploy paths are fragile**: `windeployqt --dir` copies DLLs only, not exe. VS multi-config outputs to `build/Release/`, not `build/`. Always verify the zip contents locally before pushing CI packaging changes.
- **Shell matters**: Default pwsh breaks on Chinese paths and tag extraction. Use `shell: bash` for all release/deploy steps.
- **.gitignore wildcards are dangerous**: `*.ico` killed `resources/app.ico`. Always add `!` exceptions for build-critical resources.
- **CMakePresets hardcodes local paths**: The `default` preset has `C:/Qt/6.11.1/...` and VS 18 Insiders path. CI uses the `ci` preset with env var overrides. Never modify the `ci` preset's env var pattern — it's the only CI-safe path.
- **permissions: write required**: GitHub Actions needs `contents: write` to create releases. Forgot this once → 403.
- **Branch name is `master`** not `main`.
- **CI Qt version must match local**: Keep `release.yml` Qt version in sync with `CMakePresets.json` to avoid silent API differences.

## Architecture

Three-layer: **UI → Core → System**. SysHelper is the only System-layer class.

```
src/main.cpp → Application.cpp → DockManager.cpp → DockWindow.cpp
```

Key classes:
- `WindowCache` — central window state cache, WinEvent-driven updates
- `ClickStateMachine` — 5-state click behavior (no windows / background / minimized / foreground active / background visible)
- `IconProvider` — 5-level Win32 icon fallback chain
- `ProcessMonitor` — CreateToolhelp32Snapshot every 2s

Init order: Logger → Config → SysHelper → WindowCache → DockManager → UI → ProcessMonitor

Single-instance: `QSharedMemory` key `"Dock_WMac_Instance"` — second launch exits immediately.

## Conventions

- C++17, Qt6, Google Test v1.14.0 (FetchContent)
- Test shared compilation: `dock_core_objects` object library (no per-test recompile)
- Config keys use camelCase (v0.3.0+)
- CLAUDE.md is gitignored (local dev reference). README.md is user-facing.
- System libs: `dwmapi`, `shlwapi`, `shell32`, `user32` — linked in CMakeLists.txt

## References

- `CLAUDE.md` — comprehensive dev doc with full architecture, refactoring history
- `README.md` — user-facing project overview
- `graphify-out/` — knowledge graph (gitignored, generate with `graphify update .`). Codebase questions: `graphify query/explain/path` before reading source files.
