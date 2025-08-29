# Roguelike Game Engine Documentation

Welcome to the developer documentation for the Roguelike Game Engine.

This site is generated with Doxygen and covers the engine's major subsystems, public APIs, and selected internal modules. Use the left sidebar to browse files, classes/structs, and namespaces. Module pages below offer curated overviews with cross-links into source.

- See the modules index for high-level entries:
  - \ref CombatSystem
  - \ref LootSystem
  - \ref EquipmentSystem
  - \ref WorldGenSystem
  - \ref UiInputSystem
  - \ref SkillsProgression

Getting started:
- Build docs locally via the CMake `docs` target (see README “Documentation (Doxygen)”).
- HTML is emitted to `build/docs/html` and PDF to `build/docs/latex/refman.pdf` (when LaTeX is installed).

Notes:
- Graphviz is enabled for include/caller/callee/directory graphs.
- Static functions are included on module pages for context.
