# Migration

Dock_WMac v2 is a Windows-only rewrite. The current Qt6 Widgets + C++17 code is
v1 historical reference and must not drive new architecture decisions.

## Current Decision

The old source tree has been moved to `legacy/qt-v1/` so editors and future v2
work do not keep treating Qt files as active code. The archive is reference-only
and documented by `legacy/qt-v1/FREEZE.md`.

Current layout:

```text
legacy/qt-v1/   frozen Qt v1 implementation and old build files
legacy/qt-v1/docs-archive/
                stale plans and old research
src/v2/         active v2 scaffold during preparation
```

## What v1 Can Provide

- Behavior examples for launch/switch/minimize decisions.
- Edge cases for pinned items and running apps.
- Test scenarios for DWM previews, hidden/cloaked windows, DPI, and taskbar
  recovery.
- Media and lyrics exploration notes from `legacy/qt-v1/docs-archive/`, only
  after checking current v2 docs.

## What v1 Must Not Provide

- Qt ownership boundaries.
- Widget-specific rendering patterns.
- Direct UI calls into Win32, COM, DWM, or GSMTC.
- Product state stored inside system adapters.
- New feature work on the old architecture.

## Migration Steps

1. Keep v1 archived as reference while v2 scaffold and docs stabilize.
2. Add v2 interfaces for `shell/`, `media/`, `lyrics/`, and `dock/` before
   porting behavior.
3. Port behavior with tests, not files wholesale.
4. Replace old build, CI, and packaging after v2 can start and pass basic
   validation.
5. Archive obsolete planning docs after source migration.

## Risk Controls

- No directory moves in the same change as feature code.
- No taskbar hiding until startup, shutdown, crash recovery, and manual restore
  validation exist.
- No external lyric source until opt-in settings and network disclosure exist.
- No real audio capture until pseudo-response visuals and reduced-motion
  behavior are stable.
