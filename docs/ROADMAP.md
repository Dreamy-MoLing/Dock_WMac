# Roadmap

Dock_WMac v2 is Windows-only. Each phase must leave the project buildable before
the next phase starts.

## Phase 0 - Preparation

- Refresh v2 documents.
- Define architecture boundaries.
- Add Visual Studio 2022 WinUI 3 scaffold.
- Start only an empty semi-transparent `DockWindow`.
- Archive v1 as reference under `legacy/qt-v1/`.

## Phase 1 - App Foundation

- Single instance.
- Launch argument handling.
- Config load/save.
- Logging and paths.
- Basic error surface.
- Window positioning with DPI and multi-monitor awareness.

## Phase 2 - Shell Observations

- Window enumeration adapter.
- Process and executable identity.
- Taskbar pinned item reader.
- Icon extraction.
- Fake shell data for tests.

## Phase 3 - Dock Model

- Dock item model.
- Pinned/running merge.
- Click decision state machine.
- Ordering persistence.
- Multi-window grouping.

## Phase 4 - Dock UI

- WinUI item row.
- Hover magnification.
- Running indicators.
- Drag sorting.
- Auto-hide without native taskbar hiding.
- DWM preview panel.

## Phase 5 - Media Panel

- GSMTC session discovery.
- Current session selection.
- Playback snapshot.
- Transport commands.
- Artwork placeholder.
- Apple Music for Windows manual validation.

## Phase 6 - Lyrics

- LRC parser.
- Local txt fallback.
- Local lyric matching.
- Cache.
- Non-blocking no-lyrics fallback.
- Upward fade visual layer.

## Phase 7 - Audio Visuals

- Pseudo-response wave/energy flow.
- Artwork-derived colors.
- Reduced-motion fallback.
- Later: real audio analysis only after a reviewed design.

## Phase 8 - Native Taskbar Option

- Explicit opt-in setting.
- Hide/restore adapter.
- Crash recovery.
- Explorer restart recovery.
- Manual restore command.

## Phase 9 - Packaging and Release

- CI for v2.
- Release packaging.
- Clean Windows 10 and Windows 11 validation.
