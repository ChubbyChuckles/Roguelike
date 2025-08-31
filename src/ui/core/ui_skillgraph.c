#include "ui_context.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Implementation split from ui_context.c: Skill Graph (zoomable, panning, quadtree culling) */

typedef struct RogueUISkillNodeRec
{
    float x, y; /* world center */
    int icon_id;
    int rank;
    int max_rank;
    int synergy;       /* non-zero => glow */
    unsigned int tags; /* Phase 5.5 filtering */
} RogueUISkillNodeRec;

typedef struct SkillQuadNode
{
    float x, y, w, h; /* bounds */
    int first_index;  /* index into flat list of child node indices */
    int count;
    int children[4]; /* -1 if leaf */
} SkillQuadNode;

typedef struct SkillQuadTree
{
    SkillQuadNode* nodes;
    int node_count;
    int node_cap;
    int* indices;
    int index_count;
    int index_cap;
} SkillQuadTree;

static SkillQuadTree* skillgraph_quadtree_create(void)
{
    return (SkillQuadTree*) calloc(1, sizeof(SkillQuadTree));
}
static void skillgraph_quadtree_reset(SkillQuadTree* q)
{
    if (!q)
        return;
    q->node_count = 0;
    q->index_count = 0;
}
static int skillgraph_qt_push_node(SkillQuadTree* q, SkillQuadNode n)
{
    if (q->node_count >= q->node_cap)
    {
        int nc = q->node_cap ? q->node_cap * 2 : 32;
        q->nodes = (SkillQuadNode*) realloc(q->nodes, nc * sizeof(SkillQuadNode));
        q->node_cap = nc;
    }
    q->nodes[q->node_count] = n;
    return q->node_count++;
}
static int skillgraph_qt_push_index(SkillQuadTree* q, int v)
{
    if (q->index_count >= q->index_cap)
    {
        int nc = q->index_cap ? q->index_cap * 2 : 64;
        q->indices = (int*) realloc(q->indices, nc * sizeof(int));
        q->index_cap = nc;
    }
    q->indices[q->index_count] = v;
    return q->index_count++;
}
static void skillgraph_qt_subdivide(SkillQuadTree* q, int node_index, RogueUISkillNodeRec* nodes)
{
    SkillQuadNode* nd = &q->nodes[node_index];
    if (nd->count <= 8)
        return;
    float hw = nd->w * 0.5f, hh = nd->h * 0.5f;
    float xs[2] = {nd->x, nd->x + hw};
    float ys[2] = {nd->y, nd->y + hh};
    for (int i = 0; i < 4; i++)
    {
        SkillQuadNode child;
        child.x = xs[i & 1];
        child.y = ys[i >> 1];
        child.w = hw;
        child.h = hh;
        child.first_index = q->index_count;
        child.count = 0;
        for (int k = 0; k < 4; k++)
            child.children[k] = -1;
        int ci = skillgraph_qt_push_node(q, child);
        nd->children[i] = ci;
    }
    int start = nd->first_index;
    int cnt = nd->count;
    nd->count = 0;
    for (int ii = 0; ii < cnt; ++ii)
    {
        int ni = q->indices[start + ii];
        RogueUISkillNodeRec* sn = &nodes[ni];
        for (int c = 0; c < 4; c++)
        {
            SkillQuadNode* ch = &q->nodes[nd->children[c]];
            if (sn->x >= ch->x && sn->x < ch->x + ch->w && sn->y >= ch->y && sn->y < ch->y + ch->h)
            {
                skillgraph_qt_push_index(q, ni);
                ch->count++;
                break;
            }
        }
    }
}
static int skillgraph_build_qt_recurse(SkillQuadTree* q, int node_index, RogueUISkillNodeRec* nodes)
{
    SkillQuadNode* nd = &q->nodes[node_index];
    if (nd->count > 8)
    {
        skillgraph_qt_subdivide(q, node_index, nodes);
        for (int i = 0; i < 4; i++)
        {
            int ci = nd->children[i];
            if (ci >= 0)
                skillgraph_build_qt_recurse(q, ci, nodes);
        }
    }
    return 1;
}
static void skillgraph_rebuild_quadtree(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    SkillQuadTree* q = (SkillQuadTree*) ctx->skillgraph_quadtree;
    if (!q)
    {
        ctx->skillgraph_quadtree = skillgraph_quadtree_create();
        q = (SkillQuadTree*) ctx->skillgraph_quadtree;
    }
    skillgraph_quadtree_reset(q);
    if (ctx->skillgraph_node_count == 0)
        return;
    float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
    for (int i = 0; i < ctx->skillgraph_node_count; i++)
    {
        RogueUISkillNodeRec* n = &ctx->skillgraph_nodes[i];
        if (n->x < minx)
            minx = n->x;
        if (n->y < miny)
            miny = n->y;
        if (n->x > maxx)
            maxx = n->x;
        if (n->y > maxy)
            maxy = n->y;
    }
    float w = maxx - minx;
    float h = maxy - miny;
    if (w < 1)
        w = 1;
    if (h < 1)
        h = 1;
    SkillQuadNode root;
    root.x = minx;
    root.y = miny;
    root.w = w;
    root.h = h;
    root.first_index = 0;
    root.count = 0;
    for (int k = 0; k < 4; k++)
        root.children[k] = -1;
    int root_index = skillgraph_qt_push_node(q, root);
    for (int i = 0; i < ctx->skillgraph_node_count; i++)
    {
        skillgraph_qt_push_index(q, i);
        q->nodes[root_index].count++;
    }
    skillgraph_build_qt_recurse(q, root_index, ctx->skillgraph_nodes);
}
static int skillgraph_frustum_contains(float vx, float vy, float vw, float vh, float x, float y)
{
    return x >= vx && y >= vy && x <= vx + vw && y <= vy + vh;
}
static void skillgraph_emit_node(RogueUIContext* ctx, RogueUISkillNodeRec* n)
{
    float sx = (n->x - ctx->skillgraph_view_x) * ctx->skillgraph_zoom;
    float sy = (n->y - ctx->skillgraph_view_y) * ctx->skillgraph_zoom;
    float base = 28.0f * ctx->skillgraph_zoom; /* slightly larger for icon padding */
    RogueUIRect r_icon = {sx - base * 0.5f, sy - base * 0.5f, base, base};
    /* Base background layer (darker) */
    uint32_t bg_col = 0x25252CFFu;
    rogue_ui_panel(ctx, r_icon, bg_col);
    /* Synergy glow underlay (expanded) */
    if (n->synergy)
    {
        rogue_ui_panel(ctx, (RogueUIRect){r_icon.x - 4, r_icon.y - 4, r_icon.w + 8, r_icon.h + 8},
                       0x30307040u);
    }
    /* Rank ring (outer thin border using panel as proxy) */
    rogue_ui_panel(ctx, (RogueUIRect){r_icon.x - 2, r_icon.y - 2, r_icon.w + 4, r_icon.h + 4},
                   n->synergy ? 0x5060C0A0u : 0x404040A0u);
    /* Icon sprite */
    rogue_ui_sprite(ctx, (RogueUIRect){r_icon.x + 2, r_icon.y + 2, r_icon.w - 4, r_icon.h - 4},
                    n->icon_id, 0, 0xFFFFFFFFu);
    /* Rank text */
    char txt[24];
    snprintf(txt, sizeof txt, "%d/%d", n->rank, n->max_rank);
    rogue_ui_text_dup(ctx, (RogueUIRect){r_icon.x, r_icon.y + r_icon.h + 2, r_icon.w, 12}, txt,
                      0xFFFFFFFFu);
    /* Pip bar */
    int pips = n->max_rank > 10 ? 10 : n->max_rank;
    float pipw = r_icon.w / (float) pips;
    float py = r_icon.y - 7;
    for (int i = 0; i < pips; i++)
    {
        float px = r_icon.x + i * pipw;
        uint32_t c_bg = 0x202020FFu;
        uint32_t c_fill = (i < n->rank) ? (n->synergy ? 0x90E0FFFFu : 0xA0D050FFu) : 0x404040FFu;
        rogue_ui_panel(ctx, (RogueUIRect){px, py, pipw - 1, 5}, c_bg);
        if (i < n->rank)
        {
            rogue_ui_panel(ctx, (RogueUIRect){px + 1, py + 1, pipw - 3, 3}, c_fill);
        }
    }
    /* Active pulse overlay */
    for (int i = 0; i < ctx->skillgraph_pulse_count; i++)
    {
        if (ctx->skillgraph_pulses[i].icon_id == n->icon_id)
        {
            float t = ctx->skillgraph_pulses[i].remaining_ms / 280.0f;
            if (t < 0)
                t = 0;
            if (t > 1)
                t = 1;
            float scale = 1.0f + (1.0f - t) * 0.35f;
            float w = r_icon.w * scale;
            float h = r_icon.h * scale;
            float cx = r_icon.x + r_icon.w * 0.5f;
            float cy = r_icon.y + r_icon.h * 0.5f;
            uint8_t alpha = (uint8_t) (180 * t);
            rogue_ui_panel(ctx, (RogueUIRect){cx - w * 0.5f, cy - h * 0.5f, w, h},
                           (uint32_t) (0x60A0F000u | alpha));
        }
    }
    /* Spend flyouts */
    for (int i = 0; i < ctx->skillgraph_spend_count; i++)
    {
        if (ctx->skillgraph_spends[i].icon_id == n->icon_id)
        {
            char amt[16];
            snprintf(amt, sizeof amt, "-%d", ctx->skillgraph_spends[i].amount);
            float t = ctx->skillgraph_spends[i].remaining_ms / 600.0f;
            if (t < 0)
                t = 0;
            if (t > 1)
                t = 1;
            float rise = (1.0f - t) * 24.0f;
            uint8_t alpha = (uint8_t) (255 * t);
            rogue_ui_text_dup(ctx, (RogueUIRect){r_icon.x, r_icon.y - 12 - rise, r_icon.w, 10}, amt,
                              (uint32_t) (0xFF5050u | (alpha << 24)));
        }
    }
}

