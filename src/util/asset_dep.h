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

    /* Enumeration API for visualization/debugging */
    int rogue_asset_dep_count(void);
    /* Returns 0 on success; out_id/out_path are pointers to internal storage (valid until reset).
     */
    int rogue_asset_dep_get(int index, const char** out_id, const char** out_path);
    /* Fills out_dep_ids with pointers to dependency ids (direct edges) and returns count (>=0),
        or <0 on error. out_dep_ids entries are valid until reset. */
    int rogue_asset_dep_get_deps(const char* id, const char** out_dep_ids, int max_out);

    /* Retrieve details of the last failed registration (cycle or path_conflict).
        Returns 1 when available, 0 when none recorded. kind is a short string:
        "cycle" or "path_conflict". nid is the id being registered; dep is the
        related node id involved in the rejection when known. */
    int rogue_asset_dep_get_last_reject(char* kind, int kind_cap, char* nid, int nid_cap, char* dep,
                                        int dep_cap);

    /* Clear the last recorded rejection info. */
    void rogue_asset_dep_clear_last_reject(void);

#ifdef __cplusplus
}
#endif
#endif
