/* asset_dep.h - Phase M3.4 asset dependency graph & hashing */
#ifndef ROGUE_UTIL_ASSET_DEP_H
#define ROGUE_UTIL_ASSET_DEP_H
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueAssetDepNode RogueAssetDepNode; /* opaque */
    int rogue_asset_dep_register(const char* id, const char* path, const char** deps,
                                 int dep_count);
    int rogue_asset_dep_hash(const char* id, unsigned long long* out_hash);
    int rogue_asset_dep_invalidate(const char* id);
    void rogue_asset_dep_reset(void);

#ifdef __cplusplus
}
#endif
#endif