void rogue_ui_skillgraph_begin(RogueUIContext* ctx, float view_x, float view_y, float view_w,
                               float view_h, float zoom)
{
    if (!ctx)
        return;
    ctx->skillgraph_active = 1;
    ctx->skillgraph_view_x = view_x;
    ctx->skillgraph_view_y = view_y;
    ctx->skillgraph_view_w = view_w;
    ctx->skillgraph_view_h = view_h;
    ctx->skillgraph_zoom = (zoom <= 0) ? 1.0f : zoom;
    ctx->skillgraph_node_count = 0;
}

void rogue_ui_skillgraph_add(RogueUIContext* ctx, float world_x, float world_y, int icon_id,
                             int rank, int max_rank, int synergy, unsigned int tags)
{
    if (!ctx || !ctx->skillgraph_active)
        return;
    if (ctx->skillgraph_node_count >= ctx->skillgraph_node_capacity)
    {
        int nc = ctx->skillgraph_node_capacity ? ctx->skillgraph_node_capacity * 2 : 64;
        ctx->skillgraph_nodes =
            (RogueUISkillNodeRec*) realloc(ctx->skillgraph_nodes, nc * sizeof(RogueUISkillNodeRec));
        ctx->skillgraph_node_capacity = nc;
    }
    RogueUISkillNodeRec* n = &ctx->skillgraph_nodes[ctx->skillgraph_node_count++];
    n->x = world_x;
    n->y = world_y;
    n->icon_id = icon_id;
    n->rank = rank;
    n->max_rank = max_rank;
    n->synergy = synergy;
    n->tags = tags;
}

