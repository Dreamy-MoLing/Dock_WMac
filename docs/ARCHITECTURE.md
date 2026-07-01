# Architecture

Dock_WMac v2 is Windows-only and uses C++20, C++/WinRT, WinUI 3, and Windows App
SDK Stable. Win32, COM, DWM, Shell APIs, and GSMTC are system capability layers,
not UI architecture.

## Layers

| Layer | Owns | Must Not Own |
|---|---|---|
| `app/` | app lifecycle, single instance, launch arguments, config loading, main window creation | Dock decisions, Shell calls hidden inside UI |
| `ui/` | WinUI 3 XAML views, Dock visuals, player panel, lyrics visual layer, animation states | direct Win32, COM, DWM, GSMTC calls |
| `dock/` | Dock state machine, icon model, durable pinned apps, running app mapping, Windows-default click decisions, ordering, persistence | WinRT/COM calls |
| `shell/` | window enumeration, DWM thumbnails, Shell Link parsing, icon extraction, taskbar pinned item reading and later sync, taskbar hide/restore | product state |
| `media/` | GSMTC discovery, active session selection, playback snapshot, media commands | Dock state |
| `lyrics/` | LRC parsing, txt loading, matching, local index, cache, no-lyrics fallback | blocking media/UI work |
| `platform/` | DPI, displays, theme, high contrast, reduced motion, window z-order | product decisions |
| `infra/` | logging, paths, JSON config, error handling, performance sampling | UI behavior |

## v1.0.0 Focus

v1.0.0 must fully exercise `app/`, `ui/`, `dock/`, `shell/`, `platform/`, and
`infra/` for taskbar-Dock behavior. `media/` and `lyrics/` remain architectural
extension points until post-v1 feature work starts.

## Dependency Direction

```text
app -> ui
app -> dock
app -> infra
ui -> dock
ui -> media
ui -> lyrics
ui -> platform
dock -> shell interfaces
dock -> infra
media -> infra
lyrics -> infra
shell -> platform
```

Concrete system adapters live at the edge. Core logic depends on interfaces
owned by the layer that needs the behavior, not on raw system APIs.

## Hard Rules

- UI never calls Win32, COM, DWM, or GSMTC directly.
- `dock/` never calls WinRT or COM directly.
- `media/` never depends on Dock state.
- `lyrics/` never blocks media snapshots or UI rendering.
- `shell/` returns observations and performs commands; it does not persist
  product state.
- System API usage must be mockable or isolated in tests.

## Initial Runtime Flow

1. `app/` starts, enforces single instance, loads config, and creates
   `DockWindow`.
2. `ui/` renders the Dock surface and binds to view models.
3. `dock/` combines system taskbar pins and running app observations into Dock
   items.
4. `shell/` supplies window, process, icon, taskbar pin, and DWM preview data.
   Taskbar pin sync commands stay isolated there when implemented.
5. `media/` supplies the current GSMTC playback snapshot.
6. `lyrics/` supplies local timed or plain text lyrics when available.

## Test Strategy

- `dock/` state transitions use fake shell observations.
- `shell/` adapters get narrow integration tests on real Windows.
- `media/` uses fake GSMTC snapshots for unit tests and manual real-session
  validation.
- `lyrics/` parsers use file-based unit tests.
- UI tests stay shallow until behavior is stable.
