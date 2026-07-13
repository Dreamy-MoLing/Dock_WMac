# Product Spec

Dock_WMac v2 is a Windows-only Mac-style taskbar for Windows. It is a desktop
multi-function taskbar that keeps Windows taskbar behavior, app/window
semantics, and system expectations while adding a centered floating Dock visual,
launcher, and window switcher.

The user provides product requirements. Engineering agents choose the
implementation details needed to satisfy those requirements, using local
validation and official platform behavior instead of asking the user to lock
technical choices prematurely.

If an ad-hoc prompt conflicts with this product direction, Windows default
taskbar behavior, or validated implementation constraints, the documented
product direction wins. The correct response is to repair incomplete docs or
implementation, not to follow a prompt into a worse product shape.

## Goals

- Provide fast app launch and window switching for Windows 10 1809+ and
  Windows 11.
- Render the Dock on the Windows primary display only. Display topology changes
  move it to whichever display Windows marks as primary; per-monitor Dock
  instances and secondary-display placement are outside v1.0.0.
- Match Windows taskbar functionality and public integration behavior wherever
  Windows defines launch, pinning, grouping, preview, restore, minimize,
  foreground, ordering, and taskbar recovery semantics.
- Keep native taskbar behavior understandable and recoverable.
- Preserve the native taskbar as the authoritative surface for shell-owned
  features that an independent process cannot observe through public APIs,
  including third-party taskbar progress, overlays, tab registration, thumbnail
  toolbars, and arbitrary application Jump Lists.
- Ship v1.0.0 as a complete Dock-style Windows taskbar extension with polished
  UI and animation.
- Add icon magnification, auto-hide, drag sorting, pinned apps, running app
  indicators, DWM previews, Windows-default switching, multi-window handling,
  and durable persistence.
- Keep all system integration behind replaceable adapters.
- Prefer stable public Windows APIs and Windows App SDK Stable.
- Render the Dock chrome through native Windows composition and drawing APIs,
  not through transparent XAML windows.
- Keep steady-state CPU, memory, handle, thread, and GPU usage low enough that
  the Dock feels like system chrome rather than a heavy desktop app.

## Non-Goals

- Do not replace Explorer, Start, notification area, clock, quick settings, or
  the full Windows taskbar.
- Do not build a complete Shell replacement or window manager.
- Do not make a cross-platform app.
- Do not add an account system, telemetry, cloud sync, plugin marketplace, or
  theme store during v2 foundation work.
- Do not add heavy runtimes, persistent browser engines, background services,
  or speculative subsystems that raise memory use without serving v1.0.0
  taskbar behavior.
- Do not use private Apple APIs for music or lyrics.
- Do not hide the native taskbar by default.
- Do not create one Dock per monitor or offer secondary-display placement in
  v1.0.0.

## v1.0.0 Release Requirements

v1.0.0 is the first formal release target. It must be usable as a Windows
taskbar extension, not just a scaffold.

- Polished Dock-style surface with responsive hover, magnification, ordering,
  running indicators, and predictable auto-hide.
- The Dock shelf and animated icon row are separate compositor layers inside a
  native Dock host, not separate XAML pages. The shelf layer owns the
  translucent rounded rail; the icon layer owns icon animation, indicators, hit
  testing, drag sorting, and previews.
- Icon hover animation follows the icon centerline. The fixed geometric
  reference is the perpendicular foot from the icon centerline to the Dock
  shelf edge, and the maximum hover pose clamps the icon outer circle tangent
  to that shelf edge.
- Hover enter and exit are eased without changing the tangent geometry at any
  intermediate frame. Frame scheduling stops at rest and must not create an
  idle 60 FPS loop. Reduced-motion preserves the terminal pose and removes the
  transition time.
- App launch, foreground, restore, minimize, and multi-window behavior follows
  Windows taskbar defaults.
- A running window that belongs to a pinned shortcut updates that pinned item;
  it must not create a duplicate running-only icon. Explicit AUMIDs define
  grouping. When only one side exposes an explicit AUMID, a unique executable
  alias may join the window to a path-identified pin. Different explicit
  AUMIDs and ambiguous executable aliases must remain separate.
- Middle-click and Shift+left-click request a new instance as on the Windows
  taskbar. The target application may still enforce a single instance.
- App item context commands support pinning and safe close requests for one
  window or every window in the app group without terminating the process.
- Runtime resource use is part of release quality. Idle Dock operation must not
  depend on high-frequency polling, hidden web runtimes, duplicate UI hosts, or
  retained objects that are not needed for visible behavior.
- Taskbar fixed items are read as the default pinned source.
- Dock pin edits persist safely inside Dock first. System taskbar sync is a
  later capability and must not break Dock state.
- Running app recognition maps windows, processes, and app identities well
  enough for normal daily use.
- DWM previews work for available windows and degrade cleanly for minimized,
  cloaked, or unavailable windows.
- Settings, layout, ordering, and pinned state survive restart.
- The app handles DPI, theme, reduced motion, Explorer restart, and clean
  shutdown without taskbar or window-state conflicts.
- Release packaging uses unpackaged Win32 with Windows App SDK bootstrapper.

Media panel, lyrics, and audio-reactive visuals are prepared architecturally but
are not v1.0.0 release blockers.

## Core Requirements

### Dock

- Centered floating window.
- Translucent Dock surface.
- Fisheye icon magnification.
- Auto-hide with predictable reveal.
- Drag sorting persisted through Dock state.
- Default pinned app source is the system taskbar.
- User pin and unpin changes must preserve Dock state first. System taskbar sync
  is a follow-up capability and must never corrupt or discard Dock state if the
  system sync path fails.
- Pinned app model remains separate from running app model.
- Multi-window app support.
- Click behavior follows Windows taskbar defaults for launch, switch, minimize,
  restore, foreground, and multi-window selection.

### Windows Integration

- Read system taskbar pinned items.
- Add system taskbar sync only after Dock pin state is durable and recoverable.
- Enumerate top-level windows.
- Resolve process, executable path, app identity, and window ownership.
- Treat visible, eligible top-level windows as taskbar candidates even when
  their title is empty; titles are presentation text, never grouping identity.
- Render DWM thumbnails for previews.
- Minimized windows remain eligible for taskbar-style previews when Windows/DWM
  can supply a thumbnail. Preview hover must not restore the window.
- Extract icons through Shell APIs.
- Optional native taskbar hide/restore with explicit user opt-in.
- Recover taskbar visibility after crashes or failed startup.

### Post-v1 Media Panel

- Use GSMTC only.
- Prefer validating with Apple Music for Windows first, but never hard-code it.
- Show artwork, title, artist, progress, playback state, and transport controls.
- Expand sideways from the Dock as a visually connected panel.
- Degrade cleanly when no session, artwork, or control permission exists.

### Post-v1 Lyrics

- Prefer local LRC.
- Support local txt fallback.
- External lyric providers are future opt-in only.
- Missing lyrics must not break media controls or artwork display.
- Lyrics render as upward fading flame/fog-like motion, not static text blocks.

### Post-v1 Audio Visuals

- Use smooth wave or energy-flow visuals.
- Avoid basic bar meters as the primary design.
- Seed color from artwork when available.
- Allow pseudo-response animation before real audio capture is added.

## v1.0.0 Acceptance Direction

- The real Windows application builds, launches, and renders the Dock through a
  native Win32 compositor host.
- Dock behavior follows Windows taskbar defaults where Windows defines an app,
  window, preview, restore, minimize, or multi-window policy.
- Visual polish, window layering, icon animation, taskbar previews, and state
  persistence are validated on the real app, not only in prototypes.
