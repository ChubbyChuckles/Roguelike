#include "../../util/log.h"
#include "ui_context.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Core initialization / shutdown and primitive node push helpers */
#include "ui_context_internal.h"
static unsigned int xorshift32(unsigned int* s)
{
    unsigned int x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

/* Forward decl from animation module (implemented in ui_context_animation.c) */
void ui_animation_master_step(double dt_ms);

/* Forward decls from skillgraph TU (existing) */
void rogue_ui_skillgraph_free_quadtree(void* qt);

/* (assign_id / push_node moved to ui_context_internal.h) */

/* Public core API */
int rogue_ui_init(RogueUIContext* ctx, const RogueUIContextConfig* cfg)
{
    if (!ctx || !cfg)
        return 0;
    memset(ctx, 0, sizeof *ctx);
    ctx->hot_index = ctx->active_index = ctx->focus_index = ctx->modal_index =
        ctx->last_hover_index = -1;
    int cap = cfg->max_nodes > 0 ? cfg->max_nodes : 128;
    if (cap <= 0)
        cap = 128;
    ctx->nodes = (RogueUINode*) calloc((size_t) cap, sizeof(RogueUINode));
    if (!ctx->nodes)
        return 0;
    ctx->node_capacity = cap;
    ctx->rng_state = cfg->seed ? cfg->seed : 0xC0FFEEu;
    ctx->theme.panel_bg_color = 0x202028FFu;
    ctx->theme.text_color = 0xFFFFFFFFu;
    size_t arena_size = cfg->arena_size ? cfg->arena_size : (size_t) (32 * 1024);
    ctx->arena = (unsigned char*) malloc(arena_size);
    if (!ctx->arena)
    {
        free(ctx->nodes);
        ctx->nodes = NULL;
        return 0;
    }
    memset(ctx->arena, 0, arena_size);
    ctx->arena_size = arena_size;
    ctx->key_repeat_initial_ms = 400.0;
    ctx->key_repeat_interval_ms = 65.0;
    ctx->chord_timeout_ms = 900.0;
    ctx->radial.active = 0;
    ctx->radial.count = 0;
    ctx->radial.selection = 0;
    ctx->initialized_flag = 0xC0DEFACE;
    return 1;
}

void rogue_ui_shutdown(RogueUIContext* ctx)
{
    if (!ctx)
        return;
    if (ctx->initialized_flag != 0xC0DEFACE)
        return;
    free(ctx->nodes);
    ctx->nodes = NULL;
    ctx->node_capacity = ctx->node_count = 0;
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
        rogue_ui_skillgraph_free_quadtree(ctx->skillgraph_quadtree);
        ctx->skillgraph_quadtree = NULL;
    }
    ctx->initialized_flag = 0;
}

void rogue_ui_begin(RogueUIContext* ctx, double delta_time_ms)
{
    if (!ctx || ctx->initialized_flag != 0xC0DEFACE)
        return;
    if (ctx->node_capacity == 0)
    {
        ctx->nodes = (RogueUINode*) calloc(64, sizeof(RogueUINode));
        if (ctx->nodes)
            ctx->node_capacity = 64;
    }
    if (!ctx->nodes)
        return;
    if (!ctx->arena)
    { /* continue without arena */
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
    ui_animation_master_step(scaled_dt);
    ctx->perf_frame_start_ms = ctx->time_ms;
    ctx->perf_update_start_ms = ctx->time_ms;
}

void rogue_ui_end(RogueUIContext* ctx)
{
    if (ctx)
        ctx->frame_active = 0;
}

uint32_t rogue_ui_make_id(const char* label)
{
    uint32_t h = 2166136261u;
    const unsigned char* s = (const unsigned char*) (label ? label : "");
    while (*s)
    {
        h ^= *s++;
        h *= 16777619u;
    }
    return h;
}
const RogueUINode* rogue_ui_find_by_id(const RogueUIContext* ctx, uint32_t id_hash)
{
    if (!ctx)
        return NULL;
    for (int i = 0; i < ctx->node_count; i++)
        if (ctx->nodes[i].id_hash == id_hash)
            return &ctx->nodes[i];
    return NULL;
}

int rogue_ui_panel(RogueUIContext* ctx, RogueUIRect r, uint32_t color)
{
    if (!ctx || !ctx->frame_active)
        return -1;
    RogueUINode n;
    memset(&n, 0, sizeof n);
    n.rect = r;
    n.color = color;
    n.kind = 0;
    rogue_ui_internal_assign_id(&n);
    return rogue_ui_internal_push_node(ctx, n);
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
    rogue_ui_internal_assign_id(&n);
    return rogue_ui_internal_push_node(ctx, n);
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
    rogue_ui_internal_assign_id(&n);
    return rogue_ui_internal_push_node(ctx, n);
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
    return rogue_ui_internal_push_node(ctx, n);
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
    return rogue_ui_internal_push_node(ctx, n);
}

/* Expose helper for other modules */
int rogue_ui_focused_index(const RogueUIContext* ctx) { return ctx ? ctx->focus_index : -1; }

unsigned int rogue_ui_rng_next(RogueUIContext* ctx)
{
    if (!ctx)
        return 0;
    return xorshift32(&ctx->rng_state);
}
void rogue_ui_set_theme(RogueUIContext* ctx, const RogueUITheme* theme)
{
    if (ctx && theme)
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

/* Serialization & diff */
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
uint64_t rogue_ui_tree_hash(RogueUIContext* ctx)
{
    if (!ctx)
        return 0;
    char tmp[1024];
    size_t len = rogue_ui_serialize(ctx, tmp, sizeof tmp);
    return fnv1a64(tmp, len);
}

/* Accessors */
const RogueUINode* rogue_ui_nodes(const RogueUIContext* ctx, int* count_out)
{
    if (count_out)
        *count_out = ctx ? ctx->node_count : 0;
    return ctx ? ctx->nodes : NULL;
}
