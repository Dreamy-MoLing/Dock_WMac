# v1 Freeze Notice

The existing Qt6 Widgets + C++17 implementation is frozen as v1 historical
reference for the v2 rewrite.

This directory contains the archived v1 implementation. It was moved here so
editors and future v2 work do not treat the Qt code as active source.

Frozen reference areas include:

- `legacy/qt-v1/CMakeLists.txt`, `CMakePresets.json`, and `Taskfile.yml`
- `legacy/qt-v1/include/`
- `legacy/qt-v1/src/`
- `legacy/qt-v1/tests/`
- `legacy/qt-v1/resources/`
- `legacy/qt-v1/docs-archive/`

Rules:

- Do not add v2 product features to the Qt architecture.
- Do not change v1 behavior unless the user explicitly requests v1 maintenance.
- Use v1 only to understand product behavior, edge cases, and interaction goals.
- Keep v2 work in the WinUI 3 / C++/WinRT project line.
