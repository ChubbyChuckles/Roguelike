#include "ui/core/ui_context.h"
#include "ui/core/ui_animation.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int xorshift32(unsigned int* s)
{
    unsigned int x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}
/* Forward decl for animation tick (Phase 8) */
static void ui_anim_step(double dt_ms);

/* ----------------- Phase 5 Skill Graph (zoomable, panning, quadtree culling) ------------------ */
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
    /* Icon sprite: encode skill index in sheet_id (data_i0) so renderer can map to loaded textures.
     */
    rogue_ui_sprite(ctx, (RogueUIRect){r_icon.x + 2, r_icon.y + 2, r_icon.w - 4, r_icon.h - 4},
                    n->icon_id, 0, 0xFFFFFFFFu);
    /* Rank text */
    char txt[24];
    snprintf(txt, sizeof txt, "%d/%d", n->rank, n->max_rank);
    rogue_ui_text_dup(ctx, (RogueUIRect){r_icon.x, r_icon.y + r_icon.h + 2, r_icon.w, 12}, txt,
                      0xFFFFFFFFu);
    /* Pip bar (styled: filled vs empty) */
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
    /* Active pulse overlay (fade & scale) */
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
    /* Spend flyouts (amount text rising) */
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
int rogue_ui_skillgraph_allocate(RogueUIContext* ctx, int icon_id)
{
    if (!ctx)
        return 0;
    for (int i = 0; i < ctx->skillgraph_node_count; i++)
    {
        RogueUISkillNodeRec* n = &ctx->skillgraph_nodes[i];
        if (n->icon_id == icon_id && n->rank < n->max_rank)
        {
            if (ctx->skillgraph_undo_count <
                (int) (sizeof(ctx->skillgraph_undo) / sizeof(ctx->skillgraph_undo[0])))
            {
                ctx->skillgraph_undo[ctx->skillgraph_undo_count].icon_id = icon_id;
                ctx->skillgraph_undo[ctx->skillgraph_undo_count].prev_rank = n->rank;
                ctx->skillgraph_undo_count++;
            }
            n->rank++;
            rogue_ui_skillgraph_pulse(ctx, icon_id);
            return 1;
        }
    }
    return 0;
}
int rogue_ui_skillgraph_undo(RogueUIContext* ctx)
{
    if (!ctx)
        return 0;
    if (ctx->skillgraph_undo_count <= 0)
        return 0;
    ctx->skillgraph_undo_count--;
    int icon = ctx->skillgraph_undo[ctx->skillgraph_undo_count].icon_id;
    int prev = ctx->skillgraph_undo[ctx->skillgraph_undo_count].prev_rank;
    for (int i = 0; i < ctx->skillgraph_node_count; i++)
    {
        RogueUISkillNodeRec* n = &ctx->skillgraph_nodes[i];
        if (n->icon_id == icon)
        {
            n->rank = prev;
            return 1;
        }
    }
    return 0;
}

int rogue_ui_init(RogueUIContext* ctx, const RogueUIContextConfig* cfg)
{
    fprintf(stderr, "INIT_ENTER raw ctx=%p cfg=%p\n", (void*) ctx, (void*) cfg);
    fflush(stderr);
    if (!ctx || !cfg)
    {
        fprintf(stderr, "INIT_FAIL null ctx or cfg\n");
        return 0;
    }
    fprintf(stderr,
            "INIT_START ctx=%p cfg.max_nodes=%d cfg.seed=%u cfg.arena_size=%zu sizeof(ctx)=%zu\n",
            (void*) ctx, cfg->max_nodes, cfg->seed, cfg->arena_size, sizeof *ctx);
    fflush(stderr);
    memset(ctx, 0, sizeof *ctx);
    /* Explicitly initialize interaction indices to -1 (not active) instead of 0.
        Leaving them zeroed after memset would treat index 0 as active/modal by default,
        which breaks navigation and gating semantics in tests. */
    ctx->hot_index = -1;
    ctx->active_index = -1;
    ctx->focus_index = -1;
    ctx->modal_index = -1;
    ctx->last_hover_index = -1;
    /* (Removed temporary static asserts to diagnose runtime corruption) */
    int cap = cfg->max_nodes > 0 ? cfg->max_nodes : 128;
    if (cap <= 0)
    {
        fprintf(stderr, "INIT_WARN computed cap<=0 from cfg.max_nodes=%d forcing 128\n",
                cfg->max_nodes);
        cap = 128;
    }
    size_t node_bytes = (size_t) cap * sizeof(RogueUINode);
    fprintf(stderr, "INIT_ALLOC nodes cap=%d bytes=%zu sizeof(node)=%zu\n", cap, node_bytes,
            sizeof(RogueUINode));
    fflush(stderr);
    ctx->nodes = (RogueUINode*) calloc((size_t) cap, sizeof(RogueUINode));
    ctx->stat_preview_slot = -1; /* Initialize stat_preview_slot */
    if (!ctx->nodes)
    {
        fprintf(stderr, "INIT_FAIL node alloc bytes=%zu\n", node_bytes);
        fflush(stderr);
        return 0;
    }
    ctx->node_capacity = cap;
    if (ctx->node_capacity == 0)
    {
        fprintf(stderr, "INIT_ERROR node_capacity ended 0 after alloc bytes=%zu\n", node_bytes);
    }
    fprintf(stderr, "INIT_NODES_OK capacity=%d nodes_ptr=%p\n", ctx->node_capacity,
            (void*) ctx->nodes);
    fflush(stderr);
    ctx->rng_state = cfg->seed ? cfg->seed : 0xC0FFEEu;
    ctx->theme.panel_bg_color = 0x202028FFu;
    ctx->theme.text_color = 0xFFFFFFFFu;
    size_t arena_size = cfg->arena_size ? cfg->arena_size : (size_t) (32 * 1024);
    fprintf(stderr, "INIT_ALLOC arena size=%zu\n", arena_size);
    fflush(stderr);
    ctx->arena = (unsigned char*) malloc(arena_size);
    if (!ctx->arena)
    {
        fprintf(stderr, "INIT_FAIL arena alloc size=%zu\n", arena_size);
        fflush(stderr);
        free(ctx->nodes);
        ctx->nodes = NULL;
        return 0;
    }
    memset(ctx->arena, 0, arena_size);
    ctx->arena_size = arena_size;
    fprintf(stderr, "INIT_ARENA_OK ptr=%p size=%zu\n", (void*) ctx->arena, ctx->arena_size);
    fflush(stderr);
    /* Default key repeat configuration */
    ctx->key_repeat_initial_ms = 400.0; /* typical desktop delay */
    ctx->key_repeat_interval_ms = 65.0; /* ~15 repeats/sec */
    ctx->chord_timeout_ms = 900.0;      /* generous default */
    /* Radial selector initial */
    ctx->radial.active = 0;
    ctx->radial.count = 0;
    ctx->radial.selection = 0;
    ctx->initialized_flag = 0xC0DEFACE; /* magic */
    fprintf(stderr, "INIT_SUCCESS ctx=%p magic=0x%X\n", (void*) ctx, ctx->initialized_flag);
    fflush(stderr);
    return 1;
}

void rogue_ui_shutdown(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    if (ctx->initialized_flag != 0xC0DEFACE)
    {
        return;
    } /* already shut down or never init */
    free(ctx->nodes);
    ctx->nodes = NULL;
    ctx->node_capacity = 0;
    ctx->node_count = 0;
    free(ctx->arena);
    ctx->arena = NULL;
    ctx->arena_size = ctx->arena_offset = 0;
    if (ctx->skillgraph_nodes)
    {
        free(ctx->skillgraph_nodes);
        ctx->skillgraph_nodes = NULL;
        ctx->skillgraph_node_count = ctx->skillgraph_node_capacity = 0;
    }
    if (ctx->skillgraph_quadtree)
    {
        SkillQuadTree* q = (SkillQuadTree*) ctx->skillgraph_quadtree;
        free(q->nodes);
        free(q->indices);
        free(q);
        ctx->skillgraph_quadtree = NULL;
    }
    ctx->initialized_flag = 0; /* mark shutdown */
}

static void ui_animation_master_step(double dt_ms); /* forward */
void rogue_ui_begin(RogueUIContext* ctx, double delta_time_ms)
{
    if (!ctx)
        return;
    if (ctx->initialized_flag != 0xC0DEFACE)
    {
        fprintf(stderr, "UI_BEGIN_ABORT not initialized ctx=%p flag=0x%X\n", (void*) ctx,
                ctx->initialized_flag);
        return;
    }
    fprintf(stderr, "UI_BEGIN enter ctx=%p dt=%.2f cap=%d nodes=%p arena=%p arena_size=%zu\n",
            (void*) ctx, delta_time_ms, ctx->node_capacity, (void*) ctx->nodes, (void*) ctx->arena,
            ctx->arena_size);
    /* If node_capacity==0 something went wrong with init; attempt lazy alloc minimal buffer to keep
     * tests alive. */
    if (ctx->node_capacity == 0)
    {
        fprintf(stderr, "UI_BEGIN alloc fallback nodes 64\n");
        ctx->nodes = (RogueUINode*) calloc(64, sizeof(RogueUINode));
        if (ctx->nodes)
        {
            ctx->node_capacity = 64;
        }
    }
    if (!ctx->nodes)
    {
        fprintf(stderr, "UI_BEGIN_FATAL nodes NULL after alloc attempt\n");
        return;
    }
    if (!ctx->arena)
    {
        fprintf(stderr, "UI_BEGIN_WARN arena NULL (continuing)\n");
    }
    if (ctx->anim_time_scale <= 0)
        ctx->anim_time_scale = 1.0f;
    double scaled_dt = delta_time_ms * (double) ctx->anim_time_scale;
    ctx->frame_dt_ms = delta_time_ms;
    ctx->time_ms += scaled_dt;
    ctx->node_count = 0;
    ctx->stats.draw_calls = 0;
    ctx->frame_active = 1;
    ctx->arena_offset = 0;
    ctx->hot_index = -1;
    ctx->dirty_reported_this_frame = 0;
    /* Advance skill graph animations */
    for (int i = 0; i < ctx->skillgraph_pulse_count;)
    {
        ctx->skillgraph_pulses[i].remaining_ms -= (float) delta_time_ms;
        if (ctx->skillgraph_pulses[i].remaining_ms <= 0)
        {
            ctx->skillgraph_pulses[i] = ctx->skillgraph_pulses[--ctx->skillgraph_pulse_count];
            continue;
        }
        i++;
    }
    for (int i = 0; i < ctx->skillgraph_spend_count;)
    {
        ctx->skillgraph_spends[i].remaining_ms -= (float) delta_time_ms;
        ctx->skillgraph_spends[i].y_offset += (float) delta_time_ms * 0.02f;
        if (ctx->skillgraph_spends[i].remaining_ms <= 0)
        {
            ctx->skillgraph_spends[i] = ctx->skillgraph_spends[--ctx->skillgraph_spend_count];
            continue;
        }
        i++;
    }
    /* Phase 8 animation tick (scaled) */
    ui_animation_master_step(scaled_dt);
    /* Perf timing begin (Phase 9) */
    ctx->perf_frame_start_ms = ctx->time_ms;
    ctx->perf_update_start_ms =
        ctx->time_ms; /* simplistic: update occurs inside begin for headless tests */
    fprintf(stderr, "UI_BEGIN exit ctx=%p node_cap=%d\n", (void*) ctx, ctx->node_capacity);
}

/* DEBUG TRACE */
static void dbg_trace(const char* tag)
{
    fprintf(stderr, "TRACE %s\n", tag);
    fflush(stderr);
}
void rogue_ui_end(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    ctx->frame_active = 0;
}
/* ensure report flag resets for next frame's diff comparison */

static int push_node(RogueUIContext* ctx, RogueUINode n)
{
    if (!ctx)
    {
        fprintf(stderr, "PUSH_NODE_FAIL null ctx\n");
        return -1;
    }
    if (!ctx->nodes)
    {
        fprintf(stderr, "PUSH_NODE_FAIL nodes NULL ctx=%p node_count=%d cap=%d\n", (void*) ctx,
                ctx->node_count, ctx->node_capacity);
        return -1;
    }
    if (ctx->node_capacity <= 0)
    {
        fprintf(stderr, "PUSH_NODE_FAIL capacity<=0 ctx=%p cap=%d\n", (void*) ctx,
                ctx->node_capacity);
        return -1;
    }
    if (ctx->node_count >= ctx->node_capacity)
    {
        fprintf(stderr, "PUSH_NODE_FAIL overflow ctx=%p count=%d cap=%d kind=%d\n", (void*) ctx,
                ctx->node_count, ctx->node_capacity, n.kind);
        return -1;
    }
    if (n.parent_index < -1)
        n.parent_index = -1;
    ctx->nodes[ctx->node_count] = n;
    ctx->node_count++;
    if (ctx->node_count <= 8)
    {
        fprintf(stderr, "PUSH_NODE ok idx=%d kind=%d count=%d cap=%d\\n", ctx->node_count - 1,
                n.kind, ctx->node_count, ctx->node_capacity);
    }
    ctx->stats.node_count = ctx->node_count;
    return ctx->node_count - 1;
}

