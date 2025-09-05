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

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_SKILL_ASSETS_H */
