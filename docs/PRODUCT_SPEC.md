# Product Spec

Dock_WMac v2 is a Windows-only desktop Dock. It keeps the core Windows taskbar
workflow while adding a centered floating launcher and window switcher with a
polished Dock-style surface.

## Goals

- Provide fast app launch and window switching for Windows 10 1809+ and
  Windows 11.
- Keep native taskbar behavior understandable and recoverable.
- Add icon magnification, auto-hide, drag sorting, pinned apps, running app
  indicators, DWM previews, a media panel, lyrics, and audio-reactive visuals.
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

## Core Requirements

### Dock

- Centered floating window.
- Translucent Dock surface.
- Fisheye icon magnification.
- Auto-hide with predictable reveal.
- Drag sorting persisted through Dock state.
- Pinned app model separate from running app model.
- Multi-window app support.
- Click behavior chooses launch, switch, minimize, restore, or foreground.

### Windows Integration

- Read taskbar pinned items.
- Enumerate top-level windows.
- Resolve process, executable path, app identity, and window ownership.
- Render DWM thumbnails for previews.
- Extract icons through Shell APIs.
- Optional native taskbar hide/restore with explicit user opt-in.
- Recover taskbar visibility after crashes or failed startup.

### Media Panel

- Use GSMTC only.
- Prefer validating with Apple Music for Windows first, but never hard-code it.
- Show artwork, title, artist, progress, playback state, and transport controls.
- Degrade cleanly when no session, artwork, or control permission exists.

### Lyrics

- Prefer local LRC.
- Support local txt fallback.
- External lyric providers are future opt-in only.
- Missing lyrics must not break media controls or artwork display.
- Lyrics render as upward fading flame/fog-like motion, not static text blocks.

### Audio Visuals

- Use smooth wave or energy-flow visuals.
- Avoid basic bar meters as the primary design.
- Seed color from artwork when available.
- Allow pseudo-response animation before real audio capture is added.

## Acceptance for Preparation Stage

- v2 docs exist and consistently describe the Windows-only rewrite.
- Architecture boundaries are explicit.
- A Visual Studio 2022 solution exists for a minimal WinUI 3 `DockWindow`.
- The window scaffold does not implement Dock, GSMTC, taskbar hiding, lyrics, or
  audio analysis yet.
