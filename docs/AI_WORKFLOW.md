# AI Workflow

Dock_WMac v2 is Windows-only. Future agents must treat the rewrite as a new
C++20, C++/WinRT, Windows App SDK project with a native Win32,
DirectComposition, and Direct2D/DirectWrite Dock surface. WinUI 3 is reserved
for secondary app surfaces.

## Working Order

1. Read `AGENTS.md`.
2. Read the relevant v2 docs under `docs/`.
3. Inspect the current branch and dirty files.
4. Use codebase-memory-mcp for code discovery when available.
5. Use official Microsoft documentation or local tool output for Windows App
   SDK, Win32, DirectComposition, Direct2D/DirectWrite, DWM, and WinUI
   questions.
6. Make the smallest buildable change.
7. Run the narrowest validation that proves the change.

The user defines product needs. The agent chooses implementation details based
on the codebase, source-of-truth docs, local machine, official documentation,
Windows default behavior, resource measurements, and real validation. User
prompts may contain inaccurate implementation direction; treat them as input to
reconcile with the documented product goal, not as commands to follow
mechanically. Ask the user only when a decision changes product behavior,
privacy/security, release expectations, or visible UX.

For v1.0.0, optimize for a complete, polished taskbar-Dock release rather than
a narrow minimum feature demo. Use existing Windows behavior and mature
community approaches where they clearly reduce risk, but keep the documented
architecture boundaries intact.

If docs do not clearly express the product goal, fix the docs before coding.
If docs are clear, implement against them even when an ad-hoc prompt would lead
to churn, fake progress, heavy resource use, or worse UI.

If the `superpowers` plugin is available in a future session, use its
brainstorming workflow before broad requirement changes or ambiguous product
decisions. It should clarify intent, not replace the source-of-truth docs.

For motion, layout, and visual-density decisions, prefer a Browser Plugin
preview that the user can inspect and annotate before committing larger UI
changes.

The current provisional v1.0.0 Dock visual and motion baseline is captured in
`docs/UI_MOTION_BASELINE.md`, with the runnable reference prototype under
`prototypes/dock-motion-baseline/`.

## Do Not Drift

- Do not use the removed Qt implementation for v2 product work.
- Do not add speculative layers, factories, or plugin systems.
- Do not follow prompt wording into implementation churn when the docs already
  define the better path.
- Do not add full player, lyrics, Dock animation, or taskbar hiding before their
  roadmap phase.
- Do not build the production Dock chrome as transparent XAML windows.
- Do not call Win32, COM, DirectComposition, Direct2D, DWM, Shell APIs, or GSMTC
  from UI view-model code; use render/platform/shell adapters.
- Do not use private Apple APIs.
- Do not add external lyric providers without explicit user opt-in.
- Do not use removed legacy archive material as active requirements.
- Do not add idle memory/CPU cost without a visible v1.0.0 taskbar benefit.

## Documentation Rule

When behavior changes, update the smallest relevant doc:

- Product behavior: `docs/PRODUCT_SPEC.md`
- Architecture boundary: `docs/ARCHITECTURE.md`
- Visible interaction: `docs/UX_SPEC.md`
- UI/motion baseline: `docs/UI_MOTION_BASELINE.md`
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