/* Simple FNV1a 32-bit for IDs */
static uint32_t fnv1a32(const char* s)
{
    uint32_t h = 2166136261u;
    while (s && *s)
    {
        h ^= (unsigned char) (*s++);
        h *= 16777619u;
    }
    return h;
}
uint32_t rogue_ui_make_id(const char* label) { return fnv1a32(label ? label : "\0"); }
const RogueUINode* rogue_ui_find_by_id(const RogueUIContext* ctx, uint32_t id_hash)
{
    if (!ctx)
        return NULL;
    for (int i = 0; i < ctx->node_count; i++)
        if (ctx->nodes[i].id_hash == id_hash)
            return &ctx->nodes[i];
    return NULL;
}

static void assign_id(RogueUINode* n)
{
    if (n->text)
        n->id_hash = rogue_ui_make_id(n->text);
}
int rogue_ui_panel(RogueUIContext* ctx, RogueUIRect r, uint32_t color)
{
    if (!ctx)
    {
        fprintf(stderr, "PANEL_FAIL null ctx\n");
        return -1;
    }
    if (!ctx->frame_active)
    {
        fprintf(stderr, "PANEL_FAIL frame_inactive ctx=%p\n", (void*) ctx);
        return -1;
    }
    if (!ctx->nodes)
    {
        fprintf(stderr, "PANEL_FAIL nodes NULL before push ctx=%p cap=%d\n", (void*) ctx,
                ctx->node_capacity);
    }
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.color = color;
    n.kind = 0;
    assign_id(&n);
    return push_node(ctx, n);
}
int rogue_ui_text(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color)
{
    if (!ctx || !ctx->frame_active)
        return -1;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.text = text;
    n.color = color;
    n.kind = 1;
    assign_id(&n);
    return push_node(ctx, n);
}
int rogue_ui_image(RogueUIContext* ctx, RogueUIRect r, const char* path, uint32_t tint)
{
    if (!ctx || !ctx->frame_active)
        return -1;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.text = path;
    n.color = tint;
    n.kind = 2;
    assign_id(&n);
    return push_node(ctx, n);
}
int rogue_ui_sprite(RogueUIContext* ctx, RogueUIRect r, int sheet_id, int frame, uint32_t tint)
{
    if (!ctx || !ctx->frame_active)
        return -1;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.color = tint;
    n.data_i0 = sheet_id;
    n.data_i1 = frame;
    n.kind = 3;
    return push_node(ctx, n);
}
int rogue_ui_progress_bar(RogueUIContext* ctx, RogueUIRect r, float value, float max_value,
                          uint32_t bg_color, uint32_t fill_color, int orientation)
{
    if (!ctx || !ctx->frame_active)
        return -1;
    if (max_value <= 0)
        max_value = 1.0f;
    if (value < 0)
        value = 0;
    if (value > max_value)
        value = max_value;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.color = bg_color;
    n.aux_color = fill_color;
    n.value = value;
    n.value_max = max_value;
    n.data_i0 = orientation;
    n.kind = 4;
    return push_node(ctx, n);
}

/* ---- Interaction helpers ---- */
static int rect_contains(const RogueUIRect* r, float x, float y)
{
    return x >= r->x && y >= r->y && x <= r->x + r->w && y <= r->y + r->h;
}

void rogue_ui_set_input(RogueUIContext* ctx, const RogueUIInputState* in)
{
    if (!ctx || !in)
        return;
    ctx->input = *in;
    ctx->hot_index = -1;
    /* Record input for replay if enabled */
    if (ctx->replay_recording &&
        ctx->replay_count < (int) (sizeof(ctx->replay_buffer) / sizeof(ctx->replay_buffer[0])))
    {
        ctx->replay_buffer[ctx->replay_count++] = *in;
    }
    /* Chord prime evaluation */
    if (ctx->input.key_ctrl && ctx->input.key_char)
    {
        char c = ctx->input.key_char;
        if (ctx->pending_chord)
        {
            char first = ctx->pending_chord;
            for (int i = 0; i < ctx->chord_count; i++)
            {
                if (ctx->chord_commands[i].k1 == first && ctx->chord_commands[i].k2 == c)
                {
                    ctx->last_command_executed = ctx->chord_commands[i].command_id;
                    break;
                }
            }
            ctx->pending_chord = 0;
        }
        else
        {
            for (int i = 0; i < ctx->chord_count; i++)
            {
                if (ctx->chord_commands[i].k1 == c)
                {
                    ctx->pending_chord = c;
                    ctx->pending_chord_time_ms = ctx->time_ms;
                    break;
                }
            }
        }
    }
}
int rogue_ui_focused_index(const RogueUIContext* ctx) { return ctx ? ctx->focus_index : -1; }

/* ---------------- Phase 11.1 Style Guide Catalog ---------------- */
void rogue_ui_style_guide_build(RogueUIContext* ctx)
{
    if (!ctx || !ctx->frame_active)
        return; /* Emit a representative catalog of widgets using deterministic layout */
    float x = 10, y = 10;
    rogue_ui_text(ctx, (RogueUIRect){x, y, 160, 14}, "STYLE GUIDE", 0xFFFFFFFFu);
    y += 18;
    rogue_ui_panel(ctx, (RogueUIRect){x, y, 140, 28}, 0x303030FFu);
    y += 34;
    rogue_ui_button(ctx, (RogueUIRect){x, y, 100, 22}, "Button", 0x406090FFu, 0xFFFFFFFFu);
    y += 28;
    int tgl_state = 1;
    rogue_ui_toggle(ctx, (RogueUIRect){x, y, 100, 22}, "Toggle", &tgl_state, 0x505050FFu,
                    0x208020FFu, 0xFFFFFFFFu);
    y += 28;
    float slider_v = 0.5f;
    rogue_ui_slider(ctx, (RogueUIRect){x, y, 120, 16}, 0.0f, 1.0f, &slider_v, 0x202020FFu,
                    0x80C040FFu);
    y += 24;
    char buf[16] = "Txt";
    rogue_ui_text_input(ctx, (RogueUIRect){x, y, 120, 20}, buf, (int) sizeof buf, 0x202020FFu,
                        0xFFFFFFFFu);
    y += 26;
    rogue_ui_progress_bar(ctx, (RogueUIRect){x, y, 120, 10}, 66, 100, 0x202020FFu, 0x60A0F0FFu, 0);
    y += 18;
}

/* ---------------- Phase 11.2 Developer Inspector ---------------- */
void rogue_ui_inspector_enable(RogueUIContext* ctx, int enabled)
{
    if (ctx)
        ctx->inspector_enabled = enabled ? 1 : 0;
}
int rogue_ui_inspector_enabled(const RogueUIContext* ctx)
{
    return ctx ? ctx->inspector_enabled : 0;
}
void rogue_ui_inspector_select(RogueUIContext* ctx, int node_index)
{
    if (!ctx)
        return;
    if (node_index >= 0 && node_index < ctx->node_count)
        ctx->inspector_selected_index = node_index;
}
int rogue_ui_inspector_emit(RogueUIContext* ctx, uint32_t highlight_color)
{
    if (!ctx || !ctx->frame_active || !ctx->inspector_enabled)
        return -1;
    for (int i = 0; i < ctx->node_count; i++)
    {
        const RogueUINode* n = &ctx->nodes[i];
        if (n->kind >= 5 && n->kind <= 8)
        { /* focusable */
            RogueUIRect r = n->rect;
            r.x -= 2;
            r.y -= 2;
            r.w += 4;
            r.h += 4;
            rogue_ui_panel(ctx, r,
                           (i == ctx->inspector_selected_index) ? highlight_color : 0xFF00FF30u);
        }
    }
    return ctx->node_count - 1; /* last overlay index */
}
int rogue_ui_inspector_edit_color(RogueUIContext* ctx, int node_index, uint32_t new_color)
{
    if (!ctx)
        return 0;
    if (node_index < 0 || node_index >= ctx->node_count)
        return 0;
    ctx->nodes[node_index].color = new_color;
    return 1;
}

/* ---------------- Phase 11.3 Crash Snapshot ---------------- */
int rogue_ui_snapshot(const RogueUIContext* ctx, RogueUICrashSnapshot* out)
{
    if (!ctx || !out)
        return 0;
    out->node_count = ctx->node_count;
    out->tree_hash = ((RogueUIContext*) ctx)->last_serial_hash ? ctx->last_serial_hash : 0;
    out->input = ctx->input;
    return 1;
}

static int interactive_push(RogueUIContext* ctx, RogueUINode* node)
{
    int idx = push_node(ctx, *node);
    if (idx < 0)
        return idx;
    float mx = ctx->input.mouse_x, my = ctx->input.mouse_y;
    if (rect_contains(&node->rect, mx, my))
        ctx->hot_index = idx;
    return idx;
}

int rogue_ui_button(RogueUIContext* ctx, RogueUIRect r, const char* label, uint32_t bg_color,
                    uint32_t text_color)
{
    if (!ctx || !ctx->frame_active)
        return -1;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.text = label;
    n.color = bg_color;
    n.aux_color = text_color;
    n.kind = 5;
    assign_id(&n);
    int idx = interactive_push(ctx, &n);
    if (idx < 0)
        return -1;
    int clicked = 0;
    if (ctx->modal_index >= 0 && ctx->modal_index != idx)
        return idx; /* modal gating */
    if (ctx->hot_index == idx)
    {
        if (ctx->input.mouse_pressed)
        {
            ctx->active_index = idx;
        }
        if (ctx->input.mouse_released && ctx->active_index == idx)
        {
            clicked = 1;
            ctx->active_index = -1;
        }
    }
    if (idx >= 0 && clicked)
        ctx->nodes[idx].value = 1.0f;
    return idx;
}

int rogue_ui_toggle(RogueUIContext* ctx, RogueUIRect r, const char* label, int* state,
                    uint32_t off_color, uint32_t on_color, uint32_t text_color)
{
    if (!ctx || !ctx->frame_active || !state)
        return -1;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.text = label;
    n.color = *state ? on_color : off_color;
    n.aux_color = text_color;
    n.kind = 6;
    assign_id(&n);
    int idx = interactive_push(ctx, &n);
    if (idx < 0)
        return -1;
    if (ctx->modal_index >= 0 && ctx->modal_index != idx)
        return idx;
    /* Press within toggle sets active; release within toggle flips state.
       Accept release if either this control is still active OR it's currently hovered,
       as long as the cursor is within the same rect. This is robust to transient hot_index
       changes across frames. */
    if (ctx->hot_index == idx && ctx->input.mouse_pressed)
    {
        fprintf(stderr, "TOGGLE press idx=%d hot=%d act=%d mx=%.1f my=%.1f\n", idx, ctx->hot_index,
                ctx->active_index, ctx->input.mouse_x, ctx->input.mouse_y);
        ctx->active_index = idx;
    }
    if (ctx->input.mouse_released && (ctx->active_index == idx || ctx->hot_index == idx))
    {
        /* accept release only if cursor is still within the same rect */
        float mx = ctx->input.mouse_x, my = ctx->input.mouse_y;
        int inside = rect_contains(&n.rect, mx, my);
        fprintf(stderr,
                "TOGGLE release idx=%d hot=%d act=%d inside=%d state_before=%d mx=%.1f my=%.1f\n",
                idx, ctx->hot_index, ctx->active_index, inside, *state, mx, my);
        if (inside)
        {
            *state = !*state;
            ctx->nodes[idx].color = *state ? on_color : off_color;
            fprintf(stderr, "TOGGLE flipped idx=%d new_state=%d\n", idx, *state);
        }
        if (ctx->active_index == idx)
            ctx->active_index = -1;
    }
    ctx->nodes[idx].value = (float) (*state);
    return idx;
}

