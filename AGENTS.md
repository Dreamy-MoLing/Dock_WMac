# AGENTS.md

This repository is a Windows-only Dock application. The active line is v2:
C++20, C++/WinRT, WinUI 3, Windows App SDK Stable, MSVC, Visual Studio 2022,
Windows 10 1809+, and Windows 11.

The existing Qt6 Widgets + C++17 code is v1 historical reference only. Do not
add new product features to the Qt architecture unless the user explicitly asks
for v1 maintenance.

## Source of Truth

- `README.md` is the user-facing v2 overview and build entry.
- `AGENTS.md` is the engineering handoff.
- `docs/PRODUCT_SPEC.md` defines product scope and non-goals.
- `docs/ARCHITECTURE.md` defines layer ownership and hard boundaries.
- `docs/UX_SPEC.md` defines visible behavior.
- `docs/TECH_STACK.md` defines the fixed technology stack.
- `docs/MIGRATION.md` defines how v1 is used as reference.
- `docs/ROADMAP.md` defines staged delivery.
- `docs/VALIDATION.md` defines acceptance checks.
- `docs/AI_WORKFLOW.md` defines future agent workflow.
- `docs/archive/`, `.claude/`, `.mimocode/`, and `graphify-out/` are not
  authoritative for v2.

## Current Engineering Rule

First build a small, verifiable v2 foundation. Do not implement the full Dock,
player, lyrics, or animation system during preparation work. Keep the first
running result to an empty semi-transparent `DockWindow`.

Do not introduce Electron, Qt, WPF, Avalonia, or WebView as the main UI stack.

## Build

Open v2 in Visual Studio 2022:

```powershell
start Dock_WMac_v2.sln
```

Build from a Developer PowerShell:

```powershell
msbuild Dock_WMac_v2.sln /restore /p:Configuration=Debug /p:Platform=x64
```

This remains the Visual Studio 2022 / `v143` baseline. If the local machine only
has Visual Studio Insiders / VS2026, use the installed MSBuild with the `v145`
toolset override:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" Dock_WMac_v2.sln /restore /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145
```

The old CMake and go-task files are archived under `legacy/qt-v1/`. Root-level
build work should use `Dock_WMac_v2.sln`.

## v2 Architecture

Layer ownership is fixed:

- `app/`: lifecycle, single instance, launch arguments, config loading, main
  window creation.
- `ui/`: WinUI 3 XAML pages, Dock visuals, media panel, lyrics visual layer,
  animation states.
- `dock/`: Dock state machine, icon model, pinned item management, running app
  mapping, click decisions, ordering, persistence.
- `shell/`: Win32 window enumeration, DWM thumbnails, Shell Link parsing, icon
  extraction, taskbar pinned item reading, native taskbar hide/restore.
- `media/`: GSMTC session discovery, current session selection, playback
  snapshots, media transport commands.
- `lyrics/`: LRC parsing, lyric matching, local lyric index, cache, no-lyrics
  fallback.
- `platform/`: DPI, multi-monitor, system theme, high contrast, reduced motion,
  window z-order policy.
- `infra/`: logging, path management, JSON config, error handling, performance
  sampling.

## Hard Boundaries

- UI must not call Win32, COM, DWM, or GSMTC directly.
- `dock/` must not call WinRT or COM directly.
- `media/` must not depend on Dock state.
- `lyrics/` must never block the media panel.
- `shell/` adapts system APIs and must not store product state.
- Every system API call must sit behind an interface or adapter that can be
  mocked or isolated in tests.
- Apple Music means Apple Music for Windows through GSMTC only.
- Lyrics must not depend on private Apple interfaces.

## v1 Reference Policy

Use v1 to understand behavior, edge cases, and tests. Do not copy Qt ownership
boundaries into v2. v1 is archived under `legacy/qt-v1/` and documented by
`legacy/qt-v1/FREEZE.md`.

## Code Discovery

Prefer codebase-memory-mcp for code discovery when available:

1. `search_graph`
2. `trace_path`
3. `get_code_snippet`
4. `query_graph`
5. `get_architecture`

Use text search for docs, configs, string literals, and cases where graph
results are insufficient.
