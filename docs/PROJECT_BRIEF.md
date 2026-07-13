# Project Brief

Dock_WMac v2 is a Windows-only desktop Dock rewrite. The current product and
engineering source of truth has moved to:

- `docs/PRODUCT_SPEC.md`
- `docs/ARCHITECTURE.md`
- `docs/UX_SPEC.md`
- `docs/TECH_STACK.md`
- `docs/MIGRATION.md`
- `docs/ROADMAP.md`
- `docs/V1_RELEASE_SPEC.md`
- `docs/UI_MOTION_BASELINE.md`
- `docs/SYSTEM_API_RISKS.md`
- `docs/VALIDATION.md`
- `docs/AI_WORKFLOW.md`

The old Qt implementation has been removed from active repository context.
Build the C++20, C++/WinRT, Windows App SDK v2 line from the active docs and
`src/v2/`. The production Dock surface uses a native Win32 host with
DirectComposition and Direct2D/DirectWrite; WinUI 3 is reserved for secondary
app surfaces.
