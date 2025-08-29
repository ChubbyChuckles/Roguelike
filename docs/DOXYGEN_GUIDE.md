# Doxygen Commenting Guide

This project uses Doxygen to generate API documentation. Keep comments concise and consistent; document public headers and any internal modules that have non-trivial behavior.

Golden rules:
- Prefer documenting headers over C files; add function-level docs where behavior is subtle.
- Don’t restate names; write value-add summaries and edge cases.
- Link related symbols with @see and @ref. Group modules with @defgroup/@addtogroup.
- Keep examples minimal; prefer @code blocks or @snippet pointing to small example files.

Minimal templates:

File header:
/**
 * @file foo.h
 * @brief What this component provides and when to use it.
 * @defgroup Foo Foo
 * @{ */

// ... declarations ...

/** @} */

Function:
/**
 * @brief One-line value-add summary.
 * @param[in]  a  Units and valid range.
 * @param[out] b  Ownership and lifetime.
 * @return 0 on success; negative error on failure.
 * @see foo_init, Foo
 */

Struct:
/** @brief Purpose and usage patterns. */
typedef struct Foo { int bar; } Foo;

Diagrams:
- Keep dot graphs on by default; large graphs: prefer @dot digraph snippets.
- For big states, add PlantUML in docs/arch and link from module overviews.

Style:
- Third-person, present tense; avoid “we/I”.
- Units, ranges, pre/post-conditions, error modes.
- Thread-safety note when applicable.

Build:
- Run the CMake target “docs” or use CI artifacts (docs-html / Pages).