int rogue_ui_slider(RogueUIContext* ctx, RogueUIRect r, float min_v, float max_v, float* value,
                    uint32_t track_color, uint32_t fill_color)
{
    if (!ctx || !ctx->frame_active || !value)
        return -1;
    if (max_v == min_v)
    {
        max_v = min_v + 1.0f;
    }
    if (*value < min_v)
        *value = min_v;
    if (*value > max_v)
        *value = max_v;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.color = track_color;
    n.aux_color = fill_color;
    n.kind = 7;
    n.value = *value;
    n.value_max = max_v;
    assign_id(&n);
    int idx = interactive_push(ctx, &n);
    if (idx < 0)
        return -1;
    if (ctx->modal_index >= 0 && ctx->modal_index != idx)
        return idx;
    if (ctx->hot_index == idx)
    {
        if (ctx->input.mouse_pressed)
        {
            ctx->active_index = idx;
        }
        if (ctx->active_index == idx && ctx->input.mouse_down)
        {
            float t = (ctx->input.mouse_x - r.x) / r.w;
            if (t < 0)
                t = 0;
            if (t > 1)
                t = 1;
            *value = min_v + t * (max_v - min_v);
            ctx->nodes[idx].value = *value;
        }
        if (ctx->input.mouse_released && ctx->active_index == idx)
        {
            ctx->active_index = -1;
        }
    }
    return idx;
}

int rogue_ui_text_input(RogueUIContext* ctx, RogueUIRect r, char* buffer, int buffer_cap,
                        uint32_t bg_color, uint32_t text_color)
{
    if (!ctx || !ctx->frame_active || !buffer || buffer_cap <= 0)
        return -1;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.text = buffer;
    n.color = bg_color;
    n.aux_color = text_color;
    n.kind = 8;
    assign_id(&n);
    int idx = interactive_push(ctx, &n);
    if (idx < 0)
        return -1;
    int hovered = (ctx->hot_index == idx);
    if (hovered && ctx->input.mouse_pressed)
    {
        ctx->focus_index = idx;
    }
    if (ctx->modal_index >= 0 && ctx->modal_index != idx)
        return idx;
    if (ctx->focus_index == idx)
    {
        if (ctx->input.key_paste)
        {
            const char* clip = rogue_ui_clipboard_get();
            if (clip)
            {
                int len = (int) strlen(buffer);
                for (int i = 0; clip[i] && len < buffer_cap - 1; ++i)
                {
                    buffer[len++] = clip[i];
                }
                buffer[len] = '\0';
            }
        }
        if (ctx->input.text_char)
        {
            int len = (int) strlen(buffer);
            if (len < buffer_cap - 1)
            {
                buffer[len] = (char) ctx->input.text_char;
                buffer[len + 1] = '\0';
            }
        }
        if (ctx->input.backspace)
        {
            int len = (int) strlen(buffer);
            if (len > 0)
            {
                buffer[len - 1] = '\0';
            }
        }
        if (ctx->input.key_tab)
        {
            ctx->focus_index = (idx + 1 < ctx->node_count) ? idx + 1 : 0;
        }
    }
    return idx;
}

/* Layout containers */
int rogue_ui_row_begin(RogueUIContext* ctx, RogueUIRect r, int padding, int spacing)
{
    if (!ctx || !ctx->frame_active)
        return -1;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.kind = 0;
    n.data_i0 = padding;
    n.data_i1 = spacing;
    n.text = "__row";
    assign_id(&n);
    int idx = push_node(ctx, n);
    return idx;
}
int rogue_ui_column_begin(RogueUIContext* ctx, RogueUIRect r, int padding, int spacing)
{
    if (!ctx || !ctx->frame_active)
        return -1;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.kind = 0;
    n.data_i0 = padding;
    n.data_i1 = spacing;
    n.text = "__col";
    assign_id(&n);
    int idx = push_node(ctx, n);
    return idx;
}
int rogue_ui_row_next(RogueUIContext* ctx, int row_index, float width, float height,
                      RogueUIRect* out_rect)
{
    if (!ctx || row_index < 0 || row_index >= ctx->node_count)
        return 0;
    RogueUINode* row = &ctx->nodes[row_index];
    float cursor = row->value;
    float padding = (float) row->data_i0;
    float spacing = (float) row->data_i1;
    if (cursor == 0)
        cursor = padding;
    RogueUIRect rr = row->rect;
    RogueUIRect child = {rr.x + cursor, rr.y + padding, width, height};
    cursor += width + spacing;
    row->value = cursor;
    if (out_rect)
        *out_rect = child;
    return 1;
}
int rogue_ui_column_next(RogueUIContext* ctx, int col_index, float width, float height,
                         RogueUIRect* out_rect)
{
    if (!ctx || col_index < 0 || col_index >= ctx->node_count)
        return 0;
    RogueUINode* col = &ctx->nodes[col_index];
    float cursor = col->value;
    float padding = (float) col->data_i0;
    float spacing = (float) col->data_i1;
    if (cursor == 0)
        cursor = padding;
    RogueUIRect cr = col->rect;
    RogueUIRect child = {cr.x + padding, cr.y + cursor, width, height};
    cursor += height + spacing;
    col->value = cursor;
    if (out_rect)
        *out_rect = child;
    return 1;
}
RogueUIRect rogue_ui_grid_cell(RogueUIRect grid_rect, int rows, int cols, int r, int c, int padding,
                               int spacing)
{
    RogueUIRect cell = {0, 0, 0, 0};
    if (rows <= 0 || cols <= 0)
        return cell;
    float fpad = (float) padding;
    float fsp = (float) spacing;
    float total_spacing_x = fsp * (cols - 1) + fpad * 2.0f;
    float total_spacing_y = fsp * (rows - 1) + fpad * 2.0f;
    float cw = (grid_rect.w - total_spacing_x) / (float) cols;
    float ch = (grid_rect.h - total_spacing_y) / (float) rows;
    cell.x = grid_rect.x + fpad + (float) c * (cw + fsp);
    cell.y = grid_rect.y + fpad + (float) r * (ch + fsp);
    cell.w = cw;
    cell.h = ch;
    return cell;
}
int rogue_ui_layer(RogueUIContext* ctx, RogueUIRect r, int layer_order)
{
    if (!ctx || !ctx->frame_active)
        return -1;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.kind = 0;
    n.data_i0 = layer_order;
    n.text = "__layer";
    assign_id(&n);
    return push_node(ctx, n);
}

/* Scroll Container Implementation (Phase 2.4) */
int rogue_ui_scroll_begin(RogueUIContext* ctx, RogueUIRect r, float content_height)
{
    if (!ctx || !ctx->frame_active)
        return -1;
    if (content_height < r.h)
        content_height = r.h;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.kind = 0;
    n.text = "__scroll";
    n.value = 0.0f; /* scroll offset */
    n.value_max = content_height;
    assign_id(&n);
    int idx = push_node(ctx, n);
    if (idx < 0)
        return -1; /* Apply wheel */
    if (ctx->input.wheel_delta != 0)
    {
        float delta = -ctx->input.wheel_delta * 24.0f;
        float off = ctx->nodes[idx].value + delta;
        float max_off = content_height - r.h;
        if (max_off < 0)
            max_off = 0;
        if (off < 0)
            off = 0;
        if (off > max_off)
            off = max_off;
        ctx->nodes[idx].value = off;
    }
    return idx;
}
void rogue_ui_scroll_set_content(int scroll_index, RogueUIContext* ctx, float content_height)
{
    if (!ctx || scroll_index < 0 || scroll_index >= ctx->node_count)
        return;
    if (content_height < ctx->nodes[scroll_index].rect.h)
        content_height = ctx->nodes[scroll_index].rect.h;
    ctx->nodes[scroll_index].value_max = content_height;
    float max_off = content_height - ctx->nodes[scroll_index].rect.h;
    if (max_off < 0)
        max_off = 0;
    if (ctx->nodes[scroll_index].value > max_off)
        ctx->nodes[scroll_index].value = max_off;
}
float rogue_ui_scroll_offset(const RogueUIContext* ctx, int scroll_index)
{
    if (!ctx || scroll_index < 0 || scroll_index >= ctx->node_count)
        return 0.0f;
    return ctx->nodes[scroll_index].value;
}
RogueUIRect rogue_ui_scroll_apply(const RogueUIContext* ctx, int scroll_index,
                                  RogueUIRect child_raw)
{
    RogueUIRect r = child_raw;
    if (!ctx || scroll_index < 0 || scroll_index >= ctx->node_count)
        return r;
    r.y -= ctx->nodes[scroll_index].value;
    return r;
}

/* Tooltip (Phase 2.5) */
int rogue_ui_tooltip(RogueUIContext* ctx, int target_index, const char* text, uint32_t bg_color,
                     uint32_t text_color, int delay_ms)
{
    if (!ctx || !ctx->frame_active || !text || target_index < 0 || target_index >= ctx->node_count)
        return -1;
    if (ctx->hot_index == target_index)
    {
        if (ctx->last_hover_index != target_index)
        {
            ctx->last_hover_index = target_index;
            ctx->last_hover_start_ms = ctx->time_ms;
        }
        if ((ctx->time_ms - ctx->last_hover_start_ms) >= (double) delay_ms)
        {
            RogueUIRect tr = ctx->nodes[target_index].rect;
            RogueUIRect tip = {tr.x + tr.w + 6.0f, tr.y, 160.0f, 24.0f};
            int panel = rogue_ui_panel(ctx, tip, bg_color);
            if (panel >= 0)
            {
                RogueUIRect text_r = tip;
                rogue_ui_text(ctx, text_r, text, text_color);
            }
            return panel;
        }
    }
    else
    {
        if (ctx->last_hover_index == target_index)
        {
            ctx->last_hover_index = -1;
        }
    }
    return -1;
}

/* Navigation (Phase 2.8) advanced directional heuristics */
static int nav_key_down(const RogueUIInputState* in, int key_index)
{
    switch (key_index)
    {
    case 0:
        return in->key_left;
    case 1:
        return in->key_right;
    case 2:
        return in->key_up;
    case 3:
        return in->key_down;
    case 4:
        return in->key_tab;
    case 5:
        return in->key_activate;
    default:
        return 0;
    }
}

static void rogue_apply_repeat(RogueUIContext* c, int idx, int* move_h, int* move_v,
                               int* activate_ptr)
{
    if (c->key_repeat_state[idx])
    {
        double acc = c->key_repeat_accum[idx];
        if (acc >= c->key_repeat_initial_ms)
        {
            double over = acc - c->key_repeat_initial_ms;
            int pulses = (int) (over / c->key_repeat_interval_ms);
            if (pulses > 0)
            {
                c->key_repeat_accum[idx] =
                    c->key_repeat_initial_ms + over - pulses * c->key_repeat_interval_ms;
                if (idx == 0)
                    *move_h = -1;
                else if (idx == 1)
                    *move_h = 1;
                else if (idx == 2)
                    *move_v = -1;
                else if (idx == 3)
                    *move_v = 1;
                else if (idx == 4)
                    *move_h = 1;
                else if (idx == 5)
                    *activate_ptr = 1;
            }
        }
    }
}