static void skillgraph_query_emit(SkillQuadTree* q, int node_index, RogueUIContext* ctx)
{
    SkillQuadNode* nd = &q->nodes[node_index];
    float vx = ctx->skillgraph_view_x, vy = ctx->skillgraph_view_y, vw = ctx->skillgraph_view_w,
          vh = ctx->skillgraph_view_h;
    if (nd->x + nd->w < vx || nd->y + nd->h < vy || nd->x > vx + vw || nd->y > vy + vh)
        return;
    if (nd->children[0] < 0)
    {
        for (int i = 0; i < nd->count; i++)
        {
            int idx = q->indices[nd->first_index + i];
            RogueUISkillNodeRec* rec = &ctx->skillgraph_nodes[idx];
            if (ctx->skillgraph_filter_tags && !(rec->tags & ctx->skillgraph_filter_tags))
                continue;
            if (skillgraph_frustum_contains(vx, vy, vw, vh, rec->x, rec->y))
                skillgraph_emit_node(ctx, rec);
        }
    }
    else
    {
        for (int c = 0; c < 4; c++)
        {
            if (nd->children[c] >= 0)
                skillgraph_query_emit(q, nd->children[c], ctx);
        }
    }
}

int rogue_ui_skillgraph_build(RogueUIContext* ctx)
{
    if (!ctx || !ctx->skillgraph_active)
        return 0;
    skillgraph_rebuild_quadtree(ctx);
    SkillQuadTree* q = (SkillQuadTree*) ctx->skillgraph_quadtree;
    if (!q || q->node_count == 0)
    {
        ctx->skillgraph_active = 0;
        return 0;
    }
    int before = ctx->node_count;
    skillgraph_query_emit(q, 0, ctx);
    ctx->skillgraph_active = 0;
    return ctx->node_count - before;
}

