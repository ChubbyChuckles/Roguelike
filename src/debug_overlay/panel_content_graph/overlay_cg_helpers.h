/*
 * overlay_cg_helpers.h
 * Lightweight helpers for Content Graph overlay panel (dependency checks and forward collection).
 */
#ifndef OVERLAY_CG_HELPERS_H
#define OVERLAY_CG_HELPERS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Forward reachable nodes collection (BFS) up to max_depth. Returns node count. */
    int content_graph_collect_forward(const char* root_id, int max_depth, const char** out_ids,
                                      int* out_depths, int max_nodes, int (*out_edges)[2],
                                      int max_edges, int* out_edge_count);

    /* Direct dependency checks. */
    int content_graph_has_dep_on(const char* src_id, const char* target_id);
    int content_graph_is_dep_of(const char* maybe_dep_id, const char* of_id);

#ifdef __cplusplus
}
#endif

#endif /* OVERLAY_CG_HELPERS_H */
#ifndef OVERLAY_CG_HELPERS_H
#define OVERLAY_CG_HELPERS_H

#ifdef __cplusplus
extern "C"
{
#endif

    /* Forward declarations for content graph helpers */

    int content_graph_has_dep_on(const char* src_id, const char* target_id);
    int content_graph_is_dep_of(const char* maybe_dep_id, const char* of_id);

    int content_graph_collect_forward(const char* root_id, int max_depth, const char** out_ids,
                                      int* out_depths, int max_nodes, int (*out_edges)[2],
                                      int max_edges, int* out_edge_count);

#ifdef __cplusplus
}
#endif

#endif /* OVERLAY_CG_HELPERS_H */
