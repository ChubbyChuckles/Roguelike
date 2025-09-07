/* Placeholder asset enforcement helper (Phase 2)
   Ensures a generic placeholder sprite exists for missing visual assets.
   Pure filesystem existence probe; no SDL dependency. */
#ifndef ROGUE_ASSET_PLACEHOLDER_H
#define ROGUE_ASSET_PLACEHOLDER_H

#ifdef __cplusplus
extern "C"
{
#endif

/* Canonical relative path (from repository root) to the placeholder sprite. */
#define ROGUE_ASSET_PLACEHOLDER_PATH "assets/placeholder.png"

    /* Returns 1 if the placeholder file exists (searching common relative prefixes
       for test/build working directories: "", "../", "../../", "../../../"). */
    int rogue_asset_placeholder_exists(void);

    /* Generic filesystem existence probe exposed for the unit test (returns 1 on existence). */
    int rogue_asset_path_exists(const char* path);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_ASSET_PLACEHOLDER_H */
