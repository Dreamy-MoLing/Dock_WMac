# Roadmap

Dock_WMac v2 is Windows-only. Each phase must leave the project buildable before
the next phase starts.

## Phase 0 - Preparation

- Refresh v2 documents.
- Define architecture boundaries.
- Add Visual Studio 2022 WinUI 3 scaffold.
- Start only an empty semi-transparent `DockWindow`.
- Remove the old Qt implementation from active repository context.

## Phase 1 - v1.0.0 Taskbar Dock Release

- App lifecycle: single instance, launch arguments, config load/save, logging,
  paths, and error surface.
- Platform: DPI, theme, reduced motion, primary display work area, and
  bottom/left/right placement model without per-monitor placement UI.
- Shell/taskbar: window enumeration, process/app identity, taskbar pinned item
  reader, Shell icon extraction, DWM thumbnail previews, and Explorer restart
  recovery.
- Dock model: pinned/running merge, durable Dock pin state, ordering
  persistence, running indicators, and Windows-default click decisions.
- Dock UI: polished Dock-style surface, hover magnification, drag sorting,
  predictable auto-hide, multi-window preview/chooser, and graceful fallback
  states.
- Release: unpackaged Win32 + Windows App SDK bootstrapper packaging and clean
  Windows 10/11 validation.

## Phase 2 - Post-v1 Media Panel

- GSMTC session discovery.
- Current session selection.
- Playback snapshot.
- Transport commands.
- Artwork placeholder.
- Side-attached panel connected to the Dock surface.
- Apple Music for Windows manual validation.

## Phase 3 - Post-v1 Lyrics

- LRC parser.
- Local txt fallback.
- Local lyric matching.
- Cache.
- Non-blocking no-lyrics fallback.
- Upward fade visual layer.

## Phase 4 - Post-v1 Audio Visuals

- Pseudo-response wave/energy flow.
- Artwork-derived colors.
- Reduced-motion fallback.
- Later: real audio analysis only after a reviewed design.

## Phase 5 - Native Taskbar Option

- Explicit opt-in setting.
- Hide/restore adapter.
- Crash recovery.
- Explorer restart recovery.
- Manual restore command.

## Later Capability Backlog

- Sync Dock pin changes back to the system taskbar after Dock pin state is
  durable and recoverable.
- Revisit MSIX or self-contained packaging only if release validation requires
  it.
