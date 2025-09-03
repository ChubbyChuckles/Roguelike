/* overlay_cg_snapshot_diff.c: implementation of snapshot & diff for Content Graph overlay */
#include <stdio.h>
#include <string.h>

#include "../widgets/overlay_widgets.h" /* overlay_label */
#include "overlay_cg_snapshot_diff.h"

static OverlayCGSnapshot g_snapA = {0};
static OverlayCGSnapshot g_snapB = {0};

static int g_diff_ready = 0;
static int g_added_count = 0, g_removed_count = 0;
static char g_added_from[OVERLAY_CG_MAX_EDGES][64];
static char g_added_to[OVERLAY_CG_MAX_EDGES][64];
static char g_removed_from[OVERLAY_CG_MAX_EDGES][64];
static char g_removed_to[OVERLAY_CG_MAX_EDGES][64];
static int g_show_diff_overlay = 1;

void overlay_cg_capture_snapshot(OverlayCGSnapshot* snap, const char* root_id, int depth,
                                 const char** nids, int ncount, int (*edges)[2], int ecount)
{
    int i;
    if (!snap)
        return;
    snap->valid = 0;
    snap->depth = depth;
    {
        size_t rl = root_id ? strlen(root_id) : 0;
        if (rl >= sizeof snap->root)
            rl = sizeof snap->root - 1;
        if (rl > 0)
            memcpy(snap->root, root_id, rl);
        snap->root[rl] = '\0';
    }
    snap->ncount = (ncount > OVERLAY_CG_MAX_NODES) ? OVERLAY_CG_MAX_NODES : ncount;
    for (i = 0; i < snap->ncount; ++i)
    {
        const char* s = nids[i] ? nids[i] : "";
        size_t sl = strlen(s);
        if (sl >= sizeof snap->nodes[0])
            sl = sizeof snap->nodes[0] - 1;
        memcpy(snap->nodes[i], s, sl);
        snap->nodes[i][sl] = '\0';
    }
    snap->ecount = (ecount > OVERLAY_CG_MAX_EDGES) ? OVERLAY_CG_MAX_EDGES : ecount;
    for (i = 0; i < snap->ecount; ++i)
    {
        int sidx = edges[i][0];
        int tidx = edges[i][1];
        const char* sf = (sidx >= 0 && sidx < ncount) ? nids[sidx] : "";
        const char* st = (tidx >= 0 && tidx < ncount) ? nids[tidx] : "";
        size_t slf = strlen(sf);
        size_t slt = strlen(st);
        if (slf >= sizeof snap->edge_from[0])
            slf = sizeof snap->edge_from[0] - 1;
        if (slt >= sizeof snap->edge_to[0])
            slt = sizeof snap->edge_to[0] - 1;
        memcpy(snap->edge_from[i], sf, slf);
        snap->edge_from[i][slf] = '\0';
        memcpy(snap->edge_to[i], st, slt);
        snap->edge_to[i][slt] = '\0';
    }
    snap->valid = 1;
}

void overlay_cg_set_snapshot_a(const OverlayCGSnapshot* s)
{
    if (s)
        g_snapA = *s;
}
void overlay_cg_set_snapshot_b(const OverlayCGSnapshot* s)
{
    if (s)
        g_snapB = *s;
}
const OverlayCGSnapshot* overlay_cg_get_snapshot_a(void) { return &g_snapA; }
const OverlayCGSnapshot* overlay_cg_get_snapshot_b(void) { return &g_snapB; }

void overlay_cg_reset_diff(void)
{
    int i;
    g_diff_ready = 0;
    g_added_count = 0;
    g_removed_count = 0;
    for (i = 0; i < OVERLAY_CG_MAX_EDGES; ++i)
    {
        g_added_from[i][0] = '\0';
        g_added_to[i][0] = '\0';
        g_removed_from[i][0] = '\0';
        g_removed_to[i][0] = '\0';
    }
}

static int overlay_cg_edge_eq(const char* a0, const char* a1, const char* b0, const char* b1)
{
    if (!a0)
        a0 = "";
    if (!a1)
        a1 = "";
    if (!b0)
        b0 = "";
    if (!b1)
        b1 = "";
    return (strcmp(a0, b0) == 0) && (strcmp(a1, b1) == 0);
}

void overlay_cg_compute_diff(void)
{
    int i, j;
    overlay_cg_reset_diff();
    if (!g_snapA.valid || !g_snapB.valid)
        return;
    for (i = 0; i < g_snapB.ecount; ++i)
    {
        const char* bf = g_snapB.edge_from[i];
        const char* bt = g_snapB.edge_to[i];
        int found = 0;
        for (j = 0; j < g_snapA.ecount; ++j)
        {
            if (overlay_cg_edge_eq(bf, bt, g_snapA.edge_from[j], g_snapA.edge_to[j]))
            {
                found = 1;
                break;
            }
        }
        if (!found && g_added_count < OVERLAY_CG_MAX_EDGES)
        {
            size_t lf = strlen(bf), lt = strlen(bt);
            if (lf >= sizeof g_added_from[0])
                lf = sizeof g_added_from[0] - 1;
            if (lt >= sizeof g_added_to[0])
                lt = sizeof g_added_to[0] - 1;
            memcpy(g_added_from[g_added_count], bf, lf);
            g_added_from[g_added_count][lf] = '\0';
            memcpy(g_added_to[g_added_count], bt, lt);
            g_added_to[g_added_count][lt] = '\0';
            g_added_count++;
        }
    }
    for (i = 0; i < g_snapA.ecount; ++i)
    {
        const char* af = g_snapA.edge_from[i];
        const char* at = g_snapA.edge_to[i];
        int found = 0;
        for (j = 0; j < g_snapB.ecount; ++j)
        {
            if (overlay_cg_edge_eq(af, at, g_snapB.edge_from[j], g_snapB.edge_to[j]))
            {
                found = 1;
                break;
            }
        }
        if (!found && g_removed_count < OVERLAY_CG_MAX_EDGES)
        {
            size_t lf = strlen(af), lt = strlen(at);
            if (lf >= sizeof g_removed_from[0])
                lf = sizeof g_removed_from[0] - 1;
            if (lt >= sizeof g_removed_to[0])
                lt = sizeof g_removed_to[0] - 1;
            memcpy(g_removed_from[g_removed_count], af, lf);
            g_removed_from[g_removed_count][lf] = '\0';
            memcpy(g_removed_to[g_removed_count], at, lt);
            g_removed_to[g_removed_count][lt] = '\0';
            g_removed_count++;
        }
    }
    g_diff_ready = 1;
}

