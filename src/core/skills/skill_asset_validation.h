/* Skill Asset Validation Utilities
   Provides reusable classification logic for sprite asset paths and grid inference heuristics
   used by the debug visuals validation panel and unit tests. */
#ifndef ROGUE_SKILL_ASSET_VALIDATION_H
#define ROGUE_SKILL_ASSET_VALIDATION_H

#ifdef __cplusplus
extern "C"
{
#endif

    int rogue_skill_asset_validate(const char* path, int* missing, int* load_failed, int* dim_err,
                                   int* w, int* h, int* ext_warn);
    int rogue_visuals_infer_grid(int tex_w, int tex_h, int* grid_w, int* grid_h, int* frame_count);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_SKILL_ASSET_VALIDATION_H */
