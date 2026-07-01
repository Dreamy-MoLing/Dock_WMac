# Music Panel Development

Dock_WMac v2 is Windows-only. The media panel is a post-v1.0.0 feature built on
GSMTC through the `media/` layer, not a standalone player and not a v1 Qt
feature.

## Scope

- Use Windows GSMTC to observe and control media sessions.
- Prioritize Apple Music for Windows during manual validation, without
  hard-coding that app.
- Show artwork, title, artist, progress, playback state, and transport controls.
- Degrade cleanly when there is no session, no artwork, or unsupported control.
- Keep Dock launch, switching, previews, and taskbar behavior independent.

## Non-Goals

- No full audio playback engine.
- No private Apple APIs.
- No assumption that Apple Music exposes a third-party lyrics API.
- No external lyrics request unless the user explicitly enables it.
- No media state inside `dock/`.
- No GSMTC calls from `ui/`.

## Layer Ownership

```text
media/   GSMTC discovery, session selection, playback snapshot, commands
lyrics/  LRC parsing, txt fallback, local matching, cache
ui/      WinUI media panel and lyric visual rendering
infra/   paths, logging, config, error handling
```

`media/` must not depend on Dock state. `lyrics/` must never block the media
panel. UI consumes snapshots and commands through view models only.

## Lyrics Strategy

Accepted first sources:

1. User-mapped local LRC.
2. Local LRC index match.
3. Local txt fallback.
4. Local cache.
5. No lyrics.

External providers are future opt-in work. No-lyrics is a valid state and must
not reduce player controls.

## Visual Direction

- Panel: compact, polished WinUI surface that expands sideways from the Dock
  and remains visually connected to the Dock bar.
- Lyrics: upward fading flame/fog-like layer.
- Audio visuals: smooth waves or energy flow, not a basic bar meter.
- Color may be seeded from artwork.
- Reduced motion must simplify or disable movement.

## Acceptance Before Implementation

- `media/` interfaces exist before real GSMTC code.
- Fake media snapshots can drive the UI.
- Missing session, missing artwork, unsupported controls, and no-lyrics states
  are represented.
- Apple Music for Windows is tested only through GSMTC.
- No taskbar hiding or Dock animation work is mixed into media-panel changes.