int overlay_cg_diff_ready(void) { return g_diff_ready; }

int overlay_cg_get_added_count(void) { return g_added_count; }
const char* overlay_cg_get_added_from(int idx)
{
    return (idx >= 0 && idx < g_added_count) ? g_added_from[idx] : "";
}
const char* overlay_cg_get_added_to(int idx)
{
    return (idx >= 0 && idx < g_added_count) ? g_added_to[idx] : "";
}

int overlay_cg_get_removed_count(void) { return g_removed_count; }
const char* overlay_cg_get_removed_from(int idx)
{
    return (idx >= 0 && idx < g_removed_count) ? g_removed_from[idx] : "";
}
const char* overlay_cg_get_removed_to(int idx)
{
    return (idx >= 0 && idx < g_removed_count) ? g_removed_to[idx] : "";
}

void overlay_cg_set_show_diff_overlay(int show) { g_show_diff_overlay = show ? 1 : 0; }
int overlay_cg_get_show_diff_overlay(void) { return g_show_diff_overlay; }

void overlay_cg_export_diff_json(void)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "build/content_subgraph_diff.json", "wb");
#else
    f = fopen("build/content_subgraph_diff.json", "wb");
#endif
    if (!f)
    {
        overlay_label("Failed to open diff JSON output.");
        return;
    }
    const char* nodes_u[OVERLAY_CG_MAX_NODES * 2];
    int nu = 0;
    int i, j;
    for (i = 0; i < g_snapA.ncount && nu < (int) (sizeof nodes_u / sizeof nodes_u[0]); ++i)
        nodes_u[nu++] = g_snapA.nodes[i];
    for (i = 0; i < g_snapB.ncount && nu < (int) (sizeof nodes_u / sizeof nodes_u[0]); ++i)
    {
        int seen = 0;
        for (j = 0; j < nu; ++j)
            if (strcmp(nodes_u[j] ? nodes_u[j] : "", g_snapB.nodes[i]) == 0)
            {
                seen = 1;
                break;
            }
        if (!seen)
            nodes_u[nu++] = g_snapB.nodes[i];
    }

    fputs("{\n", f);
    fprintf(f, "  \"rootA\": \"%s\", \"depthA\": %d,\n", g_snapA.root, g_snapA.depth);
    fprintf(f, "  \"rootB\": \"%s\", \"depthB\": %d,\n", g_snapB.root, g_snapB.depth);

    fputs("  \"degree_deltas\": [\n", f);
    for (i = 0; i < nu; ++i)
    {
        const char* id = nodes_u[i] ? nodes_u[i] : "";
        int outA = 0, inA = 0, outB = 0, inB = 0;
        for (j = 0; j < g_snapA.ecount; ++j)
        {
            if (strcmp(g_snapA.edge_from[j], id) == 0)
                outA++;
            if (strcmp(g_snapA.edge_to[j], id) == 0)
                inA++;
        }
        for (j = 0; j < g_snapB.ecount; ++j)
        {
            if (strcmp(g_snapB.edge_from[j], id) == 0)
                outB++;
            if (strcmp(g_snapB.edge_to[j], id) == 0)
                inB++;
        }
        fprintf(f,
                "    "
                "{\"id\":\"%s\",\"out_before\":%d,\"out_after\":%d,\"delta_out\":%d,\"in_before\":%"
                "d,\"in_after\":%d,\"delta_in\":%d}%s\n",
                id, outA, outB, (outB - outA), inA, inB, (inB - inA), (i + 1 < nu) ? "," : "");
    }
    fputs("  ],\n", f);

    fputs("  \"added_edges\": [\n", f);
    for (i = 0; i < g_added_count; ++i)
    {
        fprintf(f, "    {\"from\":\"%s\",\"to\":\"%s\"}%s\n", g_added_from[i], g_added_to[i],
                (i + 1 < g_added_count) ? "," : "");
    }
    fputs("  ],\n", f);
    fputs("  \"removed_edges\": [\n", f);
    for (i = 0; i < g_removed_count; ++i)
    {
        fprintf(f, "    {\"from\":\"%s\",\"to\":\"%s\"}%s\n", g_removed_from[i], g_removed_to[i],
                (i + 1 < g_removed_count) ? "," : "");
    }
    fputs("  ],\n", f);
    fputs("  \"cycles\": []\n}", f);
    fclose(f);
    overlay_label("Diff JSON exported (build/content_subgraph_diff.json).");
}
