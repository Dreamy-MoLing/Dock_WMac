# Tech Stack

Dock_WMac v2 is Windows-only.

The stack is chosen to produce a Windows-facing Mac-style taskbar with Windows
taskbar behavior and low steady-state resource use. Avoid technologies that
solve UI convenience by adding large runtimes, duplicate renderers, hidden
browser processes, or avoidable memory overhead.

## Fixed Stack

- Language: C++20.
- Dock surface: native Win32 popup host rendered with DirectComposition plus
  Direct2D/DirectWrite. This is the production path for the Dock shelf, icons,
  running indicators, shadows, hover magnification, drag visuals, hit testing,
  transparent window chrome, and low-overhead animation.
- Secondary UI: WinUI 3 XAML is allowed for settings, dialogs, menus, and
  non-Dock application surfaces. It must not be the primary renderer for the
  Dock chrome.
- Windows projection: C++/WinRT.
- SDK: Windows App SDK Stable for app/runtime integration and any secondary
  WinUI surfaces. The Dock surface must not depend on XAML transparency or
  XAML window clipping behavior.
- Compiler: MSVC v143.
- IDE: Visual Studio 2022.
- Local validation may use Visual Studio Insiders / VS2026 with the installed
  `v145` toolset, provided the project remains on the same C++20, C++/WinRT,
  Windows App SDK, DirectComposition, and Direct2D line.
- OS target: Windows 10 1809+ and Windows 11.
- System APIs: Win32, COM, DirectComposition, Direct2D, DirectWrite, WIC, DWM,
  Shell API, and GSMTC only through adapters.

## Runtime Packages

The v2 project pins packages so the project can be restored by Visual Studio:

- `Microsoft.WindowsAppSDK` version `1.8.260529003`
- `Microsoft.Windows.CppWinRT` version `2.0.250303.1`

Upgrade them only in a dedicated toolchain change that verifies Visual Studio
2022, restore, build, and runtime startup.

## Not Allowed as Main UI

- Electron.
- Qt.
- WPF.
- Avalonia.
- WebView.
- WinUI XAML for the Dock surface itself.
- Any UI runtime or helper process whose idle memory cost is disproportionate
  to taskbar/Dock behavior.

## Resource Policy

- Prefer single-process, native Windows rendering for v1.0.0.
- Avoid always-on high-frequency timers when compositor-driven animation or
  event-driven refresh is sufficient.
- Cache icons, thumbnails, and layout data with explicit bounds.
- Track process working set, private bytes, handle count, thread count, and CPU
  use during validation. Treat unexplained growth as a release blocker.
- Do not add background services or persistent helper processes for v1.0.0
  taskbar behavior.

## Build Model

The v2 entry is `Dock_WMac_v2.sln`. Root-level build, validation, and release
work use this solution.

Command-line build from a Developer PowerShell:

```powershell
msbuild Dock_WMac_v2.sln /restore /p:Configuration=Debug /p:Platform=x64
```

Local VS2026 verification command:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" Dock_WMac_v2.sln /restore /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145
```

The production Dock host is a native non-activating top-level window. It owns
per-pixel transparency, compositor visuals, render timing, pointer hit testing,
and animation. XAML may coexist for ordinary app windows, but no v1.0.0 Dock
visual requirement should be implemented by stacking transparent WinUI windows.

## Packaging Decision

Default v2 packaging direction: unpackaged Win32 app with Windows App SDK
bootstrapper.

- MSIX packaged app: provides app identity, clean install/uninstall, update
  plumbing, and tighter integration with Windows deployment rules. It is better
  when installer behavior and identity matter more than portable layout.
- Unpackaged Win32 app with Windows App SDK bootstrapper: behaves like a
  traditional desktop app, is easier to zip or install with a custom installer,
  and depends on the Windows App SDK runtime being installed or bootstrapped.
- Self-contained unpackaged app: carries more runtime files for fewer machine
  prerequisites, at the cost of larger output and more packaging weight.

Revisit this only if release, update, or system-integration validation proves
the default shape is the wrong fit.

## Community Solution Policy

Mature community approaches are allowed when they reduce delivery risk. Before
using one, verify license compatibility, active maintenance, Windows behavior,
and fit with the v2 architecture. Do not adopt another main UI framework or copy
code without review. Native Windows APIs remain preferred for taskbar semantics
and Dock chrome.

### RoundedTB reference boundary

RoundedTB is a product and systems-behavior reference for dynamic taskbar
geometry, DPI-aware margins, Explorer recreation, reversible settings, and
failure recovery. It is not an implementation dependency or a source-code
base for Dock_WMac.

Reference repository: https://github.com/RoundedTB/RoundedTB

- RoundedTB modifies Explorer taskbar HWND regions and uses a WPF/.NET process;
  Dock_WMac renders an independent native Dock and does not reshape
  `Shell_TrayWnd` for v1.0.0.
- RoundedTB is archived and its documented dynamic/auto-hide behavior has
  flicker, vertical-placement, and multi-monitor limitations. Those constraints
  are evidence against adopting its taskbar-mutation architecture here.
- RoundedTB is GPL-3.0. Do not copy or adapt its source into Dock_WMac without a
  separate license decision. Product ideas and public Windows API behavior may
  be studied and reimplemented independently.
- Animation quality is determined by the compositor path and frame scheduling,
  not by switching the primary language. C++20 with DirectComposition and
  Direct2D remains the v1 Dock path; another language is acceptable only for an
  isolated secondary surface when measured quality and maintenance benefits
  exceed the added runtime cost.

### Reference portfolio and adoption boundary

- Microsoft taskbar, AppUserModelID, DWM, Shell, DirectComposition, and Win32
  documentation and samples are the normative behavior and API references.
- TranslucentTB is a reference for low-resource lifecycle, taskbar state
  observation, Explorer recreation, and reversible appearance changes:
  https://github.com/TranslucentTB/TranslucentTB
- ExplorerPatcher and Windhawk taskbar mods are references for Windows-version
  compatibility cases and behavior regression coverage:
  https://github.com/valinet/ExplorerPatcher
  https://github.com/ramensoftware/windows-11-taskbar-styling-guide
- Seelen UI and TaskbarX are product/interaction references for Dock workflows,
  animation controls, multi-monitor behavior, and feature inventory:
  https://github.com/eythaann/seelen-ui
  https://github.com/ChrisAnd1998/TaskbarX
- Explorer injection, private symbol hooks, undocumented Taskband parsing, and
  mutation of internal Windows 11 XAML elements are excluded from the v1 core.
  Those techniques require an explicit compatibility and security decision.
- GPL/AGPL project code is not copied into Dock_WMac without a deliberate
  license decision. Tauri/WebView, WPF, and helper-process architectures remain
  references only because they conflict with the native low-resource Dock host.
- Dock_WMac is an independent behavioral implementation built on public Windows
  APIs. Matching observable Windows and macOS interaction conventions is not a
  license to recover, copy, or depend on private Microsoft or Apple code.

## API Policy

- GSMTC is the only first-stage media session source.
- Apple Music means Apple Music for Windows observed through GSMTC.
- Lyrics must not depend on private Apple APIs.
- External lyric sources require explicit user opt-in.
- Native taskbar hide/restore is experimental and disabled by default.