void rogue_ui_navigation_update(RogueUIContext* ctx)
{
    if (!ctx || !ctx->frame_active)
        return;
    /* Phase 3.1 input replay injection */
    if (ctx->replay_playing)
    {
        if (!rogue_ui_replay_step(ctx))
        { /* finished */
        }
    }
    /* Phase 3.4 key repeat update (simplified across arrow + tab + activate) */
    for (int i = 0; i < 6; i++)
    {
        if (nav_key_down(&ctx->input, i))
        {
            if (ctx->key_repeat_state[i] == 0)
            {
                ctx->key_repeat_state[i] = 1;
                ctx->key_repeat_accum[i] = 0.0;
            }
        }
        else
        {
            ctx->key_repeat_state[i] = 0;
        }
    }
    int focusable_count = 0;
    for (int i = 0; i < ctx->node_count; i++)
    {
        int k = ctx->nodes[i].kind;
        if (k >= 5 && k <= 8)
            focusable_count++;
    }
    if (!focusable_count)
        return;
    /* Controller axis mapping (3.2) */
    int axis_move_h = 0, axis_move_v = 0;
    float threshold = 0.55f;
    if (ctx->controller.axis_x > threshold)
        axis_move_h = 1;
    else if (ctx->controller.axis_x < -threshold)
        axis_move_h = -1;
    if (ctx->controller.axis_y > threshold)
        axis_move_v = 1;
    else if (ctx->controller.axis_y < -threshold)
        axis_move_v = -1;
    /* Axis repeat gating using key_repeat slots 6,7 */
    if (axis_move_h)
    {
        if (ctx->key_repeat_state[6] == 0)
        {
            ctx->key_repeat_state[6] = 1;
            ctx->key_repeat_accum[6] = 0.0;
        }
    }
    else
        ctx->key_repeat_state[6] = 0;
    if (axis_move_v)
    {
        if (ctx->key_repeat_state[7] == 0)
        {
            ctx->key_repeat_state[7] = 1;
            ctx->key_repeat_accum[7] = 0.0;
        }
    }
    else
        ctx->key_repeat_state[7] = 0;
    int move_h = 0, move_v = 0, activate = 0;
    /* Base key edge triggers */
    if (ctx->input.key_left)
        move_h = -1;
    else if (ctx->input.key_right)
        move_h = 1;
    if (ctx->input.key_up)
        move_v = -1;
    else if (ctx->input.key_down)
        move_v = 1;
    if (ctx->input.key_tab)
        move_h = 1;
    if (ctx->input.key_activate)
        activate = 1;
    if (!move_h && axis_move_h)
        move_h = axis_move_h;
    if (!move_v && axis_move_v)
        move_v = axis_move_v;
    /* Key repeat pulses */
    for (int i = 0; i < 6; i++)
        if (ctx->key_repeat_state[i])
            ctx->key_repeat_accum[i] += ctx->frame_dt_ms;
    for (int i = 6; i < 8; i++)
        if (ctx->key_repeat_state[i])
            ctx->key_repeat_accum[i] += ctx->frame_dt_ms;
    /* helper inline for repeat application */
    rogue_apply_repeat(ctx, 0, &move_h, &move_v, &activate);
    rogue_apply_repeat(ctx, 1, &move_h, &move_v, &activate);
    rogue_apply_repeat(ctx, 2, &move_h, &move_v, &activate);
    rogue_apply_repeat(ctx, 3, &move_h, &move_v, &activate);
    rogue_apply_repeat(ctx, 4, &move_h, &move_v, &activate);
    rogue_apply_repeat(ctx, 5, &move_h, &move_v, &activate);
    if (axis_move_h)
        rogue_apply_repeat(ctx, axis_move_h > 0 ? 1 : 0, &move_h, &move_v, &activate);
    if (axis_move_v)
        rogue_apply_repeat(ctx, axis_move_v > 0 ? 3 : 2, &move_h, &move_v, &activate);
    if (ctx->focus_index < 0 || ctx->focus_index >= ctx->node_count)
    { /* choose first focusable */
        for (int i = 0; i < ctx->node_count; i++)
        {
            int k = ctx->nodes[i].kind;
            if (k >= 5 && k <= 8)
            {
                ctx->focus_index = i;
                break;
            }
        }
    }
    /* Modal focus enforcement */
    if (ctx->modal_index >= 0)
        ctx->focus_index = ctx->modal_index;
    if (ctx->focus_index < 0)
        return;

    /* Tab semantics: always advance to the next focusable once per call and return */
    if (ctx->input.key_tab && ctx->node_count > 1)
    {
        int prev = ctx->focus_index;
        int start = ctx->focus_index < 0 ? 0 : ctx->focus_index;
        int curi = start;
        for (int tries = 0; tries < ctx->node_count; ++tries)
        {
            curi = (curi + 1 < ctx->node_count) ? (curi + 1) : 0;
            int k = ctx->nodes[curi].kind;
            if (k >= 5 && k <= 8)
            {
                ctx->focus_index = curi;
                break;
            }
        }
        fprintf(stderr, "NAV_TAB advance prev=%d now=%d node_count=%d\n", prev, ctx->focus_index,
                ctx->node_count);
        /* Chord timeout maintenance */
        if (ctx->pending_chord &&
            (ctx->time_ms - ctx->pending_chord_time_ms) > ctx->chord_timeout_ms)
        {
            ctx->pending_chord = 0;
        }
        return;
    }
    if (activate)
    {
        RogueUINode* cur = &ctx->nodes[ctx->focus_index];
        if (cur->kind == 5)
        {
            cur->value = 1.0f;
        }
        else if (cur->kind == 6)
        {
            cur->value = (cur->value == 0.0f) ? 1.0f : 0.0f;
        }
    }
    if (!move_h && !move_v)
        return;
    RogueUINode* cur = &ctx->nodes[ctx->focus_index];
    float cx = cur->rect.x + cur->rect.w * 0.5f;
    float cy = cur->rect.y + cur->rect.h * 0.5f;
    int best = -1;
    float best_score = 1e9f;
    for (int i = 0; i < ctx->node_count; i++)
    {
        if (i == ctx->focus_index)
            continue;
        int k = ctx->nodes[i].kind;
        if (k < 5 || k > 8)
            continue;
        if (ctx->modal_index >= 0 && i != ctx->modal_index)
            continue;
        RogueUINode* n = &ctx->nodes[i];
        float nx = n->rect.x + n->rect.w * 0.5f;
        float ny = n->rect.y + n->rect.h * 0.5f;
        float dx = nx - cx;
        float dy = ny - cy;
        if (move_h)
        {
            if (move_h < 0 && dx >= -1e-3f)
                continue;
            if (move_h > 0 && dx <= 1e-3f)
                continue;
        }
        if (move_v)
        {
            if (move_v < 0 && dy >= -1e-3f)
                continue;
            if (move_v > 0 && dy <= 1e-3f)
                continue;
        }
        float primary = move_h ? fabsf(dx) : fabsf(dy);
        float secondary = move_h ? fabsf(dy) : fabsf(dx);
        if (secondary > primary * 2.5f)
            continue;
        float dist = (float) sqrt(dx * dx + dy * dy);
        float score = dist + secondary * 0.25f + primary * 0.1f;
        if (score < best_score)
        {
            best_score = score;
            best = i;
        }
    }
    if (best >= 0)
    {
        ctx->focus_index = best;
        return;
    }
    /* fallback linear wrap */
    int dir = (move_h > 0 || move_v > 0) ? 1 : -1;
    int start = ctx->focus_index;
    int curi = start;
    for (;;)
    {
        curi += dir;
        if (curi >= ctx->node_count)
            curi = 0;
        if (curi < 0)
            curi = ctx->node_count - 1;
        if (curi == start)
            break;
        int k = ctx->nodes[curi].kind;
        if (k >= 5 && k <= 8)
        {
            ctx->focus_index = curi;
            break;
        }
    }
    /* Chord timeout maintenance */
    if (ctx->pending_chord && (ctx->time_ms - ctx->pending_chord_time_ms) > ctx->chord_timeout_ms)
    {
        ctx->pending_chord = 0;
    }
}

/* Phase 3 scaffolding implementations (stubs / storage already added in context) */
void rogue_ui_set_modal(RogueUIContext* ctx, int modal_index)
{
    if (!ctx)
        return;
    ctx->modal_index = modal_index;
}
void rogue_ui_set_controller(RogueUIContext* ctx, const RogueUIControllerState* st)
{
    if (!ctx || !st)
        return;
    ctx->controller = *st;
}
static char g_clipboard[256];
void rogue_ui_clipboard_set(const char* text)
{
    if (!text)
        text = "";
    size_t i = 0;
    for (; i < sizeof(g_clipboard) - 1 && text[i]; ++i)
        g_clipboard[i] = text[i];
    g_clipboard[i] = '\0';
}
const char* rogue_ui_clipboard_get(void) { return g_clipboard; }
void rogue_ui_ime_start(void) {}
void rogue_ui_ime_cancel(void) {}
void rogue_ui_ime_commit(RogueUIContext* ctx, const char* text)
{
    (void) ctx;
    (void) text;
}
void rogue_ui_key_repeat_config(RogueUIContext* ctx, double initial_delay_ms, double interval_ms)
{
    if (!ctx)
        return;
    ctx->key_repeat_initial_ms = initial_delay_ms;
    ctx->key_repeat_interval_ms = interval_ms;
}
int rogue_ui_register_chord(RogueUIContext* ctx, char k1, char k2, int command_id)
{
    if (!ctx)
        return 0;
    if (ctx->chord_count >= 8)
        return 0;
    ctx->chord_commands[ctx->chord_count].k1 = k1;
    ctx->chord_commands[ctx->chord_count].k2 = k2;
    ctx->chord_commands[ctx->chord_count].command_id = command_id;
    ctx->chord_count++;
    return 1;
}
int rogue_ui_last_command(const RogueUIContext* ctx)
{
    return ctx ? ctx->last_command_executed : 0;
}
void rogue_ui_replay_start_record(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    ctx->replay_recording = 1;
    ctx->replay_playing = 0;
    ctx->replay_count = 0;
}
void rogue_ui_replay_stop_record(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    ctx->replay_recording = 0;
}
void rogue_ui_replay_start_playback(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    ctx->replay_playing = 1;
    ctx->replay_recording = 0;
    ctx->replay_cursor = 0;
}
int rogue_ui_replay_step(RogueUIContext* ctx)
{
    if (!ctx)
        return 0;
    if (!ctx->replay_playing)
        return 0;
    if (ctx->replay_cursor >= ctx->replay_count)
    {
        ctx->replay_playing = 0;
        return 0;
    }
    ctx->input = ctx->replay_buffer[ctx->replay_cursor++];
    return 1;
}

/* ---------------- Phase 8 Animation System ---------------- */
typedef struct UIAnimEntry
{
    uint32_t id;
    float t;
    float duration;
    int kind; /*0=entrance 1=exit 2=pulse*/
    RogueUIEaseType ease;
    float extra;
} UIAnimEntry;
typedef struct UIAnimExitDone
{
    uint32_t id;
    int ttl;
} UIAnimExitDone;
static UIAnimEntry g_ui_anims[128];
static int g_ui_anim_count = 0;
static UIAnimExitDone g_ui_exit_done[64];
static int g_ui_exit_done_count = 0;
/* Phase 8.2 Timeline / Keyframe System */
typedef struct UITimelineEntry
{
    uint32_t id;
    float t;
    float duration;
    int keyframe_count;
    RogueUITimelineKeyframe kf[6];
    int active;
} UITimelineEntry; /* active=1 while playing; becomes 0 when finished but entry lingers one extra
                      step for final sample */
static UITimelineEntry g_ui_timelines[64];
static int g_ui_timeline_count = 0;
static UITimelineEntry* ui_tl_find(uint32_t id)
{
    for (int i = 0; i < g_ui_timeline_count; i++)
        if (g_ui_timelines[i].id == id)
            return &g_ui_timelines[i];
    return NULL;
}
static UITimelineEntry* ui_tl_alloc(uint32_t id)
{
    UITimelineEntry* e = ui_tl_find(id);
    if (e)
        return e;
    if (g_ui_timeline_count >= (int) (sizeof(g_ui_timelines) / sizeof(g_ui_timelines[0])))
        return NULL;
    e = &g_ui_timelines[g_ui_timeline_count++];
    memset(e, 0, sizeof *e);
    e->id = id;
    e->active = 1;
    return e;
}
void rogue_ui_timeline_play(RogueUIContext* ctx, uint32_t id_hash,
                            const RogueUITimelineKeyframe* kfs, int count, float duration_ms,
                            RogueUITimelinePolicy policy)
{
    (void) ctx;
    if (!kfs || count < 2)
        return;
    if (duration_ms <= 0)
        duration_ms = 1.0f;
    if (count > 6)
        count = 6;
    if (policy == ROGUE_UI_TIMELINE_REPLACE)
    {
        UITimelineEntry* ex = ui_tl_find(id_hash);
        if (ex)
        {
            *ex = (UITimelineEntry){0};
            ex->id = id_hash;
            ex->active = 1;
        }
    }
    if (policy == ROGUE_UI_TIMELINE_IGNORE && ui_tl_find(id_hash))
        return;
    UITimelineEntry* e = ui_tl_alloc(id_hash);
    if (!e)
        return;
    e->t = 0;
    e->duration = duration_ms;
    e->keyframe_count = count;
    for (int i = 0; i < count; i++)
    {
        e->kf[i] = kfs[i];
        if (e->kf[i].at < 0)
            e->kf[i].at = 0;
        if (e->kf[i].at > 1)
            e->kf[i].at = 1;
    }
    e->active = 1;
}
static void ui_timeline_step(double dt_ms)
{
    for (int i = 0; i < g_ui_timeline_count;)
    {
        UITimelineEntry* e = &g_ui_timelines[i];
        if (!e->active && e->t > e->duration)
        {
            g_ui_timelines[i] = g_ui_timelines[--g_ui_timeline_count];
            continue;
        }
        e->t += (float) dt_ms;
        if (e->active && e->t >= e->duration)
        {
            e->t = e->duration;
            e->active =
                0; /* linger until next step so sampling after completion returns final value */
        }
        i++;
    }
}
static float ui_timeline_sample(uint32_t id, int mode_scale, int* active_out)
{
    UITimelineEntry* e = ui_tl_find(id);
    if (active_out)
        *active_out = (e && e->active) ? 1 : 0;
    if (!e)
        return 1.0f;
    float norm = e->duration > 0 ? e->t / e->duration : 1.0f;
    if (norm < 0)
        norm = 0;
    if (norm > 1)
        norm = 1;
    RogueUITimelineKeyframe a = e->kf[0];
    RogueUITimelineKeyframe b = e->kf[e->keyframe_count - 1];
    for (int i = 1; i < e->keyframe_count; i++)
    {
        if (norm <= e->kf[i].at)
        {
            a = e->kf[i - 1];
            b = e->kf[i];
            break;
        }
    }
    float span = b.at - a.at;
    float lt = span > 1e-6f ? (norm - a.at) / span : 1.0f;
    float eased = rogue_ui_ease(b.ease, lt);
    float scale = a.scale + (b.scale - a.scale) * eased;
    float alpha = a.alpha + (b.alpha - a.alpha) * eased;
    return mode_scale ? scale : alpha;
}
float rogue_ui_timeline_scale(const RogueUIContext* ctx, uint32_t id_hash, int* active_out)
{
    (void) ctx;
    return ui_timeline_sample(id_hash, 1, active_out);
}
float rogue_ui_timeline_alpha(const RogueUIContext* ctx, uint32_t id_hash, int* active_out)
{
    (void) ctx;
    return ui_timeline_sample(id_hash, 0, active_out);
}
static UIAnimEntry* ui_anim_find(uint32_t id)
{
    for (int i = 0; i < g_ui_anim_count; i++)
        if (g_ui_anims[i].id == id)
            return &g_ui_anims[i];
    return NULL;
}
static UIAnimEntry* ui_anim_alloc(uint32_t id)
{
    UIAnimEntry* e = ui_anim_find(id);
    if (e)
        return e;
    if (g_ui_anim_count >= (int) (sizeof(g_ui_anims) / sizeof(g_ui_anims[0])))
        return NULL;
    e = &g_ui_anims[g_ui_anim_count++];
    memset(e, 0, sizeof *e);
    e->id = id;
    return e;
}

