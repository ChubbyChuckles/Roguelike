#include "../core/app/app_state.h"
#include "../util/asset_dep.h"
#include "overlay_core.h"
#include "overlay_input.h"
#include "overlay_widgets.h"

#include <stdio.h>
#include <string.h>

#if ROGUE_ENABLE_DEBUG_OVERLAY

/* MSVC C89 compatibility: use compile-time macros for fixed array sizes (no VLAs) */
#define OVERLAY_CG_MAX_NODES 48
#define OVERLAY_CG_MAX_EDGES 96

/* Helper: does node `src_id` list `target_id` as a direct dependency? */
static int content_graph_has_dep_on(const char* src_id, const char* target_id)
{
    const char* deps[16];
    int dc =
        rogue_asset_dep_get_deps(src_id ? src_id : "", deps, (int) (sizeof deps / sizeof deps[0]));
    for (int i = 0; i < dc; ++i)
        if (deps[i] && target_id && strcmp(deps[i], target_id) == 0)
            return 1;
    return 0;
}

/* Helper: is `maybe_dep_id` a direct dependency of node `of_id`? */
static int content_graph_is_dep_of(const char* maybe_dep_id, const char* of_id)
{
    const char* deps[16];
    int dc =
        rogue_asset_dep_get_deps(of_id ? of_id : "", deps, (int) (sizeof deps / sizeof deps[0]));
    for (int i = 0; i < dc; ++i)
        if (deps[i] && maybe_dep_id && strcmp(deps[i], maybe_dep_id) == 0)
            return 1;
    return 0;
}

