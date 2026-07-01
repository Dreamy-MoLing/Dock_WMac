# UI Motion Baseline

Status: provisional v1.0.0 Dock UI baseline, confirmed by user on 2026-07-01.

This document captures the visual and interaction baseline for the
`v1.0.0 Taskbar Dock Release`. It is a design/behavior reference for the WinUI
implementation, not a request to add Electron, Qt, WPF, Avalonia, or WebView to
the product.

## Preview Artifact

Interactive browser prototype:

```powershell
cd prototypes\dock-motion-baseline
python -m http.server 8765 --bind 127.0.0.1
```

Open:

```text
http://127.0.0.1:8765/index.html
```

The prototype is a temporary visual communication artifact. The production app
remains C++20, C++/WinRT, WinUI 3, and Windows App SDK.

## Chosen Direction

Use the Fluent / Acrylic direction as the v1.0.0 baseline.

- The Dock defaults to the bottom edge.
- Left and right placement follow the system taskbar position. Do not expose a
  separate arbitrary side-position model unless a future UX decision requires it.
- The Dock should feel native to Windows while borrowing Dock-style compactness,
  magnification, and shelf-like visual depth.
- Do not mimic macOS chrome exactly.

## Dock Surface

- Use a Fluent-style translucent Acrylic shelf.
- The background rail is half-height relative to the icons, like a low shelf
  behind the icon row.
- Keep the shelf subtle. It should support the icons, not look like a full
  rounded toolbar.
- The icon backgrounds should be as transparent as possible. Avoid visible
  white/translucent rounded squares behind each icon.
- Use restrained shadow and blur for depth.
- Preserve a readable fallback when transparency, blur, high contrast, or
  reduced effects are disabled.

Reference prototype defaults:

- Icon size: `56px`.
- Icon gap: `10px`.
- Magnification: about `1.68x`.
- Background blur: `22px`.
- Dock radius: `18px`.
- Active item lift: about half of hover lift, currently `6px`.

These are design anchors, not hard-coded production constants.

## Icon States

Pinned and running apps share one row.

- No numeric badges for window counts.
- Single running window: small dot under the icon.
- Multiple windows: short pill/line under the icon.
- Focused app/window: icon rises slightly in its resting state.
- Hover magnification stacks on top of the focused lift.
- Do not use color alone to communicate active/running state.

The focused lift should feel similar to hover motion but use about half the
height, so focus is visible without looking like hover is stuck.

## Hover and Magnification

- Hover magnification is localized to nearby icons.
- The curve should be smooth and predictable; the current prototype uses a
  cosine-like falloff.
- Non-hovered distant icons should not shift unpredictably.
- Reduced-motion mode keeps the same states but removes most transition time.

## Drag Sorting

- Drag sorting should show a clear insertion marker between icons.
- Dragging must not cause unrelated Dock elements to resize or jump.
- Persisted order belongs to Dock state first. System taskbar sync remains a
  later capability.

## Auto-Hide

- Default Dock position remains bottom.
- Auto-hide reveal should use a forgiving edge trigger area.
- Auto-hide is separate from native taskbar hiding.
- Native taskbar hiding remains explicit opt-in and must be reversible.

## Multi-Window Preview

- Multiple windows open a thumbnail group/chooser equivalent to Windows taskbar
  behavior.
- Use DWM thumbnails in production through `shell/`.
- Do not show a custom "unavailable window" warning in the normal preview UI.
  Follow Windows default thumbnail behavior instead.
- If a thumbnail cannot be obtained, degrade in the least surprising
  Windows-like way without inventing a separate Dock-only state.

## Out of Scope

For this baseline, do not add:

- Media panel.
- Lyrics.
- Audio-reactive visuals.
- Native taskbar hide by default.
- Theme marketplace, plugin system, or cross-platform UI framework.

## Implementation Notes

- The WinUI implementation should recreate the behavior and visual result, not
  copy browser prototype code directly.
- UI code must not call Win32, COM, DWM, Shell APIs, or GSMTC directly. Use the
  documented v2 layer boundaries.
- Keep all motion constants centralized enough to support reduced motion and
  later tuning, but do not build a speculative animation framework.