void rogue_ui_set_time_scale(RogueUIContext* ctx, float scale)
{
    if (!ctx)
        return;
    ctx->anim_time_scale = scale;
}
static float ease_cubic_in(float x) { return x * x * x; }
static float ease_cubic_out(float x)
{
    float inv = 1.0f - x;
    return 1.0f - inv * inv * inv;
}
static float ease_cubic_in_out(float x)
{
    if (x < 0.5f)
    {
        return 4.0f * x * x * x;
    }
    float f = (2.0f * x - 2.0f);
    return 0.5f * f * f * f + 1.0f;
}
static float ease_spring(float x)
{
    float d = 1.0f - x;
    return 1.0f - (d * d * (1.0f + 2.2f * d));
}
static float ease_elastic_out(float x)
{
    if (x <= 0)
        return 0;
    if (x >= 1)
        return 1;
    float p = 0.3f;
    return powf(2.0f, -10.0f * x) * sinf((x - p / 4.0f) * (2.0f * 3.14159265f) / p) + 1.0f;
}
float rogue_ui_ease(RogueUIEaseType t, float x)
{
    if (x < 0)
        x = 0;
    if (x > 1)
        x = 1;
    switch (t)
    {
    case ROGUE_EASE_CUBIC_IN:
        return ease_cubic_in(x);
    case ROGUE_EASE_CUBIC_OUT:
        return ease_cubic_out(x);
    case ROGUE_EASE_CUBIC_IN_OUT:
        return ease_cubic_in_out(x);
    case ROGUE_EASE_SPRING:
        return ease_spring(x);
    case ROGUE_EASE_ELASTIC_OUT:
        return ease_elastic_out(x);
    default:
        return x;
    }
}

void rogue_ui_entrance(RogueUIContext* ctx, uint32_t id_hash, float duration_ms,
                       RogueUIEaseType ease)
{
    (void) ctx;
    if (duration_ms <= 0)
        duration_ms = 1.0f;
    if (ctx && ctx->reduced_motion)
    {
        duration_ms *= 0.25f;
    }
    UIAnimEntry* e = ui_anim_alloc(id_hash);
    if (!e)
        return;
    e->t = 0;
    e->duration = duration_ms;
    e->kind = 0;
    e->ease = ease;
}
void rogue_ui_exit(RogueUIContext* ctx, uint32_t id_hash, float duration_ms, RogueUIEaseType ease)
{
    (void) ctx;
    if (duration_ms <= 0)
        duration_ms = 1.0f;
    if (ctx && ctx->reduced_motion)
    {
        duration_ms *= 0.25f;
    }
    UIAnimEntry* e = ui_anim_alloc(id_hash);
    if (!e)
        return;
    e->t = 0;
    e->duration = duration_ms;
    e->kind = 1;
    e->ease = ease;
}
void rogue_ui_button_press_pulse(RogueUIContext* ctx, uint32_t id_hash)
{
    (void) ctx;
    UIAnimEntry* e = ui_anim_alloc(id_hash ^ 0xB00B135u);
    if (!e)
        return;
    e->t = 0;
    e->duration = 180.0f;
    e->kind = 2;
    e->ease = ROGUE_EASE_SPRING;
}

static void ui_anim_step(double dt_ms)
{
    for (int i = 0; i < g_ui_anim_count;)
    {
        UIAnimEntry* e = &g_ui_anims[i];
        e->t += (float) dt_ms;
        if (e->t >= e->duration)
        {
            if (e->kind == 1)
            {
                if (g_ui_exit_done_count <
                    (int) (sizeof(g_ui_exit_done) / sizeof(g_ui_exit_done[0])))
                {
                    g_ui_exit_done[g_ui_exit_done_count].id = e->id;
                    g_ui_exit_done[g_ui_exit_done_count].ttl = 30;
                    g_ui_exit_done_count++;
                }
            }
            g_ui_anims[i] = g_ui_anims[--g_ui_anim_count];
            continue;
        }
        i++;
    }
    for (int i = 0; i < g_ui_exit_done_count;)
    {
        if (--g_ui_exit_done[i].ttl <= 0)
        {
            g_ui_exit_done[i] = g_ui_exit_done[--g_ui_exit_done_count];
            continue;
        }
        i++;
    }
}
static void ui_animation_master_step(double dt_ms)
{
    ui_anim_step(dt_ms);
    ui_timeline_step(dt_ms);
}

/* ---------------- Phase 9 Performance & Virtualization ---------------- */
int rogue_ui_list_virtual_range(int total_items, int item_height, int view_height,
                                int scroll_offset, int* first_index_out, int* count_out)
{
    if (item_height <= 0 || view_height <= 0 || total_items <= 0)
    {
        if (first_index_out)
            *first_index_out = 0;
        if (count_out)
            *count_out = 0;
        return 0;
    }
    if (scroll_offset < 0)
        scroll_offset = 0;
    int first = scroll_offset / item_height;
    if (first >= total_items)
        first = total_items - 1;
    int visible = (view_height + item_height - 1) / item_height;
    if (first + visible > total_items)
        visible = total_items - first;
    if (visible < 0)
        visible = 0;
    if (first_index_out)
        *first_index_out = first;
    if (count_out)
        *count_out = visible;
    return visible;
}
int rogue_ui_list_virtual_emit(RogueUIContext* ctx, RogueUIRect area, int total_items,
                               int item_height, int scroll_offset, uint32_t color_base,
                               uint32_t color_alt)
{
    int first = 0, count = 0;
    int emitted = 0;
    if (rogue_ui_list_virtual_range(total_items, item_height, (int) area.h, scroll_offset, &first,
                                    &count) <= 0)
        return 0;
    for (int i = 0; i < count; i++)
    {
        float y = area.y + (float) ((first + i) * item_height - scroll_offset);
        RogueUIRect r = {area.x, y, area.w, (float) item_height};
        uint32_t c = ((first + i) & 1) ? color_alt : color_base;
        rogue_ui_panel(ctx, r, c);
        emitted++;
    }
    return emitted;
}
RogueUIDirtyInfo rogue_ui_dirty_info(const RogueUIContext* ctx)
{
    RogueUIDirtyInfo di = {0};
    if (!ctx)
        return di;
    di.changed = ctx->dirty_changed;
    di.x = ctx->dirty_x;
    di.y = ctx->dirty_y;
    di.w = ctx->dirty_w;
    di.h = ctx->dirty_h;
    di.changed_node_count = ctx->dirty_node_count;
    di.kind = ctx->dirty_kind;
    return di;
}
void rogue_ui_perf_set_budget(RogueUIContext* ctx, double frame_budget_ms)
{
    if (!ctx)
        return;
    ctx->perf_budget_ms = frame_budget_ms;
}
int rogue_ui_perf_frame_over_budget(const RogueUIContext* ctx)
{
    return (ctx && ctx->perf_last_frame_ms > ctx->perf_budget_ms && ctx->perf_budget_ms > 0) ? 1
                                                                                             : 0;
}
double rogue_ui_perf_last_update_ms(const RogueUIContext* ctx)
{
    return ctx ? ctx->perf_last_update_ms : 0.0;
}
double rogue_ui_perf_last_render_ms(const RogueUIContext* ctx)
{
    return ctx ? ctx->perf_last_render_ms : 0.0;
}
void rogue_ui_perf_set_time_provider(RogueUIContext* ctx, double (*now_ms_fn)(void*), void* user)
{
    if (!ctx)
        return;
    ctx->perf_now = now_ms_fn;
    ctx->perf_now_user = user;
}
static double ui_perf_now(const RogueUIContext* ctx)
{
    if (ctx && ctx->perf_now)
        return ctx->perf_now(ctx->perf_now_user);
    return ctx ? ctx->time_ms : 0.0;
}
void rogue_ui_render(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    double render_start = ui_perf_now(ctx); /* Simulate render time minimal */
    /* Dirty tracking: union of node rects if node count changed or diff changed */
    int node_delta = ctx->node_count - ctx->prev_node_count;
    int diff = rogue_ui_diff_changed(ctx);
    if (node_delta != 0 || (diff && !ctx->dirty_reported_this_frame))
    {
        ctx->dirty_changed = 1;
        ctx->dirty_node_count = ctx->node_count;
        ctx->dirty_kind =
            node_delta != 0 ? 1 : 2; /* structural if node count changed else content */
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        for (int i = 0; i < ctx->node_count; i++)
        {
            RogueUIRect r = ctx->nodes[i].rect;
            if (r.x < minx)
                minx = r.x;
            if (r.y < miny)
                miny = r.y;
            if (r.x + r.w > maxx)
                maxx = r.x + r.w;
            if (r.y + r.h > maxy)
                maxy = r.y + r.h;
        }
        if (ctx->node_count > 0)
        {
            ctx->dirty_x = minx;
            ctx->dirty_y = miny;
            ctx->dirty_w = maxx - minx;
            ctx->dirty_h = maxy - miny;
        }
        ctx->dirty_reported_this_frame = 1;
    }
    else
    {
        ctx->dirty_changed = 0;
        ctx->dirty_kind = 0;
    }
    ctx->prev_node_count = ctx->node_count;
    double render_end = ui_perf_now(ctx);
    ctx->perf_last_render_ms = render_end - render_start;
    double frame_end = render_end;
    ctx->perf_last_frame_ms = frame_end - ctx->perf_frame_start_ms;
    ctx->perf_last_update_ms =
        ctx->perf_last_frame_ms - ctx->perf_last_render_ms; /* simplistic split */
    /* Phase timing accumulation: render phase id=1 */
    ctx->perf_phase_accum[1] += ctx->perf_last_render_ms;
    /* Regression guard evaluation */
    if (ctx->perf_baseline_ms > 0 && ctx->perf_regress_threshold_pct > 0)
    {
        double allowed = ctx->perf_baseline_ms * (1.0 + ctx->perf_regress_threshold_pct);
        if (ctx->perf_last_frame_ms > allowed)
        {
            ctx->perf_regressed_flag = 1;
        }
    }
}

