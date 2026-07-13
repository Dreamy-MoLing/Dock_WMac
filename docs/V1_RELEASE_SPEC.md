# v1.0.0 Release Spec

Dock_WMac v1.0.0 is a Windows-facing Mac-style taskbar. It must reproduce the
default Windows taskbar's core app and window behavior while presenting a
Dock-like visual and motion experience.

## Product Definition

- The app is a desktop multi-function taskbar, not a Shell replacement.
- Windows remains the source of truth for normal app/window behavior.
- The Dock adds visual style, hover magnification, drag ordering, auto-hide,
  DWM previews, and compact taskbar-like workflows.
- Low steady-state memory and CPU use are release requirements. The app should
  behave like lightweight desktop chrome, not a heavy foreground application.
- The Dock shelf and animated icon row are separate compositor layers inside a
  native Dock host. They visually behave as one Dock while avoiding default
  window background bleed and icon clipping during magnification.
- Media panel, lyrics, and audio-reactive visuals are post-v1.0.0 extensions.

## v1.0.0 Acceptance

- Launch apps from pinned Dock items.
- Read system taskbar fixed items as the initial Dock pinned set.
- Preserve Dock pin/order state locally without corrupting it if system sync is
  unavailable.
- Identify running apps and merge them with pinned items.
- Preserve one taskbar group for a pinned app and its running windows. Prefer
  matching explicit shortcut/window/process AUMIDs; use executable aliases only
  when no stronger running identity exists, and reject ambiguous aliases.
- Follow Windows taskbar default behavior for foreground, restore, minimize,
  and multi-window selection.
- Support Windows-style new-instance requests through middle-click and
  Shift+left-click.
- Avoid duplicate render hosts, hidden browser runtimes, persistent helper
  processes, high-frequency idle polling, and unbounded caches.
- Show DWM previews for supported windows.
- Show taskbar-style previews for minimized windows when Windows/DWM can
  provide them; preview hover does not restore the source window.
- Provide fallback states for minimized, cloaked, unavailable, elevated, or
  otherwise inaccessible windows.
- Support bottom, left, and right placement on the Windows primary display.
  The Dock follows a change of primary display, but never renders a second
  instance or offers secondary-display placement in v1.0.0.
- Persist Dock placement, ordering, local pins, auto-hide, animation preference,
  and UI state.
- Expose native Dock context commands for placement, auto-hide, and normal
  process exit; do not require manual JSON editing for these daily controls.
- Expose taskbar-style app context commands for pin/unpin, selecting a specific
  window, closing one window, and closing all windows in an app group. Close
  commands request normal window closure and never force-terminate the app.
- Respect DPI, theme, high contrast, and reduced motion.
- Measure idle process working set, private bytes, handle count, thread count,
  and CPU usage; unexplained growth blocks release.
- Recover cleanly after Explorer restart and app restart.
- Package as unpackaged Win32 with Windows App SDK bootstrapper.

## Windows Behavior Matrix

| Scenario | Expected Dock behavior |
|---|---|
| App is not running | Launch the pinned target. |
| Pinned app launches a window | Keep one pinned icon and attach running state, previews, and indicators to it. |
| App has one normal window | Activate or restore it using Windows-compatible foreground behavior. |
| App window is already foreground | Match Windows taskbar behavior for minimize/restore according to configured taskbar-like behavior. |
| App has multiple windows | Present a thumbnail group or chooser equivalent to Windows taskbar behavior. |
| App item is middle-clicked or Shift+left-clicked | Request a new instance and allow the application to apply its own single-instance policy. |
| Window is minimized and hovered for preview | Show the Windows/DWM thumbnail when available without restoring or activating it. |
| Window is minimized and clicked for activation | Restore, then request foreground if Windows allows it. |
| Window is cloaked or unavailable | Keep the item visible and show a clear unavailable preview state. |
| App is elevated and Dock is not | Do not fake success; use safe fallback and visible state. |
| App exits | Remove transient running state without losing local pin state. |
| Explorer restarts | Re-read taskbar pins and rebuild shell observations without losing Dock state. |

## UI and Motion Baseline

- Use existing high-quality Dock references for visual research, but do not copy
  code or assets without license review.
- Before implementing major visual or motion changes, create a browser-preview
  prototype or other inspectable animation preview for user annotation.
- The preview should cover icon size, magnification curve, spacing, Dock
  height, corner radius, translucency, shadow, auto-hide trigger area, and
  animation duration.
- Hover motion must keep each icon on its centerline and clamp the maximum pose
  so the icon outer circle is tangent to the Dock shelf edge.
- Hover enter/exit must be eased without overshoot, preserve the tangent rule on
  every intermediate frame, and stop frame scheduling after the transition.
- The v1.0.0 Dock chrome must be implemented with native Win32,
  DirectComposition, and Direct2D/DirectWrite, not as transparent XAML windows.
- Reduced-motion mode must preserve the same terminal geometry and apply it
  immediately without starting an animation timer.

## Config and State

Use existing Windows user locations when data belongs to the user. Keep
release-bundle files together when they belong to the app.

- Install/runtime files: one app folder containing the executable, DLLs,
  bootstrapper/runtime-adjacent files, assets, and manifests needed to run.
- User state: per-user app data path for settings, local pins, layout, logs, and
  caches.
- Project-specific portable fallback: an app-local `data/` folder only when
  explicit portable mode is selected.
- Do not scatter DLLs, configs, logs, or caches outside the app folder or user
  data folder.

State that must persist:

- Dock placement: bottom, left, or right.
- Dock ordering.
- Local pinned items.
- Imported taskbar pin identity snapshots.
- Auto-hide setting.
- Animation/reduced-motion preference.
- Window/display geometry cache that is safe to discard.

## Release Folder Layout

The v1.0.0 release artifact should keep runtime files under one folder:

```text
Dock_WMac/
  Dock_WMac_v2.exe
  *.dll
  assets/
  licenses/
  logs/              optional portable mode only
  data/              optional portable mode only
```

Normal installed mode stores user-writable data under the user's app data
location, not next to system folders or random working directories.
