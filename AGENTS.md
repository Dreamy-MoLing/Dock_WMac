# AGENTS.md

This repository is a Windows-only Dock application. The active line is v2:
C++20, C++/WinRT, Windows App SDK Stable, native Win32 Dock host,
DirectComposition, Direct2D/DirectWrite, MSVC, Visual Studio 2022, Windows 10
1809+, and Windows 11. WinUI 3 is allowed only for secondary app surfaces such
as settings, dialogs, or non-Dock panels.

The old Qt implementation has been removed from the active repository context.
Do not use it for v2 discovery, planning, or implementation.

## Source of Truth

- `README.md` is the user-facing v2 overview and build entry.
- `AGENTS.md` is the engineering handoff.
- `docs/PRODUCT_SPEC.md` defines product scope and non-goals.
- `docs/ARCHITECTURE.md` defines layer ownership and hard boundaries.
- `docs/UX_SPEC.md` defines visible behavior.
- `docs/TECH_STACK.md` defines the fixed technology stack.
- `docs/MIGRATION.md` defines the legacy-removal state and v2 focus.
- `docs/ROADMAP.md` defines staged delivery.
- `docs/V1_RELEASE_SPEC.md` defines the v1.0.0 release target.
- `docs/UI_MOTION_BASELINE.md` defines the v1.0.0 Dock visual and
  motion baseline confirmed through the browser prototype.
- `docs/SYSTEM_API_RISKS.md` defines Windows API risk handling.
- `docs/VALIDATION.md` defines acceptance checks.
- `docs/AI_WORKFLOW.md` defines future agent workflow.
- `.gitignore` controls which `docs/` files are tracked; unlisted docs ignored.
- `.claude/`, `.mimocode/`, and `graphify-out/` are not authoritative for v2.

## Current Engineering Rule

The first formal development target is `v1.0.0 Taskbar Dock Release`: a
Windows-facing Mac-style taskbar: it presents a Dock-like visual surface while
matching Windows taskbar behavior, system integration, app/window semantics,
and user expectations as closely as public Windows APIs allow. It must include
polished Dock UI/animation, pin handling, running app recognition,
Windows-default window switching, multi-window handling, DWM previews,
ordering, persistence, release packaging, and low steady-state memory usage.
Media panel, lyrics, and audio-reactive visuals are post-v1.0.0 extension
tracks unless the user explicitly changes scope.

Do not introduce Electron, Qt, WPF, Avalonia, WebView, or WinUI XAML as the
main Dock surface stack. The production Dock chrome uses a native Win32
non-activating popup host rendered by DirectComposition and Direct2D/DirectWrite.

The user owns product requirements; agents own implementation choices. Do not
ask the user to choose low-level technical details unless the choice changes
visible behavior, privacy/security posture, release shape, or maintenance risk.
Use local facts, official documentation, and runnable validation to choose the
simplest implementation that satisfies the documented product need.

Treat user prompts as requirement input, not as blindly executable
implementation instructions. If a prompt conflicts with the product north star,
Windows taskbar behavior, low-resource goals, or source-of-truth docs, state the
conflict briefly, update the docs if they are incomplete, and implement the path
that best preserves product quality. The docs exist to prevent implementation
drift; do not create temporary fake results or churn through UI rewrites that
do not move the real Windows Dock toward the documented target.

The v1.0.0 native Dock does not call Windows App SDK runtime APIs. Keep its
automatic bootstrap disabled and link the Release CRT statically so the x64
zip remains a small native xcopy deployment. The pinned Windows App SDK package
is reserved for future secondary surfaces and must not pull WinUI, WebView, ML,
or Windows App Runtime payloads into the v1 Dock release.

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

Root-level build work must use `Dock_WMac_v2.sln`.

CLI flags (headless):
  Dock_WMac_v2.exe --self-check
  Dock_WMac_v2.exe --dump-dock-state

Build output: `build\v2\$(Platform)\$(Configuration)\Dock_WMac_v2.exe`

NuGet pins (upgrade only in dedicated toolchain change):
  - `Microsoft.WindowsAppSDK` `1.8.260529003`
  - `Microsoft.Windows.CppWinRT` `2.0.250303.1`

Windows SDK: target `10.0.26100.0`, min `10.0.17763.0` (Win10 1809+).

PCH: new `.cpp` files include `"pch.h"` first.

## Continuous Integration

Workflow `.github/workflows/release.yml`: triggers on push/PR to `master`/`rewrite`
and `v*` tags. Builds Release x64 (`PlatformToolset=v145`), runs `--self-check`,
packages via `scripts/package-v1-release.ps1`. Tag pushes publish a GitHub Release.

## v2 Architecture

Layer ownership is fixed:

- `app/`: lifecycle, single instance, launch arguments, config loading, Dock
  host creation, secondary window creation.
- `ui/`: Dock view models, render descriptions, animation states, WinUI 3
  secondary pages, media panel, lyrics visual layer.
- `render/` or `platform/`: native Win32 Dock host, DirectComposition visual
  tree, Direct2D/DirectWrite drawing, compositor timing, transparent window
  chrome, hit-test region wiring.
- `dock/`: Dock state machine, icon model, pinned item management, running app
  mapping, Windows-default click decisions, ordering, persistence.
- `shell/`: Win32 window enumeration, DWM thumbnails, Shell Link parsing, icon
  extraction, taskbar pinned item reading and later sync, native taskbar
  hide/restore.
- `media/`: GSMTC session discovery, current session selection, playback
  snapshots, media transport commands.
- `lyrics/`: LRC parsing, lyric matching, local lyric index, cache, no-lyrics
  fallback.
- `platform/`: primary-display placement, DPI, system theme, high contrast,
  reduced motion, and window z-order policy.
- `infra/`: logging, path management, JSON config, error handling, performance
  sampling.

## Hard Boundaries

- UI view-model code must not call Win32, COM, DWM, DirectComposition,
  Direct2D, DirectWrite, Shell APIs, or GSMTC directly. Native rendering code
  must live behind render/platform adapters.
- `dock/` must not call WinRT or COM directly.
- `media/` must not depend on Dock state.
- `lyrics/` must never block the media panel.
- `shell/` adapts system APIs and must not store product state.
- Every system API call must sit behind an interface or adapter that can be
  mocked or isolated in tests.
- Apple Music means Apple Music for Windows through GSMTC only.
- Lyrics must not depend on private Apple interfaces.

## Implementation Policy

Use mature community approaches when they reduce risk and their licenses,
maintenance status, and architecture fit are acceptable. Do not copy code or
adopt a new framework without checking license and integration cost. Prefer
native Windows behavior where it already provides the requested taskbar
semantics.

## Legacy Policy

Do not use the removed Qt implementation as v2 context. If an explicit recovery
task needs the old source, use Git history or the local archive outside this
workspace, then keep any findings out of active v2 docs unless they become
formal v2 requirements.

## Workflow Tools

OpenSpec (`openspec/`): schema-driven changes via `/opsx-*` commands
(docs in `.opencode/commands/`, skills in `.opencode/skills/`).

## Code Discovery

Prefer codebase-memory-mcp for code discovery when available:

1. `search_graph`
2. `trace_path`
3. `get_code_snippet`
4. `query_graph`
5. `get_architecture`

Use text search for docs, configs, string literals, and cases where graph
results are insufficient.