/* --------- Extended Phase 9 APIs (per-phase instrumentation) ---------- */
void rogue_ui_perf_phase_begin(RogueUIContext* ctx, int phase_id)
{
    if (!ctx || phase_id < 0 || phase_id > 7)
        return;
    ctx->perf_phase_start[phase_id] = ui_perf_now(ctx);
}
void rogue_ui_perf_phase_end(RogueUIContext* ctx, int phase_id)
{
    if (!ctx || phase_id < 0 || phase_id > 7)
        return;
    double now = ui_perf_now(ctx);
    double start = ctx->perf_phase_start[phase_id];
    if (start > 0 && now >= start)
        ctx->perf_phase_accum[phase_id] += (now - start);
    ctx->perf_phase_start[phase_id] = 0;
}
double rogue_ui_perf_phase_ms(const RogueUIContext* ctx, int phase_id)
{
    if (!ctx || phase_id < 0 || phase_id > 7)
        return 0.0;
    return ctx->perf_phase_accum[phase_id];
}
void rogue_ui_perf_set_baseline(RogueUIContext* ctx, double baseline_ms)
{
    if (!ctx)
        return;
    ctx->perf_baseline_ms = baseline_ms;
    ctx->perf_regressed_flag = 0;
}
void rogue_ui_perf_set_regression_threshold(RogueUIContext* ctx, double pct_over_baseline)
{
    if (!ctx)
        return;
    ctx->perf_regress_threshold_pct = pct_over_baseline;
}
int rogue_ui_perf_regressed(const RogueUIContext* ctx)
{
    return ctx ? ctx->perf_regressed_flag : 0;
}
void rogue_ui_perf_auto_baseline_reset(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    ctx->perf_autob_count = 0;
    ctx->perf_baseline_ms = 0;
    ctx->perf_regressed_flag = 0;
}
void rogue_ui_perf_auto_baseline_add_sample(RogueUIContext* ctx, double frame_ms, int target_count)
{
    if (!ctx || target_count <= 0)
        return;
    if (ctx->perf_autob_count <
        (int) (sizeof(ctx->perf_autob_samples) / sizeof(ctx->perf_autob_samples[0])))
    {
        ctx->perf_autob_samples[ctx->perf_autob_count++] = frame_ms;
    }
    if (ctx->perf_autob_count >= target_count)
    {
        double sum = 0;
        for (int i = 0; i < ctx->perf_autob_count; i++)
            sum += ctx->perf_autob_samples[i];
        ctx->perf_baseline_ms = sum / (double) ctx->perf_autob_count;
        ctx->perf_autob_count = 0;
        ctx->perf_regressed_flag = 0;
    }
}

/* --------- Glyph cache (Phase 9.3 simplified) ---------- */
static float glyph_synth_advance(unsigned int cp) { /* deterministic pseudo-width */
                                                    return 6.0f + (float) (cp % 5); }
void rogue_ui_text_cache_reset(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    free(ctx->glyph_cache);
    ctx->glyph_cache = NULL;
    ctx->glyph_cache_capacity = 0;
    ctx->glyph_cache_count = 0;
    ctx->glyph_cache_hits = ctx->glyph_cache_misses = 0;
    ctx->glyph_cache_tick = 1;
}
static int glyph_cache_find(RogueUIContext* ctx, unsigned int cp)
{
    for (int i = 0; i < ctx->glyph_cache_count; i++)
    {
        if (ctx->glyph_cache[i].codepoint == cp)
        {
            ctx->glyph_cache[i].lru_tick = ++ctx->glyph_cache_tick;
            return i;
        }
    }
    return -1;
}
static int glyph_cache_insert(RogueUIContext* ctx, unsigned int cp, float adv)
{
    if (ctx->glyph_cache_count >= ctx->glyph_cache_capacity)
    {
        int new_cap = ctx->glyph_cache_capacity ? ctx->glyph_cache_capacity * 2 : 64;
        ctx->glyph_cache = (struct RogueUIGlyphEntry*) realloc(
            ctx->glyph_cache, (size_t) new_cap * sizeof(*ctx->glyph_cache));
        ctx->glyph_cache_capacity = new_cap;
    }
    int idx = ctx->glyph_cache_count++;
    ctx->glyph_cache[idx].codepoint = cp;
    ctx->glyph_cache[idx].advance = adv;
    ctx->glyph_cache[idx].lru_tick = ++ctx->glyph_cache_tick;
    return idx;
}
float rogue_ui_text_cache_measure(RogueUIContext* ctx, const char* text)
{
    if (!ctx || !text)
        return 0;
    float w = 0;
    for (const unsigned char* p = (const unsigned char*) text; *p; ++p)
    {
        unsigned int cp = *p;
        int idx = glyph_cache_find(ctx, cp);
        if (idx >= 0)
        {
            w += ctx->glyph_cache[idx].advance;
            ctx->glyph_cache_hits++;
        }
        else
        {
            float adv = glyph_synth_advance(cp);
            glyph_cache_insert(ctx, cp, adv);
            ctx->glyph_cache_misses++;
            w += adv;
        }
    }
    return w;
}
int rogue_ui_text_cache_hits(const RogueUIContext* ctx) { return ctx ? ctx->glyph_cache_hits : 0; }
int rogue_ui_text_cache_misses(const RogueUIContext* ctx)
{
    return ctx ? ctx->glyph_cache_misses : 0;
}
int rogue_ui_text_cache_size(const RogueUIContext* ctx) { return ctx ? ctx->glyph_cache_count : 0; }
void rogue_ui_text_cache_compact(RogueUIContext* ctx)
{
    if (!ctx || ctx->glyph_cache_count < 2)
        return;
    int limit = ctx->glyph_cache_count / 2; /* keep newest half */
    /* Simple selection: find cutoff tick */
    unsigned int* ticks =
        (unsigned int*) malloc((size_t) ctx->glyph_cache_count * sizeof(unsigned int));
    for (int i = 0; i < ctx->glyph_cache_count; i++)
        ticks[i] = ctx->glyph_cache[i].lru_tick;
    /* find nth largest (limit) via partial selection; simple O(n^2) for small counts */
    for (int i = 0; i < limit; i++)
    {
        int max_i = i;
        for (int j = i + 1; j < ctx->glyph_cache_count; j++)
            if (ticks[j] > ticks[max_i])
                max_i = j;
        unsigned int tmp = ticks[i];
        ticks[i] = ticks[max_i];
        ticks[max_i] = tmp;
    }
    unsigned int cutoff = ticks[limit - 1];
    free(ticks);
    int write = 0;
    for (int i = 0; i < ctx->glyph_cache_count; i++)
    {
        if (ctx->glyph_cache[i].lru_tick >= cutoff)
        {
            if (write != i)
                ctx->glyph_cache[write] = ctx->glyph_cache[i];
            write++;
        }
    }
    ctx->glyph_cache_count = write;
    if (ctx->glyph_cache_capacity > 128 && ctx->glyph_cache_count < ctx->glyph_cache_capacity / 4)
    { /* shrink */
        int new_cap = ctx->glyph_cache_capacity / 2;
        if (new_cap < 64)
            new_cap = 64;
        ctx->glyph_cache = (struct RogueUIGlyphEntry*) realloc(
            ctx->glyph_cache, (size_t) new_cap * sizeof(*ctx->glyph_cache));
        ctx->glyph_cache_capacity = new_cap;
    }
}

float rogue_ui_anim_scale(const RogueUIContext* ctx, uint32_t id_hash)
{
    (void) ctx;
    UIAnimEntry* e_ent = ui_anim_find(id_hash); /* entrance/exit */
    UIAnimEntry* e_pulse = ui_anim_find(id_hash ^ 0xB00B135u);
    float base_scale = 1.0f;
    if (e_ent)
    {
        float x = e_ent->t / e_ent->duration;
        if (x < 0)
            x = 0;
        if (x > 1)
            x = 1;
        float v = rogue_ui_ease(e_ent->ease, x);
        if (e_ent->kind == 0)
        { /* entrance */
            base_scale = 0.85f + 0.15f * v;
        }
        else if (e_ent->kind == 1)
        { /* exit */
            base_scale = 1.0f - 0.15f * v;
        }
    }
    float pulse_scale = 1.0f;
    if (e_pulse && e_pulse->kind == 2)
    {
        float x = e_pulse->t / e_pulse->duration;
        if (x < 0)
            x = 0;
        if (x > 1)
            x = 1;
        float v = rogue_ui_ease(e_pulse->ease, x);
        /* Spring ease produces <0 early for overshoot; convert to positive swell */
        pulse_scale = 1.0f + (1.0f - v) * 0.15f;
    }
    /* Combine multiplicatively to allow entrance grow + pulse overshoot */
    return base_scale * pulse_scale;
}
float rogue_ui_anim_alpha(const RogueUIContext* ctx, uint32_t id_hash)
{
    (void) ctx;
    UIAnimEntry* e = ui_anim_find(id_hash);
    if (!e)
    {
        /* Check if we recently completed an exit */
        for (int i = 0; i < g_ui_exit_done_count; i++)
            if (g_ui_exit_done[i].id == id_hash)
                return 0.0f;
        return 1.0f; /* otherwise fully visible (entrance finished) */
    }
    float x = e->t / e->duration;
    if (x < 0)
        x = 0;
    if (x > 1)
        x = 1;
    float v = rogue_ui_ease(e->ease, x);
    if (e->kind == 0)
    {
        return v; /* entrance fades in */
    }
    else if (e->kind == 1)
    {
        /* exit: simple linear fade independent of easing for predictability */
        return 1.0f - x; /* ensures steady decrease */
    }
    return 1.0f;
}

/* Phase 7.5 Reduced Motion */
void rogue_ui_set_reduced_motion(RogueUIContext* ctx, int enabled)
{
    if (!ctx)
        return;
    ctx->reduced_motion = enabled ? 1 : 0;
}
int rogue_ui_reduced_motion(const RogueUIContext* ctx) { return ctx ? ctx->reduced_motion : 0; }

/* Phase 7.6 Narration stub: store last narrated string (truncated) */
void rogue_ui_narrate(RogueUIContext* ctx, const char* text)
{
    if (!ctx)
        return;
    if (!text)
        text = "";
    size_t i = 0;
    for (; i < sizeof(ctx->narration_last) - 1 && text[i]; ++i)
    {
        ctx->narration_last[i] = text[i];
    }
    ctx->narration_last[i] = '\0';
}
const char* rogue_ui_last_narration(const RogueUIContext* ctx)
{
    return ctx ? ctx->narration_last : NULL;
}

/* Phase 7.7 Focus audit */
void rogue_ui_focus_audit_enable(RogueUIContext* ctx, int enabled)
{
    if (!ctx)
        return;
    ctx->focus_audit_enabled = enabled ? 1 : 0;
}
int rogue_ui_focus_audit_enabled(const RogueUIContext* ctx)
{
    return ctx ? ctx->focus_audit_enabled : 0;
}
int rogue_ui_focus_audit_emit_overlays(RogueUIContext* ctx, uint32_t highlight_color)
{
    if (!ctx || !ctx->frame_active || !ctx->focus_audit_enabled)
        return 0;
    int added = 0;
    for (int i = 0; i < ctx->node_count; i++)
    {
        int k = ctx->nodes[i].kind;
        if (k >= 5 && k <= 8)
        {
            RogueUIRect r = ctx->nodes[i].rect; /* draw thin border panels (simplified) */
            rogue_ui_panel(ctx, (RogueUIRect){r.x - 1, r.y - 1, r.w + 2, r.h + 2}, highlight_color);
            added++;
        }
    }
    return added;
}
size_t rogue_ui_focus_order_export(RogueUIContext* ctx, char* buffer, size_t cap)
{
    if (!ctx || !buffer || cap == 0)
        return 0;
    size_t off = 0;
    for (int i = 0; i < ctx->node_count; i++)
    {
        int k = ctx->nodes[i].kind;
        if (k >= 5 && k <= 8)
        {
            const char* label =
                ctx->nodes[i].text
                    ? ctx->nodes[i].text
                    : (k == 5 ? "button" : (k == 6 ? "toggle" : (k == 7 ? "slider" : "textinput")));
            size_t len = strlen(label);
            if (off + len + 1 >= cap)
                break;
            memcpy(buffer + off, label, len);
            off += len;
            buffer[off++] = '\n';
        }
    }
    if (off < cap)
        buffer[off] = '\0';
    return off;
}

const RogueUINode* rogue_ui_nodes(const RogueUIContext* ctx, int* count_out)
{
    if (count_out)
        *count_out = ctx ? ctx->node_count : 0;
    return ctx ? ctx->nodes : NULL;
}

unsigned int rogue_ui_rng_next(RogueUIContext* ctx)
{
    if (!ctx)
        return 0;
    return xorshift32(&ctx->rng_state);
}
void rogue_ui_set_theme(RogueUIContext* ctx, const RogueUITheme* theme)
{
    if (!ctx || !theme)
        return;
    ctx->theme = *theme;
}