void rogue_ui_skillgraph_pulse(RogueUIContext* ctx, int icon_id)
{
    if (!ctx)
        return;
    if (ctx->skillgraph_pulse_count <
        (int) (sizeof(ctx->skillgraph_pulses) / sizeof(ctx->skillgraph_pulses[0])))
    {
        ctx->skillgraph_pulses[ctx->skillgraph_pulse_count].icon_id = icon_id;
        ctx->skillgraph_pulses[ctx->skillgraph_pulse_count].remaining_ms = 280.0f;
        ctx->skillgraph_pulse_count++;
    }
}

void rogue_ui_skillgraph_spend_flyout(RogueUIContext* ctx, int icon_id, int amount)
{
    if (!ctx)
        return;
    if (ctx->skillgraph_spend_count <
        (int) (sizeof(ctx->skillgraph_spends) / sizeof(ctx->skillgraph_spends[0])))
    {
        ctx->skillgraph_spends[ctx->skillgraph_spend_count].icon_id = icon_id;
        ctx->skillgraph_spends[ctx->skillgraph_spend_count].remaining_ms = 600.0f;
        ctx->skillgraph_spends[ctx->skillgraph_spend_count].y_offset = 0.0f;
        ctx->skillgraph_spends[ctx->skillgraph_spend_count].amount = amount;
        ctx->skillgraph_spend_count++;
    }
}

void rogue_ui_skillgraph_enable_synergy_panel(RogueUIContext* ctx, int enable)
{
    if (!ctx)
        return;
    ctx->skillgraph_synergy_panel_enabled = enable ? 1 : 0;
}

void rogue_ui_skillgraph_set_filter_tags(RogueUIContext* ctx, unsigned int tag_mask)
{
    if (!ctx)
        return;
    ctx->skillgraph_filter_tags = tag_mask;
}

size_t rogue_ui_skillgraph_export(const RogueUIContext* ctx, char* buffer, size_t cap)
{
    if (!ctx || !buffer || cap == 0)
        return 0;
    size_t off = 0;
    for (int i = 0; i < ctx->skillgraph_node_count; i++)
    {
        RogueUISkillNodeRec* n = &ctx->skillgraph_nodes[i];
        char line[64];
        int len =
            snprintf(line, sizeof line, "%d:%d/%d;%u\n", n->icon_id, n->rank, n->max_rank, n->tags);
        if (off + (size_t) len >= cap)
            break;
        memcpy(buffer + off, line, (size_t) len);
        off += (size_t) len;
    }
    if (off < cap)
        buffer[off] = '\0';
    return off;
}

