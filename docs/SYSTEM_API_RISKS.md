# System API Risks

Dock_WMac v1.0.0 depends on Windows taskbar, Shell, DWM, DirectComposition,
Direct2D/DirectWrite, and window-management behavior. This document records
known API risks and required fallbacks before implementation starts.

## Risk Table

| Area | Risk | Required handling |
|---|---|---|
| Foreground switching | Windows restricts which processes can call `SetForegroundWindow`; even valid callers can be denied. | Treat foreground activation as best effort. Restore/show first, request foreground, and show fallback state if Windows refuses. Do not simulate unsafe input just to force focus. |
| Window enumeration | `EnumWindows` enumerates top-level desktop windows and does not enumerate child windows. Some system or modern app windows need filtering. | Build a shell adapter that filters visibility, ownership, cloaked state, process identity, and window styles. Keep fake observations for tests. |
| Taskbar identity | Taskbar grouping relies heavily on AppUserModelID, while shortcuts and windows do not always expose the same identity source. | Read shortcut, window, and process AppUserModelIDs. If either side lacks an explicit ID, match only a unique target/icon executable alias. Never merge different explicit AUMIDs or group by window title. |
| Taskbar pinned items | Reading pinned items is practical through Shell shortcuts, but writing/syncing system taskbar pins is not guaranteed as a stable public workflow. | v1.0.0 reads system pins and persists Dock pin state locally. System sync is a later best-effort feature and must never corrupt Dock state. |
| Native pinned order | Public Shell shortcut enumeration does not expose the taskbar's internal visual ordering contract. | Import the pinned set, then make Dock-owned durable ordering authoritative. Do not parse undocumented `Taskband` binary state merely to imitate native order. |
| Taskbar extension state | Applications send progress, overlay icons, thumbnail toolbars, and tab registration to Explorer through `ITaskbarList3/4`; an independent Dock is not a public observer of those calls. | Keep the native taskbar available for shell-owned extension state. Do not inject into Explorer or proxy COM interfaces to claim unsupported parity. |
| Jump Lists | Public APIs let an application manage its own Jump List, but do not provide a supported general-purpose reader for every other application's destinations and tasks. | Leave authoritative Jump Lists on the native taskbar. Dock context menus expose only window, pin, launch, and safe close commands backed by public information. |
| DWM previews | `DwmRegisterThumbnail` requires top-level source/destination windows, and destination must be the desktop or owned by this process. | Create preview host windows owned by Dock_WMac; unregister thumbnails promptly; show fallback UI for unsupported windows. |
| Dock compositor host | Transparent non-activating windows, DirectComposition target lifetime, Direct2D device loss, and DPI changes can desynchronize visuals from hit testing. | Keep one native Dock host as the compositor owner, rebuild graphics devices on device loss, recompute hit regions on layout/DPI changes, and keep UI state outside renderer objects. |
| Minimized/cloaked windows | DWM source size and preview availability can fail or produce unusable results. | Detect and label unavailable previews; do not block hover/click handling while preview data resolves. |
| Explorer restart | Taskbar and Shell state can be recreated after Explorer restarts. | Listen for taskbar creation notifications where needed, re-read pins, rebuild observations, and preserve Dock state. |
| Explorer taskbar mutation | Applying regions or styles directly to `Shell_TrayWnd` can break after Windows updates, interfere with taskbar composition, and require Explorer recovery. | v1.0.0 uses an independent native Dock host. RoundedTB-style taskbar HWND shaping is reference material only; do not mutate Explorer taskbar regions or styles. |
| DPI/work area | Taskbar position, work area, and DPI can change while the app runs. | Recompute placement on display/DPI/theme changes. Do not persist raw pixel geometry as permanent truth. |
| Deployment runtime | Unpackaged Windows App SDK apps need bootstrapper/runtime handling. | Keep release files in one app folder and validate runtime/bootstrapper startup on clean Windows machines. |
| Self-contained option | Self-contained Windows App SDK output copies runtime dependencies next to the executable and increases bundle size. | Keep it as fallback only if bootstrapper deployment proves unreliable. |

## Reference Project Policy

Dock_WMac is an independent implementation of documented Windows behavior and
public Windows APIs. It is not a decompilation of Windows or macOS and does not
copy private shell implementation code.

- RoundedTB is a behavioral reference for monitor discovery, taskbar recreation,
  reversible changes, dynamic sizing, and failure recovery. It is archived,
  GPL-3.0, and modifies Explorer taskbar windows, so its source is not copied and
  its taskbar-region mutation strategy is not used by the production Dock.
- TaskbarX is a reference for centered icon motion and multi-monitor edge cases.
  Its Windows 11 compatibility limitations make it unsuitable as the Dock host.
- Windhawk taskbar mods and the Windows 11 taskbar styling guide are compatibility
  research for version-sensitive Explorer behavior. Injection, symbol hooks, and
  private implementation patching are outside the production architecture.
- Microsoft Shell, DWM, DirectComposition, Direct2D, window-management, and
  accessibility documentation is authoritative whenever a community project
  conflicts with documented platform behavior.

## Official References

- `SetForegroundWindow` restrictions:
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow
- `EnumWindows` top-level window enumeration:
  https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-enumwindows
- AppUserModelID and taskbar grouping:
  https://learn.microsoft.com/en-us/windows/win32/shell/appids
- Process AppUserModelID lookup:
  https://learn.microsoft.com/en-us/windows/win32/api/appmodel/nf-appmodel-getapplicationusermodelid
- Taskbar behavior, work area, taskbar buttons, and taskbar recreation:
  https://learn.microsoft.com/en-us/windows/win32/shell/taskbar
- DWM thumbnail registration:
  https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmregisterthumbnail
- DirectComposition:
  https://learn.microsoft.com/en-us/windows/win32/directcomp/directcomposition-portal
- Direct2D:
  https://learn.microsoft.com/en-us/windows/win32/direct2d/direct2d-portal
- Known folder path retrieval:
  https://learn.microsoft.com/en-us/windows/win32/api/shlobj_core/nf-shlobj_core-shgetknownfolderpath
- Windows App SDK deployment architecture and bootstrapper:
  https://learn.microsoft.com/en-us/windows/apps/windows-app-sdk/deployment-architecture
- Windows App SDK self-contained deployment:
  https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/self-contained-deploy/deploy-self-contained-apps