void rogue_ui_set_simulation_snapshot(RogueUIContext* ctx, const void* snapshot, size_t size)
{
    if (!ctx)
        return;
    ctx->sim_snapshot = snapshot;
    ctx->sim_snapshot_size = size;
}
const void* rogue_ui_simulation_snapshot(const RogueUIContext* ctx, size_t* size_out)
{
    if (size_out)
        *size_out = ctx ? ctx->sim_snapshot_size : 0;
    return ctx ? ctx->sim_snapshot : NULL;
}

static size_t align_up(size_t v, size_t a) { return (v + (a - 1)) & ~(a - 1); }
void* rogue_ui_arena_alloc(RogueUIContext* ctx, size_t size, size_t align)
{
    if (!ctx || size == 0)
        return NULL;
    if (align == 0)
        align = 8;
    size_t off = align_up(ctx->arena_offset, align);
    if (off + size > ctx->arena_size)
        return NULL;
    void* ptr = ctx->arena + off;
    ctx->arena_offset = off + size;
    return ptr;
}

int rogue_ui_text_dup(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color)
{
    if (!ctx || !text)
        return -1;
    size_t len = strlen(text) + 1;
    char* copy = (char*) rogue_ui_arena_alloc(ctx, len, 1);
    if (!copy)
        return -1;
    memcpy(copy, text, len);
    return rogue_ui_text(ctx, r, copy, color);
}

