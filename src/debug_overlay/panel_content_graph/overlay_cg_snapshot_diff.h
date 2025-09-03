/* overlay_cg_snapshot_diff.h: Snapshot and diff API for Content Graph overlay */
#ifndef OVERLAY_CG_SNAPSHOT_DIFF_H
#define OVERLAY_CG_SNAPSHOT_DIFF_H

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef OVERLAY_CG_MAX_NODES
#define OVERLAY_CG_MAX_NODES 48
#endif
#ifndef OVERLAY_CG_MAX_EDGES
#define OVERLAY_CG_MAX_EDGES 96
#endif

    typedef struct OverlayCGSnapshot
    {
        int valid;
        int depth;
        char root[64];
        int ncount;
        int ecount;
        char nodes[OVERLAY_CG_MAX_NODES][64];
        char edge_from[OVERLAY_CG_MAX_EDGES][64];
        char edge_to[OVERLAY_CG_MAX_EDGES][64];
    } OverlayCGSnapshot;

    void overlay_cg_capture_snapshot(OverlayCGSnapshot* snap, const char* root_id, int depth,
                                     const char** nids, int ncount, int (*edges)[2], int ecount);

    /* Manage global A/B snapshots hidden in the module */
    void overlay_cg_set_snapshot_a(const OverlayCGSnapshot* s);
    void overlay_cg_set_snapshot_b(const OverlayCGSnapshot* s);
    const OverlayCGSnapshot* overlay_cg_get_snapshot_a(void);
    const OverlayCGSnapshot* overlay_cg_get_snapshot_b(void);

    /* Diff compute and query */
    void overlay_cg_reset_diff(void);
    void overlay_cg_compute_diff(void);
    int overlay_cg_diff_ready(void);

    int overlay_cg_get_added_count(void);
    const char* overlay_cg_get_added_from(int idx);
    const char* overlay_cg_get_added_to(int idx);

    int overlay_cg_get_removed_count(void);
    const char* overlay_cg_get_removed_from(int idx);
    const char* overlay_cg_get_removed_to(int idx);

    /* UI flag for showing diff overlay */
    void overlay_cg_set_show_diff_overlay(int show);
    int overlay_cg_get_show_diff_overlay(void);

    /* Export diff details as JSON into build/ */
    void overlay_cg_export_diff_json(void);

#ifdef __cplusplus
}
#endif

#endif /* OVERLAY_CG_SNAPSHOT_DIFF_H */
#ifndef OVERLAY_CG_SNAPSHOT_DIFF_H
#define OVERLAY_CG_SNAPSHOT_DIFF_H

#ifdef __cplusplus
extern "C"
{
#endif

/* MSVC C89 compatibility: use compile-time macros for fixed array sizes (no VLAs) */
#ifndef OVERLAY_CG_MAX_NODES
#define OVERLAY_CG_MAX_NODES 48
#endif

#ifndef OVERLAY_CG_MAX_EDGES
#define OVERLAY_CG_MAX_EDGES 96
#endif

    typedef struct OverlayCGSnapshot
    {
        int valid;
        int depth;
        char root[64];
        int ncount;
        int ecount;
        char nodes[OVERLAY_CG_MAX_NODES][64];
        char edge_from[OVERLAY_CG_MAX_EDGES][64];
        char edge_to[OVERLAY_CG_MAX_EDGES][64];
    } OverlayCGSnapshot;

    /* Snapshot management */
    void overlay_cg_capture_snapshot(OverlayCGSnapshot* snap, const char* root_id, int depth,
                                     const char** nids, int ncount, int (*edges)[2], int ecount);

    void overlay_cg_set_snapshot_a(const OverlayCGSnapshot* snap);
    void overlay_cg_set_snapshot_b(const OverlayCGSnapshot* snap);
    const OverlayCGSnapshot* overlay_cg_get_snapshot_a(void);
    const OverlayCGSnapshot* overlay_cg_get_snapshot_b(void);

    /* Diff (A -> B) */
    void overlay_cg_reset_diff(void);
    void overlay_cg_compute_diff(void);
    int overlay_cg_diff_ready(void);

    int overlay_cg_get_added_count(void);
    int overlay_cg_get_removed_count(void);
    const char* overlay_cg_get_added_from(int idx);
    const char* overlay_cg_get_added_to(int idx);
    const char* overlay_cg_get_removed_from(int idx);
    const char* overlay_cg_get_removed_to(int idx);

    /* Diff visualization toggle */
    void overlay_cg_set_show_diff_overlay(int flag);
    int overlay_cg_get_show_diff_overlay(void);

    /* Export */
    void overlay_cg_export_diff_json(void);

#ifdef __cplusplus
}
#endif

#endif /* OVERLAY_CG_SNAPSHOT_DIFF_H */
