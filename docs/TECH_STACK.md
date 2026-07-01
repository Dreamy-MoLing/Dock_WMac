# Tech Stack

Dock_WMac v2 is Windows-only.

## Fixed Stack

- Language: C++20.
- UI: WinUI 3 XAML.
- Windows projection: C++/WinRT.
- SDK: Windows App SDK Stable.
- Compiler: MSVC v143.
- IDE: Visual Studio 2022.
- Local validation may use Visual Studio Insiders / VS2026 with the installed
  `v145` toolset, provided the project remains on the same C++/WinRT, WinUI 3,
  and Windows App SDK line.
- OS target: Windows 10 1809+ and Windows 11.
- System APIs: Win32, COM, DWM, Shell API, and GSMTC only through adapters.

## Initial Scaffold Packages

The v2 scaffold pins packages so the project can be restored by Visual Studio:

- `Microsoft.WindowsAppSDK` version `1.8.260529003`
- `Microsoft.Windows.CppWinRT` version `2.0.250303.1`

These are preparation-stage pins. Upgrade them only in a dedicated toolchain
change that verifies Visual Studio 2022, restore, build, and runtime startup.

## Not Allowed as Main UI

- Electron.
- Qt.
- WPF.
- Avalonia.
- WebView.

## Build Model

The v2 entry is `Dock_WMac_v2.sln`. The v1 CMake build is archived under
`legacy/qt-v1/` for historical reference only.

Command-line build from a Developer PowerShell:

```powershell
msbuild Dock_WMac_v2.sln /restore /p:Configuration=Debug /p:Platform=x64
```

Local VS2026 verification command:

```powershell
& "C:\Program Files\Microsoft Visual Studio\18\Insiders\MSBuild\Current\Bin\amd64\MSBuild.exe" Dock_WMac_v2.sln /restore /p:Configuration=Debug /p:Platform=x64 /p:PlatformToolset=v145
```

The current minimal target creates only `DockWindow`. XAML files exist as v2 UI
placeholders; the first scaffold builds its semi-transparent content in C++ so
toolchain and runtime linkage are verified before full XAML view wiring.

## API Policy

- GSMTC is the only first-stage media session source.
- Apple Music means Apple Music for Windows observed through GSMTC.
- Lyrics must not depend on private Apple APIs.
- External lyric sources require explicit user opt-in.
- Native taskbar hide/restore is experimental and disabled by default.