int rogue_ui_skillgraph_import(RogueUIContext* ctx, const char* buffer)
{
    if (!ctx || !buffer)
        return 0;
    int applied = 0;
    const char* p = buffer;
    while (*p)
    {
        /* Manual parse: icon:rank/max;tags */
        int icon = 0, rank = 0, maxr = 0;
        unsigned int tags = 0;
        const char* line = p;
        const char* nl = strchr(p, '\n');
        size_t len = nl ? (size_t) (nl - p) : strlen(p);
        const char* c = line; /* parse icon */
        while (*c >= '0' && *c <= '9')
        {
            icon = icon * 10 + (*c - '0');
            c++;
        }
        if (*c != ':')
        {
            goto next_line;
        }
        c++;
        while (*c >= '0' && *c <= '9')
        {
            rank = rank * 10 + (*c - '0');
            c++;
        }
        if (*c != '/')
        {
            goto next_line;
        }
        c++;
        while (*c >= '0' && *c <= '9')
        {
            maxr = maxr * 10 + (*c - '0');
            c++;
        }
        if (*c != ';')
        {
            goto next_line;
        }
        c++;
        while (*c >= '0' && *c <= '9')
        {
            tags = tags * 10 + (unsigned) (*c - '0');
            c++;
        }
        /* apply */
        for (int i = 0; i < ctx->skillgraph_node_count; i++)
        {
            if (ctx->skillgraph_nodes[i].icon_id == icon)
            {
                if (rank <= ctx->skillgraph_nodes[i].max_rank)
                {
                    ctx->skillgraph_nodes[i].rank = rank;
                    applied++;
                }
                ctx->skillgraph_nodes[i].tags = tags;
                break;
            }
        }
    next_line:
        p = nl ? nl + 1 : p + len;
        if (!nl)
            break;
    }
    return applied;
}

/* Helper to free the internal quadtree structure from external TUs without exposing its layout */
void rogue_ui_skillgraph_free_quadtree(void* qt)
{
    if (!qt)
        return;
    SkillQuadTree* q = (SkillQuadTree*) qt;
    free(q->nodes);
    free(q->indices);
    free(q);
}

int rogue_ui_skillgraph_allocate(RogueUIContext* ctx, int icon_id)
{
    if (!ctx)
        return 0;
    for (int i = 0; i < ctx->skillgraph_node_count; i++)
    {
        if (ctx->skillgraph_nodes[i].icon_id == icon_id)
        {
            if (ctx->skillgraph_nodes[i].rank < ctx->skillgraph_nodes[i].max_rank)
            {
                ctx->skillgraph_nodes[i].rank++;
                /* record undo */
                if (ctx->skillgraph_undo_count <
                    (int) (sizeof(ctx->skillgraph_undo) / sizeof(ctx->skillgraph_undo[0])))
                {
                    ctx->skillgraph_undo[ctx->skillgraph_undo_count].icon_id = icon_id;
                    ctx->skillgraph_undo[ctx->skillgraph_undo_count].prev_rank =
                        ctx->skillgraph_nodes[i].rank - 1;
                    ctx->skillgraph_undo_count++;
                }
                rogue_ui_skillgraph_pulse(ctx, icon_id);
                return 1;
            }
            return 0;
        }
    }
    return 0;
}

int rogue_ui_skillgraph_undo(RogueUIContext* ctx)
{
    if (!ctx || ctx->skillgraph_undo_count <= 0)
        return 0;
    int idx = --ctx->skillgraph_undo_count;
    int icon = ctx->skillgraph_undo[idx].icon_id;
    int prev = ctx->skillgraph_undo[idx].prev_rank;
    for (int i = 0; i < ctx->skillgraph_node_count; i++)
    {
        if (ctx->skillgraph_nodes[i].icon_id == icon)
        {
            if (prev >= 0 && prev <= ctx->skillgraph_nodes[i].max_rank)
            {
                ctx->skillgraph_nodes[i].rank = prev;
                return 1;
            }
            break;
        }
    }
    return 0;
}
