# Legacy Removal

Dock_WMac v2 is a Windows-only rewrite. The old Qt implementation is removed
from the active repository context and must not drive v2 architecture or product
decisions.

## Active Layout

```text
src/v2/         active v2 source
docs/           active v2 product, architecture, UX, validation, and workflow docs
legacy/         note for removed legacy archive context
```

## Recovery Policy

The former source remains recoverable from Git history. A local non-active copy
may exist outside the workspace for archival purposes, but it is not part of
normal code discovery or agent context.

Use recovered legacy material only when the user explicitly asks for legacy
recovery or comparison. Future v2 behavior decisions should come from active v2
docs, official Windows documentation, and runnable v2 validation.

## What Legacy Must Not Provide

- Ownership boundaries.
- Widget-specific rendering patterns.
- Product requirements.
- Test expectations unless they are promoted into active v2 validation docs.
- Build, CI, or packaging behavior.

## Risk Controls

- No taskbar hiding until startup, shutdown, crash recovery, and manual restore
  validation exist.
- No external lyric source until opt-in settings and network disclosure exist.
- No real audio capture until pseudo-response visuals and reduced-motion
  behavior are stable.
