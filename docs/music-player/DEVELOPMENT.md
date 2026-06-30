# Dock_WMac Music Player Development Guide

> Status: planned new feature. This document is the source of truth for music
> player development until code lands and README is updated.

## 1. Product Scope

The music player is a **Windows-only Dock companion panel**, not a new
cross-platform player and not a macOS Music.app integration.

V1 should add a compact Now Playing surface that belongs to Dock_WMac but stays
separate from the existing Dock feature code. The panel should follow the
current Windows media session, with Windows Apple Music as the first target.

### Goals

- Show a right-side Dock companion panel for the active media session.
- Prefer Windows Apple Music when it is the active or selected session.
- Display title, artist, album, artwork, playback state, position, and duration.
- Provide previous, play/pause, and next controls when the session supports them.
- Keep the existing Dock launch/window-management behavior independent.
- Keep the user-facing README unchanged until the feature is implemented and
  accepted.

### Non-goals

- No macOS target, AppKit, AppleScript, Music.app automation, `NSPanel`, or
  menu-bar implementation.
- No full audio-player engine in V1.
- No promise of Apple Music native synchronized lyrics.
- No hidden dependency on private Apple, web, or reverse-engineered APIs.
- No media-session logic inside the existing Dock core classes.

## 2. Technical Direction

Use Windows GSMTC as the primary integration path:

- `Windows.Media.Control.GlobalSystemMediaTransportControlsSessionManager`
  discovers system media sessions and session changes.
- `GlobalSystemMediaTransportControlsSession` reads playback information,
  timeline properties, media properties, and attempts playback controls.
- Public media properties cover fields such as title, artist, album title, and
  thumbnail. They do not provide lyrics.

Official references:

- [GlobalSystemMediaTransportControlsSessionManager](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessionmanager?view=winrt-26100)
- [GlobalSystemMediaTransportControlsSession](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssession?view=winrt-26100)
- [Session media properties](https://learn.microsoft.com/en-us/uwp/api/windows.media.control.globalsystemmediatransportcontrolssessionmediaproperties?view=winrt-26100)
- [App capability declarations](https://learn.microsoft.com/en-us/windows/uwp/packaging/app-capability-declarations)

Implementation must verify the exact capability/packaging requirement for this
Win32 Qt application before merging runtime GSMTC code. If a packaged build
requires `globalMediaControl`, document it next to the packaging change.

## 3. Repository Layout

Music code should be stored separately from the existing Dock implementation:

```text
include/music/
  core/      # snapshots, state machine, settings structs
  system/    # GSMTC / WinRT bridge
  ui/        # NowPlayingPanel public UI types
  lyrics/    # LRC parser, matcher, lyrics service

src/music/
  core/
  system/
  ui/
  lyrics/

tests/music/
```

Keep the boundaries boring:

- `music/system` owns WinRT/GSMTC calls.
- `music/core` owns session selection, snapshots, panel state, and settings.
- `music/ui` renders the panel and emits user commands.
- `music/lyrics` parses, matches, caches, and resolves lyrics.
- Existing Dock classes may wire the panel in, but should not own music logic.

Do not put GSMTC into `SysHelper`; that class is already the Win32/DWM Dock
system adapter. Do not put Now Playing state into `DockManager`; that class
owns Dock visibility and taskbar-style behavior.

## 4. V1 User Experience

The panel is a right-side slide-out Now Playing surface attached to the Dock.

Default layout:

```text
screen right edge
│
│   Dock / Apple Music icon
│   ┌──────────────────────────────┐
└──▶│ [artwork]  title              │
    │            artist             │
    │            progress bar       │
    │            prev  play  next   │
    └──────────────────────────────┘
         slides from right to left
```

Required states:

- No session: show an empty state and do not repeatedly prompt or fail.
- Session active: show metadata and supported controls.
- Metadata incomplete: show placeholders and keep controls available when safe.
- Artwork unavailable: show the music placeholder icon.
- Lyrics unavailable: keep the panel usable; lyrics are not part of the main
  control path.
- High contrast or reduced motion: use solid colors, visible borders, keyboard
  focus, and reduced animation.

Default dimensions can be refined during UI implementation, but the first
version should stay compact: roughly 300 px wide, 110-130 px tall, artwork on
the left, text/progress/controls on the right.

## 5. Lyrics Strategy

Lyrics are a separate subsystem. They consume normalized track metadata and
must not depend on Apple Music private APIs.

V1 accepted sources:

- User-bound local `.lrc`.
- Local `.txt` lyrics.
- Optional external lyrics adapter, explicitly enabled by the user.
- Local cache of previous successful matches.

Resolution order:

1. User explicit mapping.
2. Local LRC index match.
3. Local lyrics cache.
4. Enabled external adapter.
5. No lyrics.

LRC parser requirements:

- Support `[ar:]`, `[ti:]`, `[al:]`, `[by:]`, and `[offset:]` tags.
- Support multiple timestamps on one text line.
- Support `mm:ss.xx` and `mm:ss.xxx`.
- Sort lines by start time and derive `endMs` from the next line.

No-lyrics behavior is an accepted V1 outcome. The player panel must continue to
show metadata, progress, and controls.

## 6. Branch Workflow

- `codex` is the experimental implementation, review, and acceptance branch.
- `master` is the release branch.
- New music-player work starts on `codex`.
- After implementation, review, tests, and user acceptance pass, merge the
  accepted result into `master` and publish a new release.
- Keep only `codex`, `master`, `origin/codex`, and `origin/master` as active
  branches unless a short-lived feature branch is explicitly needed.

Before music implementation starts, ensure `codex` contains the latest
`master` release fixes.

## 7. Acceptance Criteria

The feature is not accepted until these scenarios pass:

- Media session discovery finds the active Windows media session.
- Windows Apple Music metadata displays when Apple Music exposes it through
  GSMTC.
- Play/pause, previous, and next buttons call session controls only when the
  session reports support.
- Progress and duration update without UI stalls.
- Missing artwork falls back to a placeholder.
- Missing lyrics never blocks the panel.
- Multiple media sessions choose a deterministic active session.
- Keyboard navigation works for all controls.
- High contrast and reduced motion settings remain usable.
- Long-running use does not show obvious memory growth or UI freezes.

## 8. Test Plan

Documentation-only changes do not require a build. Runtime implementation must
add the smallest useful tests:

- `tests/music/test_lrc_parser.cpp` for LRC parsing edge cases.
- `tests/music/test_lyrics_matcher.cpp` for title/artist/duration matching.
- A core state test for no-session, active-session, incomplete-metadata, and
  unsupported-control snapshots.

Manual validation is still required for GSMTC because it depends on real Windows
media sessions and installed players.

## 9. Archived Research

The original ChatGPT research files are archived under:

- `docs/archive/music-player-research/macos-apple-music-companion-research.md`
- `docs/archive/music-player-research/windows-gsmtc-music-player-research.md`

They are background material only. This development guide is the current
engineering source of truth.
