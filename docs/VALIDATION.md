# Validation

Dock_WMac v2 is Windows-only. Validation must prove the real native Dock host
builds and runs; screenshots or mock previews do not count as release evidence.

## Environment Check

From PowerShell:

```powershell
& "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" -all -products *
```

Required:

- Visual Studio 2022 with MSVC v143 C++ workload, or local Visual Studio
  Insiders / VS2026 with the installed `v145` toolset for machine validation.
- Windows SDK.
- NuGet restore access.
- Windows 10 1809+ or Windows 11.

## Build Check

From Developer PowerShell:

```powershell
msbuild Dock_WMac_v2.sln /restore /p:Configuration=Debug /p:Platform=x64
msbuild Dock_WMac_v2.sln /restore /p:Configuration=Release /p:Platform=x64
```

Local Visual Studio Insiders / VS2026 Debug check:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" Dock_WMac_v2.sln /restore /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145
```

Pass criteria:

- NuGet restore succeeds.
- C++/WinRT generated files compile.
- `Dock_WMac_v2.exe` is produced.
- No Qt build is required for v2.
- The production Dock surface is native Win32 plus DirectComposition and
  Direct2D/DirectWrite, not XAML Dock chrome.

## Runtime Check

Run the Debug build from Visual Studio:

- The Dock appears as one user-facing native compositor surface with separate
  shelf and icon compositor layers.
- No default white or opaque host-window background is visible outside the Dock
  shelf or icon areas.
- Icon hover magnification remains centered on each icon's centerline and is
  not clipped at maximum scale.
- Hover entry and exit are visibly eased, do not overshoot, preserve the shelf
  tangent on intermediate frames, and leave no running animation timer at rest.
- Minimized windows remain eligible for DWM preview hover without being
  restored.
- Idle CPU, memory, handles, and thread count stay low and stable for a Dock
  surface.
- Closing the window exits cleanly.
- Right-clicking empty shelf space exposes placement, automatic hiding, and
  Exit; placement/auto-hide changes apply immediately and survive restart.
- Right-clicking a running app exposes its windows, pin state, and normal close
  command; grouped apps expose `Close all windows` without force-terminating
  the owning process.
- Launching a pinned app changes that same icon to running and does not append a
  duplicate icon. Test an explicit-AUMID shortcut, an unpackaged EXE shortcut,
  and an Explorer-style shortcut with no direct target.
- Middle-click and Shift+left-click request a new instance without changing the
  normal left-click activation/minimize behavior.
- No GSMTC access is attempted.
- No native taskbar hide/restore is attempted.
- No external network lyric lookup is attempted.

## Dock State Dump Check

The diagnostic dump must run without starting the native Dock host or secondary
UI windows and must not rewrite product state.

From PowerShell:

```powershell
$state = Join-Path $env:LOCALAPPDATA "Dock_WMac\dock_state.json"
$before = if (Test-Path $state) { (Get-Item $state).LastWriteTimeUtc } else { $null }
build\v2\x64\Debug\Dock_WMac_v2.exe --dump-dock-state
$after = if (Test-Path $state) { (Get-Item $state).LastWriteTimeUtc } else { $null }
Get-ChildItem "$env:LOCALAPPDATA\Dock_WMac\diagnostics\dock-state-*.json" |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1
```

Pass criteria:

- A new `%LOCALAPPDATA%\Dock_WMac\diagnostics\dock-state-*.json` file exists.
- No Dock surface appears while the command runs.
- `dock_state.json` is not created or rewritten by the dump command.
- The JSON includes `systemTaskbarPinnedItems`, `importedTaskbarPins`,
  `localPins`, `hiddenSystemPins`, `enumeratedWindows`, and `dockItems`.
- `enumeratedWindows` includes taskbar candidates and filtered windows with
  `isTaskbarCandidate` and `filteredReason`.
- `dockItems` distinguishes pinned items from running-only transient items with
  `pinned` and `transientRunningOnly`.

## Manual Product Checks

- High DPI: 100%, 125%, 150%, 200%.
- Display topology: Dock remains on the Windows primary display after a
  secondary display is connected or disconnected and follows a new primary
  display selection without creating another Dock instance.
- Windows theme: light, dark, high contrast.
- Reduced motion enabled.
- With reduced motion enabled, enter/leave reaches the same terminal geometry
  immediately and does not start the hover frame timer.
- Explorer restart.
- Clean user profile with no local config.

## DPI And System Appearance Check

Unit tests must verify logical-to-physical scaling at 96, 120, 144, and 192 DPI
and Dock placement within a work area whose origin is not `(0, 0)`. The native
host self-check must dispatch `WM_SETTINGCHANGE` and `WM_THEMECHANGED` through
the production Dock HWND and verify that both reach the runtime settings
callback.

For the final manual gate, set the primary display to 100%, 125%, 150%, and
200% scaling, then switch Windows between light, dark, high contrast, and
reduced-motion settings while the Dock remains running. Connecting or
disconnecting a secondary display must not move the Dock away from the primary
display. Changing which display is primary must move the single Dock instance
to the new primary display. Placement and rendering must update without blank
frames, clipped icons, stale colors, or changed hover tangent geometry.

## Explorer Recovery Check

The automated self-check must dispatch the registered `TaskbarCreated` message
through the real native Dock HWND and verify that the Shell environment callback
runs exactly once. Dock model tests must verify that replacing the imported
taskbar-pin snapshot preserves Dock ordering, local pins, and hidden-system-pin
state, and that an unchanged snapshot does not rewrite state.

For the final manual gate, restart Explorer while the packaged Dock is running.
The Dock must remain responsive, re-read taskbar pins and top-level windows, and
must not lose or reorder Dock-owned state. A Shell refresh must not create a
second Dock process or renderer.

## DWM Preview Integration Check

The automated self-check must create real top-level source windows and use the
production DWM preview host. It must prove that:

- a minimized source registers a real DWM thumbnail rather than being filtered
  out before registration;
- showing the preview does not restore or activate the minimized source;
- clicking the preview through its native HWND restores the selected source;
- a two-window preview group registers both thumbnails and clicking the second
  card activates the second source only.

Unavailable and cloaked sources must continue to use the non-blocking fallback
card path without custom warning dialogs.

## Resource Check

Run the packaged native Dock for a five-minute idle measurement:

```powershell
.\scripts\validate-v1-resource-stability.ps1 -DurationSeconds 300
```

The command starts the real packaged Dock, samples it after warmup, writes a
machine-readable report under `artifacts\validation`, and fails when private
bytes, handles, threads, normalized idle CPU, or child-process count exceed the
documented thresholds. `--dump-resource-metrics` remains a point-in-time
diagnostic and does not by itself prove runtime stability.

After the idle run, repeat normal hover, preview, launch, restore, minimize, and
multi-window actions while sampling the process in Task Manager or Process
Explorer. The automated idle report and the interaction check are both release
evidence.

Pass criteria:

- No sustained idle CPU activity beyond normal compositor wakeups.
- Working set and private bytes do not grow without user-visible cause.
- Handle and thread counts stay stable after repeated hover, preview, and
  launch/switch actions.
- No hidden browser runtime, helper service, or duplicate Dock renderer process
  is present.

## v1.0.0 Release Gates

- Launch, switch, restore, minimize, and multi-window handling follow Windows
  taskbar defaults in normal daily scenarios.
- System taskbar pinned items import as Dock pinned items.
- Dock pin/order state persists safely across restart.
- DWM previews render for available windows and degrade cleanly for minimized,
  cloaked, or unavailable windows.
- Hover magnification, drag sorting, auto-hide, and reduced-motion behavior are
  visually checked, including the icon outer-circle tangent clamp at maximum
  hover.
- Resource checks pass without unexplained memory, handle, thread, or CPU
  growth.
- DPI, theme, Explorer restart, shutdown, and clean profile checks pass.
- The single Dock instance remains on the Windows primary display across
  display-topology changes.
- The native x64 unpackaged Win32 release runs without a Windows App Runtime,
  WinUI, WebView, ML runtime, or separately installed Visual C++ Redistributable.
- `scripts\package-v1-release.ps1` produces a runtime-only `Dock_WMac` folder,
  `release-manifest.json`, and a `Dock_WMac-*-windows-x64.zip`.
- Release packaging validates the packaged executable with `--self-check`,
  `--dump-dock-state`, and `--dump-resource-metrics`; build-output-only
  validation is not enough for release.

## Post-v1 Gates

- `dock/` state tests.
- `lyrics/` parser tests.
- `media/` fake session tests.
- `shell/` Windows integration tests.
- UI smoke run on Windows 10 and Windows 11.
- Dock pin state recovery tests before pin editing can ship.
- Taskbar pin sync tests before system sync can ship.
- Native taskbar recovery tests before taskbar hiding can ship.
