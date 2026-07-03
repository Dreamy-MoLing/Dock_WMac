# Dock_WMac v2

Dock_WMac v2 is a Windows-only desktop Dock project. The active rewrite targets
C++20, C++/WinRT, WinUI 3, Windows App SDK Stable, MSVC, Visual Studio 2022,
Windows 10 1809+, and Windows 11.

The goal is not to replace Windows Shell. The product is a Dock-style Windows
taskbar extension: a desktop multi-function taskbar that keeps the core Windows
taskbar workflow while adding a centered floating Dock, icon magnification,
auto-hide, app launch and window switching, DWM previews, a polished
side-attached media panel, lyric display, and smooth audio-reactive visuals.

## Current Stage

This repository is in v2 preparation for the first formal release:
`v1.0.0 Taskbar Dock Release`.

- v2 is the current product direction.
- v1.0.0 means a complete Dock-style Windows taskbar extension: usable app
  launch, pin management, running app recognition, window switching,
  multi-window handling, DWM previews, ordering, persistence, and polished Dock
  UI/animation.
- Do not introduce Electron, Qt, WPF, Avalonia, or WebView as the main UI stack.
- Media panel, lyrics, and audio-reactive visuals are post-v1.0.0 extension
  tracks unless the user explicitly changes the release scope.

## v1.0.0 Release Scope

### Dock

- Centered floating Windows desktop window.
- Dock-style translucent visual surface.
- Icon fisheye magnification.
- Auto-hide.
- Drag ordering.
- Pinned apps read from the system taskbar by default.
- User pin changes must preserve Dock state first. System taskbar sync is added
  after that state is stable.
- Running app recognition.
- Click decisions for launch, switch, minimize, restore, foreground, and
  multi-window behavior following Windows taskbar defaults.

### Windows taskbar behavior

- Read taskbar pinned items and later sync Dock pin changes back when safe.
- Enumerate top-level windows.
- Map processes, app identities, and windows.
- Support multi-window apps.
- Show DWM thumbnails.
- Optional native taskbar hide/restore, disabled by default and gated by explicit
  user opt-in.

## Post-v1.0.0 Extension Direction

### Media panel

- Use GSMTC for Windows media sessions.
- First-pass validation should prioritize Apple Music for Windows, without
  hard-coding it.
- Show artwork, title, artist, progress, and play state.
- Support previous, play/pause, and next commands.
- Degrade cleanly when no media session or artwork exists.
- Do not use private Apple APIs.

### Lyrics

- Prefer local LRC.
- Support local txt lyrics.
- Future external lyric sources must be explicit opt-in.
- Media playback must work without lyrics.
- Lyric visuals should fade upward with a flame/fog-like treatment.

### Audio-reactive visuals

- Avoid cheap bar visualizers.
- Target smooth waves, energy flow, and a polished technical feel.
- Color may be seeded from album artwork.
- Phase one may use pseudo-response animation before real audio analysis.

## Repository Layout

```text
Dock_WMac_v2.sln        Visual Studio 2022 solution for v2
src/v2/                 C++20 + C++/WinRT + WinUI 3 source
docs/                   v2 product, architecture, UX, validation, and workflow docs
legacy/                 short note for removed legacy archive context
```

The old Qt implementation is not part of the active workspace context. Keep
active v2 work under `src/v2/`.

## Open and Build

Required environment:

- Visual Studio 2022 with MSVC v143.
- Windows SDK with desktop C++ tooling.
- Windows App SDK Stable package restore through NuGet.
- Windows 10 1809+ or Windows 11.

Open:

```powershell
start Dock_WMac_v2.sln
```

Command-line build from a Developer PowerShell:

```powershell
msbuild Dock_WMac_v2.sln /restore /p:Configuration=Debug /p:Platform=x64
```

This is the Visual Studio 2022 / `v143` baseline. On this machine, Visual
Studio Insiders / VS2026 is also available; the current scaffold has been
verified with:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" Dock_WMac_v2.sln /restore /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145
```

Run the pure Dock model tests:

```powershell
build\v2\x64\Debug\Dock_WMac_v2_tests.exe
```

Run the app self-check:

```powershell
build\v2\x64\Debug\Dock_WMac_v2.exe --self-check
```

## Current Implementation Status

Implemented in the active v2 line:

- WinUI 3 Dock window creation and placement.
- Basic Dock item rendering, hover magnification, animation, and drag ordering.
- Top-level window enumeration and taskbar candidate filtering.
- System taskbar pin reading from `.lnk` shortcuts.
- Local Dock pin/unpin state, hidden system pins, persisted ordering, and JSON
  state loading/saving.
- Running app recognition through app identity, executable path, and pinned app
  metadata.
- Click decisions for launch, activate, foreground minimize, and multi-window
  chooser routing.
- DWM preview host plumbing, command-line self-check, and release folder
  packaging.

Not complete for v1.0.0 yet:

- Full Windows app identity resolution for packaged apps, UWP wrappers,
  launchers, Electron profile variants, and shortcut argument-specific
  identities.
- Native taskbar hide/restore policy and safe opt-in UX.
- Finished multi-window chooser UI behavior.
- v1.0.0 release packaging polish and installer/update story.
- Media panel, lyrics, and audio-reactive visuals; these remain post-v1.0.0
  extension tracks.

## Documentation

- `docs/PRODUCT_SPEC.md` - product requirements and non-goals.
- `docs/ARCHITECTURE.md` - v2 layers and dependency boundaries.
- `docs/UX_SPEC.md` - interaction and visual behavior.
- `docs/TECH_STACK.md` - fixed technology choices and package policy.
- `docs/MIGRATION.md` - legacy-removal and v2 focus notes.
- `docs/ROADMAP.md` - staged delivery plan.
- `docs/V1_RELEASE_SPEC.md` - v1.0.0 release behavior and acceptance.
- `docs/UI_MOTION_BASELINE.md` - confirmed v1.0.0 Dock visual and motion
  baseline.
- `docs/SYSTEM_API_RISKS.md` - Windows API risks and fallback plans.
- `docs/VALIDATION.md` - build, runtime, and release validation.
- `docs/AI_WORKFLOW.md` - rules for future AI-assisted development.

When documents conflict, `AGENTS.md` and the files above define the v2 rewrite.
