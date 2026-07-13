# UX Spec

Dock_WMac v2 is a Windows-only desktop Dock. The experience should feel native
to Windows while using a compact, polished floating Dock surface.

v1.0.0 must feel like a complete taskbar Dock, not a prototype: launch,
switching, multi-window handling, DWM previews, pin/order persistence, hover
magnification, drag sorting, and auto-hide need to work together without usage
conflicts.

The product goal is a Windows-facing Mac-style taskbar. Visual treatment may
borrow Dock-style magnification and compactness, but behavior follows Windows
taskbar defaults wherever Windows defines the workflow.

## Dock Window

- Default position: horizontally centered near the primary display bottom edge.
- Position model supports bottom, left, and right Dock placements. The user can
  change placement from the native Dock context menu.
- The Dock is shown only on the Windows primary display. It follows a primary
  display change and does not expose per-monitor or secondary-display placement.
- Shape: translucent rounded surface with depth, but no heavy decorative chrome.
- Visual layering: the translucent shelf and animated icon row behave as one
  Dock and are rendered as compositor layers inside one native Dock host. They
  must not appear as separate pages, separate panels, or rectangular windows.
- Size: compact enough to keep desktop content visible.
- Visibility: native taskbar remains visible by default.
- Naming: the production Dock host is a native non-activating Dock surface, not
  a XAML `DockWindow` page.
- Resource feel: the Dock should feel like system chrome. It should not create
  visible lag, sustained CPU load, excessive memory use, or extra background
  processes during idle use.

## Dock Items

- Pinned and running apps share one visual row.
- Pinned items default to the system taskbar pins.
- Dock pin edits must persist safely in Dock first. System taskbar sync is a
  later capability and must not break Dock state when unavailable.
- Running state is visible but subtle.
- Hover magnification is smooth and localized. Icons move along their
  centerline, using the perpendicular foot on the Dock shelf edge as the fixed
  reference. At the maximum hover pose, the icon outer circle is tangent to the
  shelf edge and is not clipped.
- Hover enter/exit uses a short monotonic ease. Every intermediate pose remains
  on the same centerline and tangent reference; motion never overshoots or
  shifts unrelated slots.
- Drag sorting must not resize or shift unrelated UI unpredictably.
- Multi-window apps open a preview or chooser instead of guessing silently.

## Click Behavior

Click decisions must remain predictable:

- Not running: launch.
- Running with one visible window: foreground or restore.
- Foreground window clicked again: minimize only when that matches the configured
  taskbar-like behavior.
- Multiple windows: follow Windows taskbar default behavior. If Windows would
  present a thumbnail group or chooser, Dock should present the equivalent
  preview/chooser instead of inventing a separate cycling policy.
- Middle-click or Shift+left-click: request a new instance of the selected app.
- A launched window attaches its running indicator and previews to the original
  pinned icon. A second identical icon is a grouping failure, not a valid Dock
  presentation.
- Grouping precedence is explicit window/shortcut AUMID, then a unique
  executable alias when either side lacks that stronger identity. Window title,
  icon appearance, and arbitrary process proximity never define a group.
- Launch failures: show a recoverable error and keep Dock responsive.

When a documented Dock-specific visual idea conflicts with Windows taskbar
behavior, Windows behavior wins unless the product spec is explicitly changed.

## Windows Taskbar Compatibility Contract

- Preserve Windows launch, activate, restore, minimize, multi-window chooser,
  middle-click new-instance, close-request, and DWM preview semantics.
- Preserve separate groups for applications that intentionally publish
  different explicit AUMIDs, even when they share one executable host.
- Keep the native taskbar enabled by default because Start, notification area,
  clock, quick settings, Jump Lists, progress overlays, badges, and shell-owned
  keyboard shortcuts are not duplicated by the Dock.
- Use Dock-owned pin ordering and persistence. Reading the user's taskbar pins
  is supported; mutating Explorer's undocumented taskband state is prohibited.
- Primary-display topology recovery, keyboard access, Explorer restart
  recovery, DPI, theme, high contrast, and reduced motion are compatibility
  requirements, not visual options.

## Auto-Hide

- Disabled by default and available from the native Dock context menu.
- Reveal trigger must remain forgiving at every supported primary-display DPI.
- Native taskbar hiding is a separate opt-in setting and must be reversible.

## Dock Context Menu

- Right-clicking an icon presents its windows and pin/unpin command.
- Right-clicking the shelf or empty Dock area presents bottom/left/right
  placement, automatic hiding, and a normal Exit command.
- Placement and automatic-hiding changes apply immediately and persist across
  restart without modifying the Windows taskbar.
- Native popup menus retain Windows keyboard navigation once opened.
- An app item menu lists its windows, provides pin or unpin, and provides
  `Close window` for a single-window app or `Close all windows` for a group.
  Closing is a normal Windows close request, not forced process termination.

## Window Previews

- Use DWM thumbnails through `shell/`.
- Preview rendering must not block Dock hover or click handling.
- Minimized windows follow Windows taskbar behavior: hover preview may show a
  thumbnail without restoring the window, and click activation restores only as
  part of the Windows-compatible activation path.
- Cloaked and unavailable windows need clear fallback states.

## Media Panel

- Uses GSMTC snapshots from `media/`.
- Apple Music for Windows is a priority validation target only.
- Layout shows artwork, title, artist, progress, play state, and transport
  controls.
- The panel expands sideways from the Dock and remains visually connected to
  the Dock surface.
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
- Reduced-motion enters and leaves the same geometric states immediately rather
  than removing magnification or changing hit targets.
- Follow the Windows app light/dark theme for Dock chrome while preserving icon
  identity and sufficient text/border contrast.
- Apply theme, high-contrast, and reduced-motion changes while the Dock is
  running; restarting the Dock is not required.
- Recalculate placement, hit regions, and rendering when DPI or display work
  area changes. Logical geometry must remain consistent at 100%, 125%, 150%,
  and 200% scaling.
- Support keyboard access for menus and player controls.
- Do not rely on color alone for running, active, or error states.
