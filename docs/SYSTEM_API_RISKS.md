# System API Risks

Dock_WMac v1.0.0 depends on Windows taskbar, Shell, DWM, and window-management
behavior. This document records known API risks and required fallbacks before
implementation starts.

## Risk Table

| Area | Risk | Required handling |
|---|---|---|
| Foreground switching | Windows restricts which processes can call `SetForegroundWindow`; even valid callers can be denied. | Treat foreground activation as best effort. Restore/show first, request foreground, and show fallback state if Windows refuses. Do not simulate unsafe input just to force focus. |
| Window enumeration | `EnumWindows` enumerates top-level desktop windows and does not enumerate child windows. Some system or modern app windows need filtering. | Build a shell adapter that filters visibility, ownership, cloaked state, process identity, and window styles. Keep fake observations for tests. |
| Taskbar identity | Taskbar grouping relies heavily on AppUserModelID. Some apps rely on system-assigned IDs that apps cannot retrieve. | Use explicit AppUserModelID when available; otherwise fall back to executable path, process identity, window class/title heuristics, and stable local mapping. |
| Taskbar pinned items | Reading pinned items is practical through Shell shortcuts, but writing/syncing system taskbar pins is not guaranteed as a stable public workflow. | v1.0.0 reads system pins and persists Dock pin state locally. System sync is a later best-effort feature and must never corrupt Dock state. |
| DWM previews | `DwmRegisterThumbnail` requires top-level source/destination windows, and destination must be the desktop or owned by this process. | Create preview host windows owned by Dock_WMac; unregister thumbnails promptly; show fallback UI for unsupported windows. |
| Minimized/cloaked windows | DWM source size and preview availability can fail or produce unusable results. | Detect and label unavailable previews; do not block hover/click handling while preview data resolves. |
| Explorer restart | Taskbar and Shell state can be recreated after Explorer restarts. | Listen for taskbar creation notifications where needed, re-read pins, rebuild observations, and preserve Dock state. |
| DPI/work area | Taskbar position, work area, and DPI can change while the app runs. | Recompute placement on display/DPI/theme changes. Do not persist raw pixel geometry as permanent truth. |
| Deployment runtime | Unpackaged Windows App SDK apps need bootstrapper/runtime handling. | Keep release files in one app folder and validate runtime/bootstrapper startup on clean Windows machines. |
| Self-contained option | Self-contained Windows App SDK output copies runtime dependencies next to the executable and increases bundle size. | Keep it as fallback only if bootstrapper deployment proves unreliable. |

## Official References

- `SetForegroundWindow` restrictions:
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow
- `EnumWindows` top-level window enumeration:
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumwindows
- AppUserModelID and taskbar grouping:
  https://learn.microsoft.com/en-us/windows/win32/shell/appids
- Taskbar behavior, work area, taskbar buttons, and taskbar recreation:
  https://learn.microsoft.com/en-us/windows/win32/shell/taskbar
- DWM thumbnail registration:
  https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmregisterthumbnail
- Known folder path retrieval:
  https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
- Windows App SDK deployment architecture and bootstrapper:
  https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/deployment-architecture
- Windows App SDK self-contained deployment:
  https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/self-contained-deploy/deploy-self-contained-apps
