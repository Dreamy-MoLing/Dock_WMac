# UX Spec

Dock_WMac v2 is a Windows-only desktop Dock. The experience should feel native
to Windows while using a compact, polished floating Dock surface.

## Dock Window

- Default position: horizontally centered near the primary display bottom edge.
- Shape: translucent rounded surface with depth, but no heavy decorative chrome.
- Size: compact enough to keep desktop content visible.
- Visibility: native taskbar remains visible by default.
- Naming: the first v2 window class is `DockWindow`.

## Dock Items

- Pinned and running apps share one visual row.
- Running state is visible but subtle.
- Hover magnification is smooth and localized.
- Drag sorting must not resize or shift unrelated UI unpredictably.
- Multi-window apps open a preview or chooser instead of guessing silently.

## Click Behavior

Click decisions must remain predictable:

- Not running: launch.
- Running with one visible window: foreground or restore.
- Foreground window clicked again: minimize only when that matches the configured
  taskbar-like behavior.
- Multiple windows: show preview/chooser first.
- Launch failures: show a recoverable error and keep Dock responsive.

## Auto-Hide

- Disabled until the basic Dock is stable.
- Reveal trigger must be forgiving across DPI and multi-monitor setups.
- Native taskbar hiding is a separate opt-in setting and must be reversible.

## Window Previews

- Use DWM thumbnails through `shell/`.
- Preview rendering must not block Dock hover or click handling.
- Cloaked, minimized, and unavailable windows need clear fallback states.

## Media Panel

- Uses GSMTC snapshots from `media/`.
- Apple Music for Windows is a priority validation target only.
- Layout shows artwork, title, artist, progress, play state, and transport
  controls.
- Missing session collapses naturally.
- Missing artwork shows a local placeholder.

## Lyrics

- Local LRC is preferred.
- Local txt is supported as untimed fallback.
- Missing lyrics leaves the player fully functional.
- Visual direction: upward fade with flame/fog-like softness.
- Lyric loading must not block controls or progress updates.

## Audio Visuals

- Primary direction: smooth waves or energy flow.
- Avoid a basic equalizer-bar look.
- Artwork color extraction can seed the palette.
- Reduced-motion settings must disable or simplify movement.

## Accessibility and System Settings

- Respect high contrast.
- Respect reduced motion.
- Support keyboard access for menus and player controls.
- Do not rely on color alone for running, active, or error states.
