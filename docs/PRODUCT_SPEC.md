# Product Spec

Dock_WMac v2 is a Windows-only Dock-style taskbar extension. It is a desktop
multi-function taskbar that keeps the core Windows taskbar workflow while adding
a centered floating launcher and window switcher with a polished Dock surface.

The user provides product requirements. Engineering agents choose the
implementation details needed to satisfy those requirements, using local
validation and official platform behavior instead of asking the user to lock
technical choices prematurely.

## Goals

- Provide fast app launch and window switching for Windows 10 1809+ and
  Windows 11.
- Keep native taskbar behavior understandable and recoverable.
- Ship v1.0.0 as a complete Dock-style Windows taskbar extension with polished
  UI and animation.
- Add icon magnification, auto-hide, drag sorting, pinned apps, running app
  indicators, DWM previews, Windows-default switching, multi-window handling,
  and durable persistence.
- Keep all system integration behind replaceable adapters.
- Prefer stable public Windows APIs and Windows App SDK Stable.

## Non-Goals

- Do not replace Explorer, Start, notification area, clock, quick settings, or
  the full Windows taskbar.
- Do not build a complete Shell replacement or window manager.
- Do not make a cross-platform app.
- Do not add an account system, telemetry, cloud sync, plugin marketplace, or
  theme store during v2 foundation work.
- Do not use private Apple APIs for music or lyrics.
- Do not hide the native taskbar by default.

## v1.0.0 Release Requirements

v1.0.0 is the first formal release target. It must be usable as a Windows
taskbar extension, not just a scaffold.

- Polished Dock-style surface with responsive hover, magnification, ordering,
  running indicators, and predictable auto-hide.
- App launch, foreground, restore, minimize, and multi-window behavior follows
  Windows taskbar defaults.
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
- Render DWM thumbnails for previews.
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

## Acceptance for Preparation Checkpoint

- v2 docs exist and consistently describe the Windows-only rewrite.
- Architecture boundaries are explicit.
- A Visual Studio 2022 solution exists for a preparation `DockWindow`.
- The window scaffold does not implement Dock, GSMTC, taskbar hiding, lyrics, or
  audio analysis yet.
