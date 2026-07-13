# Architecture

Dock_WMac v2 is Windows-only and uses C++20, C++/WinRT, Windows App SDK Stable,
a native Win32 Dock host, DirectComposition, and Direct2D/DirectWrite. WinUI 3
is available for secondary app surfaces, but not for the production Dock
chrome. Win32, COM, DirectComposition, Direct2D, DWM, Shell APIs, and GSMTC are
system capability layers.

The architecture optimizes for a Windows-facing Mac-style taskbar that behaves
like the Windows taskbar and remains light enough to feel like desktop chrome.
Low idle memory and low background CPU are design constraints, not late
optimizations.

## Layers

| Layer | Owns | Must Not Own |
|---|---|---|
| `app/` | app lifecycle, single instance, launch arguments, config loading, Dock host creation, secondary window creation | Dock decisions, Shell calls hidden inside UI |
| `ui/` | Dock view models, render descriptions, animation states, secondary WinUI 3 pages, player panel, lyrics visual layer | direct Win32, COM, DirectComposition, Direct2D, DWM, Shell, GSMTC calls |
| `render/` or `platform/` | native Dock host window, DirectComposition visual tree, Direct2D/DirectWrite drawing, transparent chrome, compositor timing, hit-test region wiring | product state, Shell enumeration, media/session ownership |
| `dock/` | Dock state machine, icon model, durable pinned apps, running app mapping, Windows-default click decisions, ordering, persistence | WinRT/COM calls |
| `shell/` | window enumeration, DWM thumbnails, Shell Link parsing, icon extraction, taskbar pinned item reading and later sync, taskbar hide/restore | product state |
| `media/` | GSMTC discovery, active session selection, playback snapshot, media commands | Dock state |
| `lyrics/` | LRC parsing, txt loading, matching, local index, cache, no-lyrics fallback | blocking media/UI work |
| `platform/` | DPI, displays, theme, high contrast, reduced motion, window z-order | product decisions |
| `infra/` | logging, paths, JSON config, error handling, performance and resource sampling | UI behavior |

## v1.0.0 Focus

v1.0.0 must fully exercise `app/`, `ui/`, `dock/`, `shell/`, `platform/`, and
`infra/` for taskbar-Dock behavior. `media/` and `lyrics/` remain architectural
extension points until post-v1 feature work starts.

## Dependency Direction

```text
app -> ui
app -> platform/render
app -> dock
app -> infra
ui -> dock
ui -> media
ui -> lyrics
ui -> platform
platform/render -> ui render descriptions
dock -> shell interfaces
dock -> infra
media -> infra
lyrics -> infra
shell -> platform
```

Concrete system adapters live at the edge. Core logic depends on interfaces
owned by the layer that needs the behavior, not on raw system APIs.

## Hard Rules

- UI view-model code never calls Win32, COM, DWM, DirectComposition, Direct2D,
  DirectWrite, Shell APIs, or GSMTC directly.
- Native Dock rendering code owns system rendering APIs behind platform/render
  adapters and must not own Dock product state.
- Long-lived caches, timers, windows, graphics resources, and background work
  require explicit ownership and teardown. Avoid duplicate UI hosts and
  unbounded retained resources.
- `dock/` never calls WinRT or COM directly.
- `media/` never depends on Dock state.
- `lyrics/` never blocks media snapshots or UI rendering.
- `shell/` returns observations and performs commands; it does not persist
  product state.
- System API usage must be mockable or isolated in tests.

## Initial Runtime Flow

1. `app/` starts, enforces single instance, loads config, and creates the
   native Dock host plus any secondary WinUI windows.
2. `ui/` prepares Dock view models, render descriptions, and animation state.
   The native Dock host renders those descriptions through DirectComposition
   and Direct2D/DirectWrite.
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
