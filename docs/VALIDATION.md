# Validation

Dock_WMac v2 is Windows-only. Validation must prove the real Windows App SDK
project builds and runs; screenshots or mock previews do not count as release
evidence.

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

## Runtime Check

Run the Debug build from Visual Studio:

- One `DockWindow` opens.
- The window is blank and semi-transparent.
- Closing the window exits cleanly.
- No GSMTC access is attempted.
- No native taskbar hide/restore is attempted.
- No external network lyric lookup is attempted.

## Dock State Dump Check

The diagnostic dump must run without starting the WinUI Dock window and must
not rewrite product state.

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
- No `DockWindow` appears while the command runs.
- `dock_state.json` is not created or rewritten by the dump command.
- The JSON includes `systemTaskbarPinnedItems`, `localPins`,
  `hiddenSystemPins`, `enumeratedWindows`, and `dockItems`.
- `enumeratedWindows` includes taskbar candidates and filtered windows with
  `isTaskbarCandidate` and `filteredReason`.
- `dockItems` distinguishes pinned items from running-only transient items with
  `pinned` and `transientRunningOnly`.

## Manual Product Checks

- High DPI: 100%, 125%, 150%, 200%.
- Multi-monitor: primary, secondary, display disconnect/reconnect.
- Windows theme: light, dark, high contrast.
- Reduced motion enabled.
- Explorer restart.
- Clean user profile with no local config.

## v1.0.0 Release Gates

- Launch, switch, restore, minimize, and multi-window handling follow Windows
  taskbar defaults in normal daily scenarios.
- System taskbar pinned items import as Dock pinned items.
- Dock pin/order state persists safely across restart.
- DWM previews render for available windows and degrade cleanly for minimized,
  cloaked, or unavailable windows.
- Hover magnification, drag sorting, auto-hide, and reduced-motion behavior are
  visually checked.
- DPI, theme, Explorer restart, shutdown, and clean profile checks pass.
- Unpackaged Win32 + Windows App SDK bootstrapper packaging installs and runs.

## Post-v1 Gates

- `dock/` state tests.
- `lyrics/` parser tests.
- `media/` fake session tests.
- `shell/` Windows integration tests.
- UI smoke run on Windows 10 and Windows 11.
- Dock pin state recovery tests before pin editing can ship.
- Taskbar pin sync tests before system sync can ship.
- Native taskbar recovery tests before taskbar hiding can ship.
- Packaging validation for unpackaged Win32 + Windows App SDK bootstrapper.
