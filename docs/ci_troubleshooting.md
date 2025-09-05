# CI Troubleshooting Guide

This guide summarizes common CI issues for the Roguelike project and how to resolve them across platforms.

## SDL2 not found

- Windows: Ensure a classic vcpkg instance is bootstrapped. Verify `VCPKG_ROOT` and presence of `installed/x64-windows/bin/SDL2.dll`.
- Linux: Confirm `libsdl2-dev`, `libsdl2-image-dev`, `libsdl2-mixer-dev` installed. Check `pkg-config --modversion sdl2`.
- macOS: `brew install sdl2 sdl2_image sdl2_mixer`. Set `PKG_CONFIG_PATH` for keg-only libs.

## Tests fail due to display

- Use headless: `SDL_VIDEODRIVER=dummy` on all OSes. On Linux, run tests within `xvfb-run`.

## Clang-Tidy complaints

- `.clang-tidy` is intentionally minimal and advisory in CI. Locally, use `cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON` then run `clang-tidy -p build <file.c>`.

## Path portability issues

- Prefer forward slashes `/` in paths. Use `tools/normalize_paths.ps1 -CheckOnly` to detect occurrences and `-Fix` to convert.

## Long build times

- CI uses caching (`actions/cache`) and `ccache` on Linux/macOS. Ensure cache keys include `CMakeLists.txt`, `cmake/`, `src/`, and `tests/`.

## Doxygen/Graphviz failures

- Ensure doxygen and graphviz are installed (`choco`, `apt`, `brew`). Re-run docs target `cmake --build build --target docs` to check.

## GitHub Pages deployment

- Pages jobs require `pages: write` and `id-token: write` permissions at the workflow level. Artifacts uploaded via `actions/upload-pages-artifact`.

## Cancel stuck CI runs

- Use `scripts/ci/cancel-run.ps1 -RunId <id> -Token <PAT>` and `scripts/ci/poll-run-status.ps1` to monitor. Hosted runners may not support per-job cancel.

---

Updated: Keep this document in sync with `.github/workflows/ci.yml`.