/* Helper: collect forward reachable nodes up to max_depth using a simple BFS. */
static int content_graph_collect_forward(const char* root_id, int max_depth, const char** out_ids,
                                         int* out_depths, int max_nodes, int (*out_edges)[2],
                                         int max_edges, int* out_edge_count)
{
    if (!root_id || !out_ids || !out_depths || max_nodes <= 0 || !out_edges || max_edges <= 0 ||
        !out_edge_count)
        return 0;
    int nc = 0;
    int ec = 0;
    /* enqueue root */
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
            /* find existing */
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

static void panel_content_graph(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("content_graph", "Content Graph", 1540, 10, 360))
        return;
    /* Filter + selection + dependency list; group nodes by top-level prefix before '/' */
    static int sel = 0;
    static char filter[64] = "";
    static int draw_edges = 1;    /* simple on-panel edge preview for selected node */
    static int preview_depth = 2; /* multi-hop preview depth (>=1) */
    static int group_only = 0; /* when on, filter list is constrained to selected group's prefix */
    /* Navigation breadcrumbs for click-to-drill within the SDL preview */
    static const char* crumbs[32];
    static int crumb_len = 0;
    overlay_input_text("Filter (substring)", filter, sizeof filter);
    /* Quick action: compute & cache all node hashes to surface issues early */
    if (overlay_button("Compute All Hashes"))
    {
        int total = rogue_asset_dep_count();
        for (int i = 0; i < total; ++i)
        {
            const char *nid = NULL, *pp = NULL;
            if (rogue_asset_dep_get(i, &nid, &pp) == 0 && nid)
            {
                unsigned long long h = 0ULL;
                (void) rogue_asset_dep_hash(nid, &h);
            }
        }
    }
    /* Export a simple Graphviz DOT */
    if (overlay_button("Export DOT (build/content_graph.dot)"))
    {
        FILE* f = NULL;
#if defined(_MSC_VER)
        fopen_s(&f, "build/content_graph.dot", "wb");
#else
        f = fopen("build/content_graph.dot", "wb");
#endif
        if (f)
        {
            fputs("digraph ContentGraph {\n  rankdir=LR;\n  node [shape=box,fontname=Helvetica];\n",
                  f);
            int nall = rogue_asset_dep_count();
            for (int i = 0; i < nall; ++i)
            {
                const char *nid = NULL, *pp = NULL;
                if (rogue_asset_dep_get(i, &nid, &pp) != 0 || !nid)
                    continue;
                char lbl[512];
                snprintf(lbl, sizeof lbl, "%s\\n%s", nid, (pp && *pp) ? pp : "<none>");
                for (char* p = lbl; *p; ++p)
                    if (*p == '"')
                        *p = '\'';
                fprintf(f, "  \"%s\" [label=\"%s\"];\n", nid, lbl);
                const char* deps[16];
                int dc = rogue_asset_dep_get_deps(nid, deps, (int) (sizeof deps / sizeof deps[0]));
                for (int j = 0; j < dc; ++j)
                {
                    if (deps[j])
                        fprintf(f, "  \"%s\" -> \"%s\";\n", nid, deps[j]);
                }
            }
            fputs("}\n", f);
            fclose(f);
            overlay_label("DOT exported.");
        }
        else
        {
            overlay_label("Failed to open output file.");
        }
    }
    /* Export JSON */
    if (overlay_button("Export JSON (build/content_graph.json)"))
    {
        FILE* f = NULL;
#if defined(_MSC_VER)
        fopen_s(&f, "build/content_graph.json", "wb");
#else
        f = fopen("build/content_graph.json", "wb");
#endif
        if (f)
        {
            fputs("{\n  \"nodes\": [\n", f);
            int nall = rogue_asset_dep_count();
            int wrote = 0;
            for (int i = 0; i < nall; ++i)
            {
                const char *nid = NULL, *pp = NULL;
                if (rogue_asset_dep_get(i, &nid, &pp) != 0 || !nid)
                    continue;
                if (wrote)
                    fputs(",\n", f);
                unsigned long long hv = 0ULL;
                (void) rogue_asset_dep_hash(nid, &hv);
                fprintf(f, "    {\"id\":\"%s\",\"hash\":\"0x%016llx\",\"deps\":[", nid, hv);
                const char* deps[64];
                int dc = rogue_asset_dep_get_deps(nid, deps, (int) (sizeof deps / sizeof deps[0]));
                int wrote_dep = 0;
                for (int j = 0; j < dc; ++j)
                {
                    const char* did = deps[j];
                    if (!did || !did[0])
                        continue;
                    if (wrote_dep)
                        fputs(",", f);
                    fprintf(f, "\"%s\"", did);
                    wrote_dep = 1;
                }
                fputs("]}", f);
                wrote = 1;
            }
            fputs("\n  ]\n}\n", f);
            fclose(f);
            overlay_label("JSON exported.");
        }
        else
        {
            overlay_label("Failed to open output file.");
        }
    }
    int n = rogue_asset_dep_count();
    if (n <= 0)
    {
        overlay_label("No content graph nodes registered.");
        overlay_end_panel();
        return;
    }
    int idxs[256];
    int idx_count = 0;
    const char* cur_group = NULL;
    if (group_only && sel >= 0 && sel < n)
    {
        const char *sid = NULL, *sp = NULL;
        if (rogue_asset_dep_get(sel, &sid, &sp) == 0 && sid)
        {
            const char* s = strchr(sid, '/');
            if (s)
                cur_group = sid;
        }
    }
    const char* f = filter;
    const char* prefix = NULL; /* id|path|dep|rev|group|hash */
    const char* farg = NULL;
    if (f && *f)
    {
        const char* colon = strchr(f, ':');
        if (colon)
        {
            static char key[16];
            size_t kl = (size_t) (colon - f);
            if (kl >= sizeof key)
                kl = sizeof key - 1;
            memcpy(key, f, kl);
            key[kl] = '\0';
            prefix = key;
            farg = colon + 1;
        }
    }
    for (int i = 0; i < n && idx_count < (int) (sizeof idxs / sizeof idxs[0]); ++i)
    {
        const char *nid = NULL, *pp = NULL;
        if (rogue_asset_dep_get(i, &nid, &pp) == 0)
        {
            int passes_text = 1;
            if (f && *f)
            {
                if (prefix && farg && *prefix)
                {
                    if (strcmp(prefix, "id") == 0)
                        passes_text = (nid && strstr(nid, farg)) ? 1 : 0;
                    else if (strcmp(prefix, "path") == 0)
                        passes_text = (pp && strstr(pp, farg)) ? 1 : 0;
                    else if (strcmp(prefix, "group") == 0)
                    {
                        const char* s2 = NULL;
                        if (nid)
                            s2 = strchr(nid, '/');
                        size_t gl = s2 && nid ? (size_t) (s2 - nid) : 0;
                        passes_text =
                            (gl > 0 && strncmp(nid, farg, gl) == 0 && farg[gl] == '\0') ? 1 : 0;
                    }
                    else if (strcmp(prefix, "hash") == 0)
                    {
                        unsigned long long hv = 0ULL;
                        (void) rogue_asset_dep_hash(nid, &hv);
                        char hx[20];
                        snprintf(hx, sizeof hx, "%016llx", (unsigned long long) hv);
                        passes_text = (strstr(hx, farg) != 0) ? 1 : 0;
                    }
                    else if (strcmp(prefix, "rev") == 0)
                    {
                        passes_text = content_graph_has_dep_on(nid ? nid : "", farg);
                    }
                    else if (strcmp(prefix, "dep") == 0)
                    {
                        passes_text = content_graph_is_dep_of(nid ? nid : "", farg);
                    }
                    else
                    {
                        passes_text = ((nid && strstr(nid, f)) || (pp && strstr(pp, f))) ? 1 : 0;
                    }
                }
                else
                {
                    passes_text = ((nid && strstr(nid, f)) || (pp && strstr(pp, f))) ? 1 : 0;
                }
            }
            int passes_group = 1;
            if (cur_group && nid)
            {
                const char* s1 = strchr(cur_group, '/');
                const char* s2 = strchr(nid, '/');
                if (s1 && s2)
                {
                    size_t gL = (size_t) (s1 - cur_group);
                    passes_group = (strncmp(cur_group, nid, gL) == 0) ? 1 : 0;
                }
            }
            if (passes_text && passes_group)
            {
                idxs[idx_count++] = i;
            }
        }
    }
    if (idx_count <= 0)
    {
        overlay_label("No nodes match filter.");
        overlay_end_panel();
        return;
    }
    if (sel < 0)
        sel = 0;
    if (sel >= idx_count)
        sel = idx_count - 1;
    if (overlay_checkbox("Show only selected group", &group_only))
    {
    }
    overlay_checkbox("Draw edges (selected)", &draw_edges);
    if (preview_depth < 1)
        preview_depth = 1;
    overlay_slider_int("Preview depth", &preview_depth, 1, 3);
    overlay_slider_int("Node", &sel, 0, idx_count - 1);
    /* Finders: Orphans (no inbound deps) and Hubs (high out-degree) */
    if (overlay_columns_begin(2, NULL))
    {
        if (overlay_button("List Orphans"))
        {
            int total = rogue_asset_dep_count();
            int inbound[512];
            int cap = (int) (sizeof inbound / sizeof inbound[0]);
            if (total > cap)
                total = cap;
            for (int i = 0; i < total; ++i)
                inbound[i] = 0;
            for (int i = 0; i < total; ++i)
            {
                const char *oid = NULL, *opp = NULL;
                if (rogue_asset_dep_get(i, &oid, &opp) != 0 || !oid)
                    continue;
                const char* deps2[16];
                int dc2 =
                    rogue_asset_dep_get_deps(oid, deps2, (int) (sizeof deps2 / sizeof deps2[0]));
                for (int j = 0; j < dc2; ++j)
                {
                    const char* dj = deps2[j];
                    if (!dj)
                        continue;
                    for (int k = 0; k < total; ++k)
                    {
                        const char *tid = NULL, *tpp = NULL;
                        if (rogue_asset_dep_get(k, &tid, &tpp) == 0 && tid && strcmp(tid, dj) == 0)
                        {
                            inbound[k]++;
                            break;
                        }
                    }
                }
            }
            int found = 0;
            for (int i = 0; i < total; ++i)
            {
                if (inbound[i] == 0)
                {
                    const char *tid = NULL, *tpp = NULL;
                    if (rogue_asset_dep_get(i, &tid, &tpp) == 0 && tid)
                    {
                        overlay_label(tid);
                        found++;
                    }
                }
            }
            if (!found)
                overlay_label("<no orphans>");
        }
        overlay_next_column();
        if (overlay_button("List Hubs (top 10)"))
        {
            int total = rogue_asset_dep_count();
            int outdeg[512];
            int ord[512];
            int cap = (int) (sizeof outdeg / sizeof outdeg[0]);
            if (total > cap)
                total = cap;
            for (int i = 0; i < total; ++i)
            {
                const char *oid = NULL, *opp = NULL;
                if (rogue_asset_dep_get(i, &oid, &opp) == 0 && oid)
                {
                    const char* deps_tmp[256];
                    int dc = rogue_asset_dep_get_deps(oid, deps_tmp,
                                                      (int) (sizeof deps_tmp / sizeof deps_tmp[0]));
                    outdeg[i] = dc >= 0 ? dc : 0;
                }
                else
                {
                    outdeg[i] = 0;
                }
                ord[i] = i;
            }
            /* partial selection sort for top 10 */
            int top = total < 10 ? total : 10;
            for (int i = 0; i < top; ++i)
            {
                int best = i;
                for (int j = i + 1; j < total; ++j)
                    if (outdeg[j] > outdeg[best])
                        best = j;
                if (best != i)
                {
                    int t = outdeg[i];
                    outdeg[i] = outdeg[best];
                    outdeg[best] = t;
                    int oi = ord[i];
                    ord[i] = ord[best];
                    ord[best] = oi;
                }
            }
            for (int i = 0; i < top; ++i)
            {
                const char *hid = NULL, *hpp = NULL;
                if (rogue_asset_dep_get(ord[i], &hid, &hpp) == 0 && hid)
                {
                    char row[256];
                    snprintf(row, sizeof row, "%s (out=%d)", hid, outdeg[i]);
                    overlay_label(row);
                }
            }
        }
        overlay_columns_end();
    }
    int node_index = idxs[sel];
    const char *id = NULL, *path = NULL;
    if (rogue_asset_dep_get(node_index, &id, &path) == 0)
    {
        char line[200];
        const char* slash = NULL;
        if (id)
            slash = strchr(id, '/');
        char group[48];
        if (slash)
        {
            size_t gl = (size_t) (slash - id);
            if (gl >= sizeof group)
                gl = sizeof group - 1;
            memcpy(group, id, gl);
            group[gl] = '\0';
            snprintf(line, sizeof line, "Group: %s", group);
            overlay_label(line);
        }
        snprintf(line, sizeof line, "[%d/%d] id=%s", node_index, n, id ? id : "<nil>");
        overlay_label(line);
        snprintf(line, sizeof line, "path=%s", path && *path ? path : "<none>");
        overlay_label(line);
        if (id)
        {
            unsigned long long hv = 0ULL;
            if (rogue_asset_dep_hash(id, &hv) == 0)
            {
                snprintf(line, sizeof line, "hash=0x%016llx", (unsigned long long) hv);
                overlay_label(line);
            }
        }
        {
            char k[32], nid[64], dep[64];
            if (rogue_asset_dep_get_last_reject(k, (int) sizeof k, nid, (int) sizeof nid, dep,
                                                (int) sizeof dep))
            {
                char msg[256];
                snprintf(msg, sizeof msg, "Last register reject: kind=%s id=%s dep=%s", k, nid,
                         dep[0] ? dep : "<n/a>");
                overlay_label(msg);
            }
        }
        overlay_label("Deps:");
        const char* deps[16];
        int dc = rogue_asset_dep_get_deps(id, deps, (int) (sizeof deps / sizeof deps[0]));
        if (dc > 0)
        {
            for (int i = 0; i < dc; i++)
            {
                overlay_label(deps[i]);
            }
        }
        else
        {
            overlay_label("<none>");
        }
        overlay_label("Reverse Deps:");
        int rev_found = 0;
        for (int i = 0; i < n; ++i)
        {
            const char *oid = NULL, *opp = NULL;
            if (rogue_asset_dep_get(i, &oid, &opp) != 0 || !oid)
                continue;
            const char* tmp[16];
            int c = rogue_asset_dep_get_deps(oid, tmp, (int) (sizeof tmp / sizeof tmp[0]));
            for (int j = 0; j < c; ++j)
            {
                if (tmp[j] && id && strcmp(tmp[j], id) == 0)
                {
                    overlay_label(oid);
                    rev_found = 1;
                    break;
                }
            }
        }
        if (!rev_found)
            overlay_label("<none>");
        if (slash)
        {
            int group_count = 0;
            for (int i = 0; i < n; ++i)
            {
                const char *gid = NULL, *gpp = NULL;
                if (rogue_asset_dep_get(i, &gid, &gpp) != 0 || !gid)
                    continue;
                const char* s2 = strchr(gid, '/');
                if (s2)
                {
                    size_t g2l = (size_t) (s2 - gid);
                    if (g2l == (size_t) (slash - id) && strncmp(gid, id, g2l) == 0)
                        group_count++;
                }
            }
            snprintf(line, sizeof line, "Group size: %d", group_count);
            overlay_label(line);
        }
        if (overlay_columns_begin(2, NULL))
        {
            if (overlay_button("Export Subgraph DOT"))
            {
                const char* nids[128];
                int ndeps[128];
                int edges[256][2];
                int ecount = 0;
                int ncount = content_graph_collect_forward(id, preview_depth, nids, ndeps, 128,
                                                           edges, 256, &ecount);
                FILE* f = NULL;
#if defined(_MSC_VER)
                fopen_s(&f, "build/content_subgraph.dot", "wb");
#else
                f = fopen("build/content_subgraph.dot", "wb");
#endif
                if (f)
                {
                    fputs("digraph ContentSubgraph {\n  rankdir=LR;\n  node "
                          "[shape=box,fontname=Helvetica];\n",
                          f);
                    for (int i = 0; i < ncount; ++i)
                    {
                        const char* nid = nids[i];
                        if (!nid)
                            continue;
                        fprintf(f, "  \"%s\";\n", nid);
                    }
                    for (int i = 0; i < ecount; ++i)
                    {
                        int s = edges[i][0], t = edges[i][1];
                        if (s >= 0 && s < ncount && t >= 0 && t < ncount)
                            fprintf(f, "  \"%s\" -> \"%s\";\n", nids[s], nids[t]);
                    }
                    fputs("}\n", f);
                    fclose(f);
                    overlay_label("Subgraph DOT exported (build/content_subgraph.dot).");
                }
                else
                {
                    overlay_label("Failed to open subgraph DOT output.");
                }
            }
            overlay_next_column();
            if (overlay_button("Export Subgraph JSON"))
            {
                const char* nids[128];
                int ndeps[128];
                int edges[256][2];
                int ecount = 0;
                int ncount = content_graph_collect_forward(id, preview_depth, nids, ndeps, 128,
                                                           edges, 256, &ecount);
                FILE* f = NULL;
#if defined(_MSC_VER)
                fopen_s(&f, "build/content_subgraph.json", "wb");
#else
                f = fopen("build/content_subgraph.json", "wb");
#endif
                if (f)
                {
                    fprintf(f, "{\n  \"root\":\"%s\",\n  \"depth\":%d,\n  \"nodes\": [\n",
                            id ? id : "", preview_depth);
                    for (int i = 0; i < ncount; ++i)
                    {
                        unsigned long long hv = 0ULL;
                        (void) rogue_asset_dep_hash(nids[i] ? nids[i] : "", &hv);
                        fprintf(f, "    {\"id\":\"%s\",\"hash\":\"0x%016llx\"}%s\n",
                                nids[i] ? nids[i] : "", hv, (i + 1 < ncount) ? "," : "");
                    }
                    fputs("  ],\n  \"edges\": [\n", f);
                    for (int i = 0; i < ecount; ++i)
                    {
                        int s = edges[i][0], t = edges[i][1];
                        if (s >= 0 && s < ncount && t >= 0 && t < ncount)
                        {
                            fprintf(f, "    {\"from\":\"%s\",\"to\":\"%s\"}%s\n", nids[s], nids[t],
                                    (i + 1 < ecount) ? "," : "");
                        }
                    }
                    fputs("  ]\n}\n", f);
                    fclose(f);
                    overlay_label("Subgraph JSON exported (build/content_subgraph.json).");
                }
                else
                {
                    overlay_label("Failed to open subgraph JSON output.");
                }
            }
            overlay_columns_end();
        }
        if (slash)
        {
            static int edges_open = 1;
            if (overlay_tree_node("Edges (group)", &edges_open))
            {
                size_t gl = (size_t) (slash - id);
                int edge_lines = 0;
                for (int i = 0; i < n; ++i)
                {
                    const char *gid = NULL, *gpp = NULL;
                    if (rogue_asset_dep_get(i, &gid, &gpp) != 0 || !gid)
                        continue;
                    const char* s2 = strchr(gid, '/');
                    if (!s2)
                        continue;
                    size_t g2l = (size_t) (s2 - gid);
                    if (g2l == gl && strncmp(gid, id, gl) == 0)
                    {
                        const char* deps2[16];
                        int dc2 = rogue_asset_dep_get_deps(gid, deps2,
                                                           (int) (sizeof deps2 / sizeof deps2[0]));
                        char row[256];
                        int off = snprintf(row, sizeof row, "%s -> ", gid);
                        if (dc2 <= 0)
                        {
                            snprintf(row + off, (size_t) (sizeof row - off), "<none>");
                        }
                        else
                        {
                            for (int j = 0; j < dc2; ++j)
                            {
                                const char* dj = deps2[j] ? deps2[j] : "?";
                                int left = (int) sizeof(row) - off;
                                if (left > 4)
                                {
                                    off += snprintf(row + off, (size_t) left, "%s%s", dj,
                                                    (j + 1 < dc2) ? ", " : "");
                                }
                            }
                        }
                        overlay_label(row);
                        edge_lines++;
                    }
                }
                if (edge_lines == 0)
                    overlay_label("<no group edges>");
                overlay_tree_pop();
            }
        }
#ifdef ROGUE_HAVE_SDL
        if (draw_edges && g_app.renderer)
        {
            const int panel_x = 1920 - 380;
            const int panel_y = 10;
            const int panel_w = 360;
            const int cx = panel_x + 12;
            const int cy = panel_y + 120;
            const int cw = panel_w - 24;
            const int ch = 180;
            SDL_Rect area = {cx, cy, cw, ch};
            SDL_SetRenderDrawColor(g_app.renderer, 12, 12, 12, 180);
            SDL_RenderFillRect(g_app.renderer, &area);
            SDL_SetRenderDrawColor(g_app.renderer, 100, 100, 140, 220);
            SDL_RenderDrawRect(g_app.renderer, &area);
            const char* nids[OVERLAY_CG_MAX_NODES];
            int ndeps[OVERLAY_CG_MAX_NODES];
            int edges[OVERLAY_CG_MAX_EDGES][2];
            int ecount = 0;
            int ncount =
                content_graph_collect_forward(id, preview_depth, nids, ndeps, OVERLAY_CG_MAX_NODES,
                                              edges, OVERLAY_CG_MAX_EDGES, &ecount);
            int parent[OVERLAY_CG_MAX_NODES];
            for (int i = 0; i < OVERLAY_CG_MAX_NODES; ++i)
                parent[i] = -1;
            for (int i = 0; i < ecount; ++i)
            {
                int s = edges[i][0], t = edges[i][1];
                if (s >= 0 && s < ncount && t >= 0 && t < ncount)
                {
                    if (parent[t] < 0 && t != 0)
                        parent[t] = s;
                }
            }
            int depth_counts[8] = {0};
            int max_d = 0;
            for (int i = 0; i < ncount; ++i)
            {
                int d = (ndeps[i] < 0) ? 0 : ndeps[i];
                if (d > 7)
                    d = 7;
                depth_counts[d]++;
                if (d > max_d)
                    max_d = d;
            }
            int col_w = (max_d + 1) > 0 ? cw / (max_d + 1) : cw;
            SDL_Rect rects[OVERLAY_CG_MAX_NODES];
            int placed_at_depth[8] = {0};
            for (int i = 0; i < ncount; ++i)
            {
                int d = ndeps[i];
                if (d < 0)
                    d = 0;
                if (d > max_d)
                    d = max_d;
                int per = depth_counts[d] > 0 ? depth_counts[d] : 1;
                int idx = placed_at_depth[d]++;
                int x = cx + d * col_w + 6;
                int y = cy + 8 + (per == 1 ? (ch / 2 - 10) : (idx * (ch - 24) / (per - 1)));
                rects[i].x = x;
                rects[i].y = y;
                rects[i].w = (col_w > 140 ? 120 : (col_w - 20 > 60 ? col_w - 20 : 60));
                rects[i].h = 20;
            }
            SDL_SetRenderDrawColor(g_app.renderer, 180, 180, 220, 220);
            for (int i = 0; i < ecount; ++i)
            {
                int s = edges[i][0], t = edges[i][1];
                if (s >= 0 && s < ncount && t >= 0 && t < ncount)
                {
                    int x0 = rects[s].x + rects[s].w;
                    int y0 = rects[s].y + rects[s].h / 2;
                    int x1 = rects[t].x;
                    int y1 = rects[t].y + rects[t].h / 2;
                    SDL_RenderDrawLine(g_app.renderer, x0, y0, x1, y1);
                }
            }
            {
                char kind[32] = {0}, nid2[64] = {0}, dep2[64] = {0};
                if (rogue_asset_dep_get_last_reject(kind, (int) sizeof kind, nid2,
                                                    (int) sizeof nid2, dep2, (int) sizeof dep2))
                {
                    if (strcmp(kind, "cycle") == 0 && nid2[0] && dep2[0])
                    {
                        int a = -1, b = -1;
                        for (int i = 0; i < ncount; ++i)
                        {
                            if (nids[i] && strcmp(nids[i], nid2) == 0)
                                a = i;
                            if (nids[i] && strcmp(nids[i], dep2) == 0)
                                b = i;
                        }
                        if (a >= 0 && b >= 0)
                        {
                            SDL_SetRenderDrawColor(g_app.renderer, 220, 60, 60, 240);
                            int x0 = rects[a].x + rects[a].w;
                            int y0 = rects[a].y + rects[a].h / 2;
                            int x1 = rects[b].x;
                            int y1 = rects[b].y + rects[b].h / 2;
                            SDL_RenderDrawLine(g_app.renderer, x0, y0, x1, y1);
                        }
                    }
                }
            }
            for (int i = 0; i < ncount; ++i)
            {
                SDL_Rect r = rects[i];
                if (i == 0)
                {
                    SDL_SetRenderDrawColor(g_app.renderer, 40, 70, 110, 220);
                }
                else
                {
                    unsigned ghash = 2166136261u;
                    const char* gid = nids[i];
                    const char* slash2 = gid ? strchr(gid, '/') : NULL;
                    int gl = 0;
                    if (gid && slash2)
                        gl = (int) (slash2 - gid);
                    for (int c = 0; c < gl; ++c)
                    {
                        ghash ^= (unsigned) gid[c];
                        ghash *= 16777619u;
                    }
                    unsigned r8 = 60u + (ghash & 95u);
                    unsigned g8 = 60u + ((ghash >> 8) & 95u);
                    unsigned b8 = 60u + ((ghash >> 16) & 95u);
                    SDL_SetRenderDrawColor(g_app.renderer, (Uint8) r8, (Uint8) g8, (Uint8) b8, 220);
                }
                SDL_RenderFillRect(g_app.renderer, &r);
                SDL_SetRenderDrawColor(g_app.renderer, 220, 220, 220, 220);
                SDL_RenderDrawRect(g_app.renderer, &r);
                const char* label = nids[i] ? nids[i] : "?";
                rogue_font_draw_text(r.x + 4, r.y + 4, label, 1,
                                     (RogueColor){i == 0 ? 255 : 220, 255, 220, 255});
            }
            const OverlayInputState* in = overlay_input_get();
            if (in && in->mouse_clicked)
            {
                int mx = (int) in->mouse_x, my = (int) in->mouse_y;
                if (mx >= area.x && mx <= area.x + area.w && my >= area.y && my <= area.y + area.h)
                {
                    for (int i = 0; i < ncount; ++i)
                    {
                        SDL_Rect rr = rects[i];
                        if (mx >= rr.x && mx <= rr.x + rr.w && my >= rr.y && my <= rr.y + rr.h)
                        {
                            const char* clicked = nids[i];
                            if (clicked)
                            {
                                const char* tmp[32];
                                int tlen = 0;
                                int cur = i;
                                while (cur >= 0 && tlen < (int) (sizeof tmp / sizeof tmp[0]))
                                {
                                    tmp[tlen++] = nids[cur];
                                    if (cur == 0)
                                        break;
                                    cur = parent[cur];
                                }
                                crumb_len = 0;
                                for (int k = tlen - 1;
                                     k >= 0 && crumb_len < (int) (sizeof crumbs / sizeof crumbs[0]);
                                     --k)
                                    crumbs[crumb_len++] = tmp[k];

                                int nall = rogue_asset_dep_count();
                                int glob = -1;
                                for (int gi = 0; gi < nall; ++gi)
                                {
                                    const char *gid2 = NULL, *gpp2 = NULL;
                                    if (rogue_asset_dep_get(gi, &gid2, &gpp2) == 0 && gid2 &&
                                        strcmp(gid2, clicked) == 0)
                                    {
                                        glob = gi;
                                        break;
                                    }
                                }
                                if (glob >= 0)
                                {
                                    int found = -1;
                                    for (int p = 0; p < idx_count; ++p)
                                    {
                                        if (idxs[p] == glob)
                                        {
                                            found = p;
                                            break;
                                        }
                                    }
                                    if (found >= 0)
                                    {
                                        sel = found;
                                    }
                                    else
                                    {
                                        snprintf(filter, sizeof filter, "id:%s", clicked);
                                        group_only = 0;
                                        sel = 0;
                                    }
                                }
                            }
                            break;
                        }
                    }
                }
            }
            rogue_font_draw_text(cx + 6, cy + ch - 14, "Graph is DAG (cycles rejected)", 1,
                                 (RogueColor){160, 200, 255, 255});
        }
#endif
    }
    if (crumb_len > 0)
    {
        char pathline[320];
        int off = snprintf(pathline, sizeof pathline, "Path: ");
        for (int i = 0; i < crumb_len; ++i)
        {
            const char* s = crumbs[i] ? crumbs[i] : "?";
            int left = (int) sizeof(pathline) - off;
            if (left <= 4)
                break;
            off += snprintf(pathline + off, (size_t) left, "%s%s", s,
                            (i + 1 < crumb_len) ? " -> " : "");
        }
        overlay_label(pathline);
        if (crumb_len > 1)
        {
            if (overlay_button("Back"))
            {
                const char* target = crumbs[crumb_len - 2];
                if (target)
                {
                    int nall = rogue_asset_dep_count();
                    int glob = -1;
                    for (int gi = 0; gi < nall; ++gi)
                    {
                        const char *gid2 = NULL, *gpp2 = NULL;
                        if (rogue_asset_dep_get(gi, &gid2, &gpp2) == 0 && gid2 &&
                            strcmp(gid2, target) == 0)
                        {
                            glob = gi;
                            break;
                        }
                    }
                    if (glob >= 0)
                    {
                        int found = -1;
                        for (int p = 0; p < idx_count; ++p)
                            if (idxs[p] == glob)
                            {
                                found = p;
                                break;
                            }
                        if (found >= 0)
                            sel = found;
                        else
                        {
                            snprintf(filter, sizeof filter, "id:%s", target);
                            group_only = 0;
                            sel = 0;
                        }
                    }
                }
                if (crumb_len > 0)
                    crumb_len--;
            }
        }
    }
    overlay_end_panel();
}

void rogue_overlay_register_panel_content_graph(void)
{
    overlay_register_panel("content_graph", "Content Graph", panel_content_graph, NULL);
}

#endif
