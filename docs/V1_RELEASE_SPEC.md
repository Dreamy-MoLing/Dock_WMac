# v1.0.0 Release Spec

Dock_WMac v1.0.0 is a Windows-only Dock-style taskbar extension. It must
reproduce the default Windows taskbar's core app and window behavior while
presenting a Dock-like visual and motion experience.

## Product Definition

- The app is a desktop multi-function taskbar, not a Shell replacement.
- Windows remains the source of truth for normal app/window behavior.
- The Dock adds visual style, hover magnification, drag ordering, auto-hide,
  DWM previews, and compact taskbar-like workflows.
- Media panel, lyrics, and audio-reactive visuals are post-v1.0.0 extensions.

## v1.0.0 Acceptance

- Launch apps from pinned Dock items.
- Read system taskbar fixed items as the initial Dock pinned set.
- Preserve Dock pin/order state locally without corrupting it if system sync is
  unavailable.
- Identify running apps and merge them with pinned items.
- Follow Windows taskbar default behavior for foreground, restore, minimize,
  and multi-window selection.
- Show DWM previews for supported windows.
- Provide fallback states for minimized, cloaked, unavailable, elevated, or
  otherwise inaccessible windows.
- Support bottom, left, and right placement models. Per-monitor placement is not
  part of v1.0.0.
- Persist Dock placement, ordering, local pins, auto-hide, animation preference,
  and UI state.
- Respect DPI, theme, high contrast, and reduced motion.
- Recover cleanly after Explorer restart and app restart.
- Package as unpackaged Win32 with Windows App SDK bootstrapper.

## Windows Behavior Matrix

| Scenario | Expected Dock behavior |
|---|---|
| App is not running | Launch the pinned target. |
| App has one normal window | Activate or restore it using Windows-compatible foreground behavior. |
| App window is already foreground | Match Windows taskbar behavior for minimize/restore according to configured taskbar-like behavior. |
| App has multiple windows | Present a thumbnail group or chooser equivalent to Windows taskbar behavior. |
| Window is minimized | Restore, then request foreground if Windows allows it. |
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
- Reduced-motion mode must have a simpler but still usable transition.

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
