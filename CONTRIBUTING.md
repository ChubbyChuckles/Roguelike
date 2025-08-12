# Contributing Guidelines

## Workflow Principles
1. Small, atomic changes (one concern per commit / PR)
2. Add / update tests with each behavioral change
3. Maintain 100% compilation without warnings (treat warnings as errors)
4. Refactor proactively to keep functions cohesive & readable

## Style
* Run `cmake --build build --target format` before committing
* Keep headers self-contained (include what you use)
* Avoid implicit narrowing conversions (keep `-Wconversion` clean)

## Tests
* Pure logic unit tests → `tests/unit/`
* Integration tests (platform / SDL) → `tests/integration/`
* Every new public function: at least one unit test unless trivial inline

## Adding Dependencies
* Prefer small, single-header libs; wrap them in `third_party/` with thin adapter
* Avoid vendor lock-in; keep interfaces narrow

## Review Checklist (LLM / Human)
* Separation of concerns clear
* No duplicated logic (extract helpers)
* Descriptive names (no cryptic abbreviations)
* Error paths checked & logged
* Tests cover success + at least one failure / edge case
