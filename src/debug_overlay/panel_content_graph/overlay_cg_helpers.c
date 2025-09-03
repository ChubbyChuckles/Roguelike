/* overlay_cg_helpers.c: implementations for Content Graph helper utilities */
#include <string.h>

#include "../../util/asset_dep.h"
#include "overlay_cg_helpers.h"

int content_graph_has_dep_on(const char* src_id, const char* target_id)
{
    const char* deps[16];
    int dc =
        rogue_asset_dep_get_deps(src_id ? src_id : "", deps, (int) (sizeof deps / sizeof deps[0]));
    for (int i = 0; i < dc; ++i)
        if (deps[i] && target_id && strcmp(deps[i], target_id) == 0)
            return 1;
    return 0;
}

int content_graph_is_dep_of(const char* maybe_dep_id, const char* of_id)
{
    const char* deps[16];
    int dc =
        rogue_asset_dep_get_deps(of_id ? of_id : "", deps, (int) (sizeof deps / sizeof deps[0]));
    for (int i = 0; i < dc; ++i)
        if (deps[i] && maybe_dep_id && strcmp(deps[i], maybe_dep_id) == 0)
            return 1;
    return 0;
}

int content_graph_collect_forward(const char* root_id, int max_depth, const char** out_ids,
                                  int* out_depths, int max_nodes, int (*out_edges)[2],
                                  int max_edges, int* out_edge_count)
{
    if (!root_id || !out_ids || !out_depths || max_nodes <= 0 || !out_edges || max_edges <= 0 ||
        !out_edge_count)
        return 0;
    int nc = 0;
    int ec = 0;
    out_ids[nc] = root_id;
    out_depths[nc] = 0;
    int qh = 0, qt = 0;
    int qidx[128];
    qidx[qt++] = nc;
    nc++;
    while (qh < qt)
    {
        int si = qidx[qh++];
        const char* sid = out_ids[si];
        int sd = out_depths[si];
        if (sd >= max_depth)
            continue;
        const char* deps[16];
        int dc =
            rogue_asset_dep_get_deps(sid ? sid : "", deps, (int) (sizeof deps / sizeof deps[0]));
        for (int i = 0; i < dc; ++i)
        {
            const char* did = deps[i];
            if (!did)
                continue;
            int di = -1;
            for (int k = 0; k < nc; ++k)
            {
                if (out_ids[k] && strcmp(out_ids[k], did) == 0)
                {
                    di = k;
                    break;
                }
            }
            if (di < 0)
            {
                if (nc < max_nodes)
                {
                    di = nc;
                    out_ids[nc] = did;
                    out_depths[nc] = sd + 1;
                    qidx[qt++ % (int) (sizeof qidx / sizeof qidx[0])] = nc;
                    nc++;
                }
                else
                {
                    di = 0;
                }
            }
            if (ec < max_edges)
            {
                out_edges[ec][0] = si;
                out_edges[ec][1] = di;
                ec++;
            }
        }
    }
    *out_edge_count = ec;
    return nc;
}
