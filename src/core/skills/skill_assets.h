#ifndef ROGUE_CORE_SKILL_ASSETS_H
#define ROGUE_CORE_SKILL_ASSETS_H

#include "skill_debug.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Directory convention (relative to assets/):
       skills/<skill_name>/{cast,projectile,impact,aoe,icon}/ ... files
       This module provides helpers to resolve and validate those assets.
    */

    typedef struct RogueSkillAssetReport
    {
        /* Resolved full paths (with assets/ prefix) when found; empty string when missing */
        char cast_path[256];
        char projectile_path[256];
        char impact_path[256];
        char aoe_path[256];
        char icon_path[256];
        int cast_exists;
        int projectile_exists;
        int impact_exists;
        int aoe_exists;
        int icon_exists;
    } RogueSkillAssetReport;

    /* Build a canonical relative directory path like
       "skills/<skill_name>/<slot>/" into out (no assets/ prefix). Returns 0 on error. */
    int rogue_skill_assets_dir_for(const char* skill_name, const char* slot, char* out, int out_sz);

    /* Validate and resolve asset paths in `vis` using the directory convention when the provided
       paths are relative. Fills `rep` with resolved path strings and existence flags. Returns 0
       on success (validation performed). */
    int rogue_skill_assets_validate(const char* skill_name, const RogueSkillVisualParams* vis,
                                    RogueSkillAssetReport* rep);

    /* Count files in a PNG sequence inside a conventional subdir, e.g.,
       skills/<skill_name>/cast/<stem>_001.png, <stem>_002.png, ...
       Returns number of frames detected (>=0). When stem is NULL or empty, defaults to "cast". */
    int rogue_skill_assets_count_png_sequence(const char* skill_name, const char* slot,
                                              const char* stem);

/* --------------------------------------------------------------------- */
/* Phase 1.4: Asset Dependency Tracking & Hot-Reload (foundation)
   Lightweight in-memory registry that tracks skill visual asset file paths, their
   reference counts, and last modification timestamps. This enables editor tooling and
   runtime systems to poll for changed source assets (e.g., updated PNGs) and trigger
   texture reloads or preview refreshes without a full restart.

   Design goals (initial slice):
     - Zero dynamic allocations (fixed-cap array) for deterministic behavior in tests.
     - Cheap polling (stat only registered unique paths).
     - Cross-platform (Windows/MSVC + POSIX) using _stat/stat for mtime.
     - Safe no-op when a path is missing or becomes unavailable.

   Limitations (future slices can extend):
     - No automatic texture reload; higher-level code must react via callback.
     - No directory watching (OS file events); relies on explicit polling cadence.
     - Capacity is modest (ROGUE_SKILL_ASSET_DEP_MAX). Exceeding silently ignores new paths.
*/

/* Maximum unique tracked dependencies (tweakable). */
#define ROGUE_SKILL_ASSET_DEP_MAX 256

    typedef struct RogueSkillAssetDependency
    {
        char path[256];           /* Normalized path as tracked (as provided) */
        int ref_count;            /* Number of skills referencing the asset */
        unsigned long long mtime; /* Last observed modification timestamp (seconds resolution) */
    } RogueSkillAssetDependency;

    /* Reset internal dependency registry (drops all tracking). */
    void rogue_skill_asset_dep_reset(void);

    /* Track (or increment) a dependency on path. Returns index (>=0) or -1 on failure. */
    int rogue_skill_asset_dep_track(const char* path);

    /* Decrement dependency ref count; when it reaches 0 the entry is removed. Returns new ref
       count or -1 if path not found. */
    int rogue_skill_asset_dep_untrack(const char* path);

    /* Return current number of unique tracked dependencies. */
    int rogue_skill_asset_dep_count(void);

    /* Accessor for read-only snapshot; returns pointer to internal array (count given by above). */
    const RogueSkillAssetDependency* rogue_skill_asset_dep_data(void);

    /* Poll all tracked dependencies; for each whose file modification timestamp changes since
       last poll, invoke on_change(path,user). Updates stored mtime. Returns number of changed
       paths reported. Safe to call with NULL callback (counts only). */
    int rogue_skill_asset_dep_poll_changes(int (*on_change)(const char* path, void* user),
                                           void* user);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_SKILL_ASSETS_H */
