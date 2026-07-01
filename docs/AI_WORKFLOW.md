# AI Workflow

Dock_WMac v2 is Windows-only. Future agents must treat the rewrite as a new
C++20, C++/WinRT, WinUI 3, Windows App SDK project.

## Working Order

1. Read `AGENTS.md`.
2. Read the relevant v2 docs under `docs/`.
3. Inspect the current branch and dirty files.
4. Use codebase-memory-mcp for code discovery when available.
5. Use official Microsoft documentation or local tool output for Windows App SDK
   and WinUI questions.
6. Make the smallest buildable change.
7. Run the narrowest validation that proves the change.

If the `superpowers` plugin is available in a future session, use its
brainstorming workflow before broad requirement changes or ambiguous product
decisions. It should clarify intent, not replace the source-of-truth docs.

For motion, layout, and visual-density decisions, prefer a Browser Plugin
preview that the user can inspect and annotate before committing larger UI
changes.

## Do Not Drift

- Do not extend the v1 Qt architecture for v2 product work.
- Do not add speculative layers, factories, or plugin systems.
- Do not add full player, lyrics, Dock animation, or taskbar hiding before their
  roadmap phase.
- Do not call Win32, COM, DWM, Shell APIs, or GSMTC from UI code.
- Do not use private Apple APIs.
- Do not add external lyric providers without explicit user opt-in.

## Documentation Rule

When behavior changes, update the smallest relevant doc:

- Product behavior: `docs/PRODUCT_SPEC.md`
- Architecture boundary: `docs/ARCHITECTURE.md`
- Visible interaction: `docs/UX_SPEC.md`
- Tooling or package version: `docs/TECH_STACK.md`
- Migration status: `docs/MIGRATION.md`
- Phase sequencing: `docs/ROADMAP.md`
- Acceptance checks: `docs/VALIDATION.md`

## Evidence Rule

Distinguish clearly between:

- Real build output.
- Manual runtime validation.
- Static file preparation.
- Temporary previews.

Do not present a mock, screenshot, or unchecked scaffold as a working feature.