/* FNV-1a 64-bit */
static uint64_t fnv1a64(const void* data, size_t len)
{
    const unsigned char* p = (const unsigned char*) data;
    uint64_t h = 0xcbf29ce484222325ULL;
    for (size_t i = 0; i < len; i++)
    {
        h ^= p[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

size_t rogue_ui_serialize(const RogueUIContext* ctx, char* buffer, size_t buffer_size)
{
    if (!ctx || !buffer || buffer_size == 0)
        return 0;
    size_t written = 0;
    for (int i = 0; i < ctx->node_count; i++)
    {
        const RogueUINode* n = &ctx->nodes[i];
        int w = snprintf(buffer + written, buffer_size - written,
                         "%d %.2f %.2f %.2f %.2f %08X %s\n", n->kind, n->rect.x, n->rect.y,
                         n->rect.w, n->rect.h, n->color, n->text ? n->text : "");
        if (w < 0)
            break;
        if ((size_t) w >= buffer_size - written)
        {
            written = buffer_size - 1;
            break;
        }
        written += (size_t) w;
    }
    if (written < buffer_size)
        buffer[written] = '\0';
    else
        buffer[buffer_size - 1] = '\0';
    return written;
}

int rogue_ui_diff_changed(RogueUIContext* ctx)
{
    if (!ctx)
        return 0;
    char tmp[1024];
    size_t len = rogue_ui_serialize(ctx, tmp, sizeof tmp);
    uint64_t h = fnv1a64(tmp, len);
    if (h != ctx->last_serial_hash)
    {
        ctx->last_serial_hash = h;
        return 1;
    }
    return 0;
}

/* Phase 10.1 Headless harness implementation */
uint64_t rogue_ui_tree_hash(RogueUIContext* ctx)
{
    if (!ctx)
        return 0;
    char tmp[1024];
    size_t len = rogue_ui_serialize(ctx, tmp, sizeof tmp);
    return fnv1a64(tmp, len);
}
int rogue_ui_headless_run(const RogueUIContextConfig* cfg, double delta_time_ms,
                          RogueUIBuildFn build, void* user, uint64_t* out_hash)
{
    if (!cfg || !build)
        return 0;
    RogueUIContext ctx;
    if (!rogue_ui_init(&ctx, cfg))
        return 0;
    rogue_ui_begin(&ctx, delta_time_ms);
    RogueUIInputState zero_in = {0};
    rogue_ui_set_input(&ctx, &zero_in);
    build(&ctx, user);
    rogue_ui_end(&ctx);
    if (out_hash)
        *out_hash = rogue_ui_tree_hash(&ctx);
    rogue_ui_shutdown(&ctx);
    return 1;
}

static void ui_enqueue(RogueUIContext* ctx, int k, int a, int b, int c)
{
    if (!ctx)
        return;
    int next = (ctx->event_tail + 1) % 32;
    if (next == ctx->event_head)
        return;
    ctx->event_queue[ctx->event_tail].kind = k;
    ctx->event_queue[ctx->event_tail].a = a;
    ctx->event_queue[ctx->event_tail].b = b;
    ctx->event_queue[ctx->event_tail].c = c;
    ctx->event_tail = next;
}
int rogue_ui_poll_event(RogueUIContext* ctx, RogueUIEvent* out)
{
    if (!ctx || !out)
        return 0;
    if (ctx->event_head == ctx->event_tail)
        return 0;
    *out = *(RogueUIEvent*) &ctx->event_queue[ctx->event_head];
    ctx->event_head = (ctx->event_head + 1) % 32;
    return 1;
}

/* ---- Radial Selector (Phase 4.10) ---- */
void rogue_ui_radial_open(RogueUIContext* ctx, int count)
{
    if (!ctx || count <= 0 || count > 12)
        return; /* cap to 12 wedges */
    ctx->radial.active = 1;
    ctx->radial.count = count;
    ctx->radial.selection = 0;
    ui_enqueue(ctx, ROGUE_UI_EVENT_RADIAL_OPEN, count, 0, 0);
}
void rogue_ui_radial_close(RogueUIContext* ctx)
{
    if (!ctx || !ctx->radial.active)
        return;
    ui_enqueue(ctx, ROGUE_UI_EVENT_RADIAL_CANCEL, ctx->radial.selection, 0, 0);
    ctx->radial.active = 0;
}

/* Angle helper: returns wedge index for angle radians (-pi..pi). */
static int radial_index_from_angle(const RogueUIRadialDesc* r, float angle)
{
    if (!r || r->count <= 0)
        return 0; /* map angle so that up ( -pi/2 ) is index 0 then clockwise */
    const float PI = 3.14159265358979323846f;
    float two_pi = 6.28318530718f;
    float a = angle + PI / 2.0f;
    while (a < 0)
        a += two_pi;
    while (a >= two_pi)
        a -= two_pi;
    float sector = two_pi / (float) r->count;
    int idx = (int) (a / sector);
    if (idx < 0)
        idx = 0;
    if (idx >= r->count)
        idx = r->count - 1;
    return idx;
}

int rogue_ui_radial_menu(RogueUIContext* ctx, float cx, float cy, float radius, const char** labels,
                         int count)
{
    if (!ctx || !ctx->frame_active || !ctx->radial.active || count != ctx->radial.count)
        return -1;
    if (radius <= 0)
        radius = 60.0f;
    if (count <= 0)
        return -1;
    /* Update selection from controller axis or keyboard arrows */
    float ax = ctx->controller.axis_x;
    float ay = ctx->controller.axis_y;
    int moved = 0;
    if (fabsf(ax) > 0.35f || fabsf(ay) > 0.35f)
    {
        float ang = atan2f(ay, ax); /* right=0, up= -pi/2 */
        ctx->radial.selection = radial_index_from_angle(&ctx->radial, ang);
        moved = 1;
    }
    else
    {
        /* Keyboard incremental cycle */
        if (ctx->input.key_right || ctx->input.key_down)
        {
            ctx->radial.selection = (ctx->radial.selection + 1) % ctx->radial.count;
            moved = 1;
        }
        else if (ctx->input.key_left || ctx->input.key_up)
        {
            ctx->radial.selection =
                (ctx->radial.selection - 1 + ctx->radial.count) % ctx->radial.count;
            moved = 1;
        }
    }
    /* Accept / cancel */
    if (ctx->input.key_activate || ctx->controller.button_a)
    {
        ui_enqueue(ctx, ROGUE_UI_EVENT_RADIAL_CHOOSE, ctx->radial.selection, 0, 0);
        ctx->radial.active = 0;
    }
    if (ctx->input.mouse_released && ctx->input.mouse_down == 0 && !ctx->input.key_activate &&
        !ctx->controller.button_a)
    { /* click release outside center cancels */
    }
    /* Root panel */
    RogueUIRect root_rect = {cx - radius - 8, cy - radius - 8, radius * 2 + 16, radius * 2 + 16};
    int root = rogue_ui_panel(ctx, root_rect, 0x202028C0u);
    /* Wedges approximated by small panels at arc midpoints */
    float two_pi = 6.28318530718f;
    for (int i = 0; i < count; i++)
    {
        float t = ((float) i + 0.5f) / (float) count;
        float ang = t * two_pi;
        float px = cx + cosf(ang) * radius * 0.65f;
        float py = cy + sinf(ang) * radius * 0.65f;
        float w = 48, h = 16;
        RogueUIRect rct = {px - w * 0.5f, py - h * 0.5f, w, h};
        uint32_t col = (i == ctx->radial.selection) ? 0x5050A0FFu : 0x303038FFu;
        rogue_ui_panel(ctx, rct, col);
        if (labels && labels[i])
            rogue_ui_text(ctx, (RogueUIRect){rct.x + 2, rct.y + 2, rct.w - 4, rct.h - 4}, labels[i],
                          0xFFFFFFFFu);
    }
    (void) moved;
    return root;
}

int rogue_ui_inventory_grid(RogueUIContext* ctx, RogueUIRect rect, const char* id,
                            int slot_capacity, int columns, int* item_ids, int* item_counts,
                            int cell_size, int* first_visible, int* visible_count)
{
    if (ctx->node_capacity == 0)
    {
        fprintf(stderr, "FATAL: node_capacity==0 (corrupted context)\n");
        return -1;
    }
    if (!ctx || !ctx->frame_active || slot_capacity <= 0 || columns <= 0)
        return -1;
    /* TEMP DEBUG: instrumentation to locate segfault in tests */
    static int dbg_counter = 0;
    dbg_counter++;
    if (cell_size <= 0)
        cell_size = 32;
    if (columns > slot_capacity)
        columns = slot_capacity;
    int root = rogue_ui_panel(ctx, rect, ctx->theme.panel_bg_color);
    (void) id;
    /* Virtualization integration (Phase 9.4 completion):
        We virtualize by ROW (not individual slots) so one wheel notch scrolls a single row.
        Steps:
           1) Maintain persistent s_scroll_row advanced by wheel_delta sign.
           2) Compute total_rows; clamp s_scroll_row.
           3) Use rogue_ui_list_virtual_range() with total_rows and row height (item_pitch) to
       obtain first_row and visible_row_count. 4) Map rows back to slot index range. */
    static int s_scroll_row = 0; /* persistent first row */
    if (ctx->input.wheel_delta > 0)
        s_scroll_row--;
    else if (ctx->input.wheel_delta < 0)
        s_scroll_row++; /* one row per notch */
    if (s_scroll_row < 0)
        s_scroll_row = 0;
    float spacing = 2.0f;
    float pad = 2.0f;
    int item_pitch = cell_size + (int) spacing; /* vertical stride per row */
    int total_rows = (slot_capacity + columns - 1) / columns;
    int max_row = total_rows - 1;
    if (max_row < 0)
        max_row = 0;
    if (s_scroll_row > max_row)
        s_scroll_row = max_row;
    int view_height = (int) rect.h;
    int scroll_offset = s_scroll_row * item_pitch; /* pixels */
    int first_row = 0, visible_rows = 0;
    rogue_ui_list_virtual_range(total_rows, item_pitch, view_height, scroll_offset, &first_row,
                                &visible_rows);
    int start = first_row * columns;
    int end_slot = start + visible_rows * columns;
    if (end_slot > slot_capacity)
        end_slot = slot_capacity;
    if (visible_count)
        *visible_count = end_slot - start;
    if (first_visible)
        *first_visible = start;
    float mx = ctx->input.mouse_x, my = ctx->input.mouse_y;
    int hovered_slot = -1;
    for (int s = start; s < end_slot; ++s)
    {
        int local = s - start;
        int r = local / columns;
        int c = local % columns;
        float x = rect.x + pad + c * (cell_size + spacing);
        float y = rect.y + pad + r * (cell_size + spacing);
        RogueUIRect cell_r = {x, y, (float) cell_size, (float) cell_size};
        uint32_t base_col = 0x303038FFu;
        if (item_ids && item_ids[s])
        {
            /* Simple rarity mapping: rarity = item_id % 5 (0..4); map to color similar to
             * rogue_rarity_color */
            int rarity = item_ids[s] % 5;
            unsigned char rr = 240, rg = 210, rb = 60; /* default (common) */
            if (rarity == 1)
            {
                rr = 80;
                rg = 220;
                rb = 80;
            }
            else if (rarity == 2)
            {
                rr = 80;
                rg = 120;
                rb = 255;
            }
            else if (rarity == 3)
            {
                rr = 180;
                rg = 70;
                rb = 220;
            }
            else if (rarity == 4)
            {
                rr = 255;
                rg = 140;
                rb = 0;
            }
            /* Use rarity color as border by creating outer panel then inner base */
            RogueUIRect outer = cell_r;
            RogueUIRect inner = {cell_r.x + 1, cell_r.y + 1, cell_r.w - 2, cell_r.h - 2};
            rogue_ui_panel(ctx, outer, (uint32_t) ((rr << 24) | (rg << 16) | (rb << 8) | 0xFFu));
            int panel = rogue_ui_panel(ctx, inner, base_col);
            (void) panel;
            if (mx >= cell_r.x && my >= cell_r.y && mx < cell_r.x + cell_r.w &&
                my < cell_r.y + cell_r.h)
                hovered_slot = s;
            if (item_counts)
            {
                /* Use arena dup so text pointer remains valid after this function returns */
                char tmp[32];
                snprintf(tmp, sizeof tmp, "%d", item_counts[s]);
                rogue_ui_text_dup(ctx,
                                  (RogueUIRect){inner.x + 2, inner.y + 2, inner.w - 4, inner.h - 4},
                                  tmp, (uint32_t) ((rr << 24) | (rg << 16) | (rb << 8) | 0xFFu));
            }
        }
        else
        {
            int panel = rogue_ui_panel(ctx, cell_r, base_col);
            (void) panel;
            if (mx >= cell_r.x && my >= cell_r.y && mx < cell_r.x + cell_r.w &&
                my < cell_r.y + cell_r.h)
                hovered_slot = s;
        }
    }
    /* Drag begin */
    if (!ctx->drag_active && hovered_slot >= 0 && ctx->input.mouse_pressed && item_ids &&
        item_ids[hovered_slot])
    {
        fprintf(stderr, "DBG %d drag_begin slot=%d id=%d\n", dbg_counter, hovered_slot,
                item_ids[hovered_slot]);
        fflush(stderr);
        ctx->drag_active = 1;
        ctx->drag_from_slot = hovered_slot;
        ctx->drag_item_id = item_ids[hovered_slot];
        ctx->drag_item_count = item_counts ? item_counts[hovered_slot] : 1;
        ui_enqueue(ctx, ROGUE_UI_EVENT_DRAG_BEGIN, hovered_slot, ctx->drag_item_id,
                   ctx->drag_item_count);
    }
    /* Drag end */
    if (ctx->drag_active && ctx->input.mouse_released)
    {
        int target = hovered_slot >= 0 ? hovered_slot : ctx->drag_from_slot;
        fprintf(stderr, "DBG %d drag_end from=%d to=%d\n", dbg_counter, ctx->drag_from_slot,
                target);
        fflush(stderr);
        if (target >= 0 && target < slot_capacity && item_ids)
        {
            if (target != ctx->drag_from_slot)
            {
                int id_a = item_ids[ctx->drag_from_slot];
                int ct_a = item_counts ? item_counts[ctx->drag_from_slot] : 0;
                int id_b = item_ids[target];
                int ct_b = item_counts ? item_counts[target] : 0;
                item_ids[target] = id_a;
                if (item_counts)
                    item_counts[target] = ct_a;
                item_ids[ctx->drag_from_slot] = id_b;
                if (item_counts)
                    item_counts[ctx->drag_from_slot] = ct_b;
            }
        }
        ui_enqueue(ctx, ROGUE_UI_EVENT_DRAG_END, ctx->drag_from_slot, target, ctx->drag_item_id);
        ctx->drag_active = 0;
        ctx->drag_from_slot = -1;
        ctx->drag_item_id = 0;
        ctx->drag_item_count = 0;
    }
    /* Context menu open (secondary button) */
    if (!ctx->ctx_menu_active && hovered_slot >= 0 && ctx->input.mouse2_pressed && item_ids &&
        item_ids[hovered_slot])
    {
        ctx->ctx_menu_active = 1;
        ctx->ctx_menu_slot = hovered_slot;
        ctx->ctx_menu_selection = 0;
        ui_enqueue(ctx, ROGUE_UI_EVENT_CONTEXT_OPEN, hovered_slot, 0, 0);
    }
    /* Context menu navigation & selection (simple vertical list) */
    static const char* menu_items[] = {"Equip", "Salvage", "Compare", "Cancel"};
    int menu_count = (int) (sizeof(menu_items) / sizeof(menu_items[0]));
    if (ctx->ctx_menu_active)
    {
        if (ctx->input.key_down)
        {
            ctx->ctx_menu_selection = (ctx->ctx_menu_selection + 1) % menu_count;
        }
        if (ctx->input.key_up)
        {
            ctx->ctx_menu_selection = (ctx->ctx_menu_selection - 1 + menu_count) % menu_count;
        }
        if (ctx->input.key_activate)
        {
            int sel = ctx->ctx_menu_selection;
            if (sel == menu_count - 1)
            {
                ui_enqueue(ctx, ROGUE_UI_EVENT_CONTEXT_CANCEL, ctx->ctx_menu_slot, 0, 0);
            }
            else
            {
                ui_enqueue(ctx, ROGUE_UI_EVENT_CONTEXT_SELECT, ctx->ctx_menu_slot, sel, 0);
            }
            ctx->ctx_menu_active = 0;
        }
        else if (ctx->input.mouse_pressed && !ctx->input.mouse2_pressed)
        { /* click outside cancels */
            ui_enqueue(ctx, ROGUE_UI_EVENT_CONTEXT_CANCEL, ctx->ctx_menu_slot, 0, 0);
            ctx->ctx_menu_active = 0;
        }
        /* Render simple menu panel to the right */
        RogueUIRect mrect = {rect.x + rect.w + 8, rect.y + 16, 100, (float) (menu_count * 16 + 4)};
        int mroot = rogue_ui_panel(ctx, mrect, 0x202028FFu);
        (void) mroot;
        for (int i = 0; i < menu_count; i++)
        {
            uint32_t col = (i == ctx->ctx_menu_selection) ? 0x5050A0FFu : 0x303038FFu;
            RogueUIRect ir = {mrect.x + 2, mrect.y + 2 + i * 16, mrect.w - 4, 14};
            rogue_ui_panel(ctx, ir, col);
            rogue_ui_text(ctx, (RogueUIRect){ir.x + 2, ir.y, ir.w - 4, ir.h}, menu_items[i],
                          ctx->theme.text_color);
        }
    }
    /* Stack split open */
    if (!ctx->stack_split_active && ctx->input.key_ctrl && hovered_slot >= 0 &&
        hovered_slot < slot_capacity && ctx->input.mouse_pressed && item_ids &&
        item_ids[hovered_slot] && item_counts && item_counts[hovered_slot] > 1)
    {
        fprintf(stderr, "DBG %d split_open slot=%d total=%d\n", dbg_counter, hovered_slot,
                item_counts[hovered_slot]);
        fflush(stderr);
        ctx->stack_split_active = 1;
        ctx->stack_split_from_slot = hovered_slot;
        ctx->stack_split_total = item_counts[hovered_slot];
        ctx->stack_split_value = ctx->stack_split_total / 2;
        ui_enqueue(ctx, ROGUE_UI_EVENT_STACK_SPLIT_OPEN, hovered_slot, ctx->stack_split_total,
                   ctx->stack_split_value);
    }
    if (ctx->stack_split_active)
    {
        if (ctx->input.wheel_delta > 0)
        {
            ctx->stack_split_value++;
            if (ctx->stack_split_value >= ctx->stack_split_total)
                ctx->stack_split_value = ctx->stack_split_total - 1;
        }
        else if (ctx->input.wheel_delta < 0)
        {
            ctx->stack_split_value--;
            if (ctx->stack_split_value < 1)
                ctx->stack_split_value = 1;
        }
        if (ctx->input.key_activate)
        {
            int from = ctx->stack_split_from_slot;
            int move = ctx->stack_split_value;
            fprintf(stderr, "DBG %d split_apply from=%d move=%d total_before=%d\n", dbg_counter,
                    from, move, item_counts ? item_counts[from] : -1);
            fflush(stderr);
            if (item_counts && from >= 0 && from < slot_capacity && item_counts[from] > move)
            {
                for (int i = 0; i < slot_capacity; i++)
                {
                    if (item_ids[i] == 0)
                    {
                        item_ids[i] = item_ids[from];
                        if (item_counts)
                            item_counts[i] = move;
                        item_counts[from] -= move;
                        fprintf(stderr,
                                "DBG %d split_apply target_slot=%d new_source=%d new_target=%d\n",
                                dbg_counter, i, item_counts[from], item_counts[i]);
                        fflush(stderr);
                        ui_enqueue(ctx, ROGUE_UI_EVENT_STACK_SPLIT_APPLY, from, i, move);
                        break;
                    }
                }
            }
            ctx->stack_split_active = 0;
        }
        else if (ctx->input.mouse_released && ctx->input.mouse_down == 0)
        {
            fprintf(stderr, "DBG %d split_cancel from=%d\n", dbg_counter,
                    ctx->stack_split_from_slot);
            fflush(stderr);
            ui_enqueue(ctx, ROGUE_UI_EVENT_STACK_SPLIT_CANCEL, ctx->stack_split_from_slot, 0, 0);
            ctx->stack_split_active = 0;
        }
        RogueUIRect m = {rect.x + rect.w + 8, rect.y, 120, 48};
        int mroot = rogue_ui_panel(ctx, m, 0x404048FFu);
        (void) mroot;
        char tmp[32];
        snprintf(tmp, sizeof tmp, "Split %d/%d", ctx->stack_split_value, ctx->stack_split_total);
        rogue_ui_text_dup(ctx, (RogueUIRect){m.x + 4, m.y + 4, m.w - 8, 16}, tmp,
                          ctx->theme.text_color);
    }

    /* Inline stat delta preview (Phase 4.5) – simplistic placeholder:
       When hovering an occupied slot, show a small panel with item's base damage or armor and a
       delta vs. a baseline (previous hovered item). In real integration this would compare against
       equipped item of same slot; here we simulate by tracking last hovered slot stats. */
    int show_preview = (hovered_slot >= 0 && item_ids && item_ids[hovered_slot]);
    if (show_preview)
    {
        fprintf(stderr, "DBG %d preview slot=%d drag_active=%d split_active=%d\n", dbg_counter,
                hovered_slot, ctx->drag_active, ctx->stack_split_active);
        fflush(stderr);
        /* node_count use is safe given capacity check above */
        int cur_slot = hovered_slot;
        if (ctx->stat_preview_slot != cur_slot)
        {
            ui_enqueue(ctx, ROGUE_UI_EVENT_STAT_PREVIEW_SHOW, cur_slot, 0, 0);
            ctx->stat_preview_slot = cur_slot;
        }
        /* Acquire fake stats: use item_id as proxy for base value for deterministic test (id % 100
         * as damage, id / 100 as armor). */
        int item_id = item_ids[cur_slot];
        int dmg = item_id % 100;
        int armor = item_id / 100;
        (void) armor;
        int prev_item_id =
            (ctx->drag_active && ctx->drag_from_slot >= 0)
                ? item_ids[ctx->drag_from_slot]
                : (ctx->stat_preview_slot == cur_slot ? item_id : item_id); /* simplified */
        int prev_dmg = prev_item_id % 100;
        int delta = dmg - prev_dmg;
        char line[64];
        snprintf(line, sizeof line, "DMG %d (%+d)", dmg, delta);
        uint32_t col = 0x808080FFu;
        if (delta > 0)
            col = 0x30A050FFu;
        else if (delta < 0)
            col = 0xA03030FFu;
        RogueUIRect pr = {rect.x + rect.w + 8,
                          rect.y + (ctx->stack_split_active
                                        ? 56
                                        : (ctx->ctx_menu_active ? (float) (16 * 5 + 8) : 0)),
                          110, 20};
        rogue_ui_panel(ctx, pr, 0x202028FFu); /* duplicate stat preview line so pointer is stable */
        rogue_ui_text_dup(ctx, (RogueUIRect){pr.x + 4, pr.y + 2, pr.w - 8, pr.h - 4}, line, col);
    }
    else if (ctx->stat_preview_slot != -1)
    {
        fprintf(stderr, "DBG %d preview_hide slot=%d\n", dbg_counter, ctx->stat_preview_slot);
        fflush(stderr);
        ui_enqueue(ctx, ROGUE_UI_EVENT_STAT_PREVIEW_HIDE, ctx->stat_preview_slot, 0, 0);
        ctx->stat_preview_slot = -1;
    }
    /* End inventory grid */
    return root;
}
