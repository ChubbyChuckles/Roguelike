#include "ui_context.h"
#include <math.h>
#include <string.h>

/* Style guide & inspector, snapshot, radial, headless */
int rogue_ui_panel(RogueUIContext* ctx, RogueUIRect r, uint32_t color);
int rogue_ui_text(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color);
int rogue_ui_text_dup(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color);

void rogue_ui_style_guide_build(RogueUIContext* ctx)
{
    if (!ctx || !ctx->frame_active)
        return;
    float x = 10, y = 10;
    rogue_ui_text(ctx, (RogueUIRect){x, y, 160, 14}, "STYLE GUIDE", 0xFFFFFFFFu);
    y += 18;
    rogue_ui_panel(ctx, (RogueUIRect){x, y, 140, 28}, 0x303030FFu);
    y += 34;
    int tgl_state = 1;
    float slider_v = 0.5f;
    char buf[16] = "Txt";
    rogue_ui_button(ctx, (RogueUIRect){x, y, 100, 22}, "Button", 0x406090FFu, 0xFFFFFFFFu);
    y += 28;
    rogue_ui_toggle(ctx, (RogueUIRect){x, y, 100, 22}, "Toggle", &tgl_state, 0x505050FFu,
                    0x208020FFu, 0xFFFFFFFFu);
    y += 28;
    rogue_ui_slider(ctx, (RogueUIRect){x, y, 120, 16}, 0.0f, 1.0f, &slider_v, 0x202020FFu,
                    0x80C040FFu);
    y += 24;
    rogue_ui_text_input(ctx, (RogueUIRect){x, y, 120, 20}, buf, (int) sizeof buf, 0x202020FFu,
                        0xFFFFFFFFu);
    y += 26;
    rogue_ui_progress_bar(ctx, (RogueUIRect){x, y, 120, 10}, 66, 100, 0x202020FFu, 0x60A0F0FFu, 0);
    y += 18;
}

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
        {
            RogueUIRect r = n->rect;
            r.x -= 2;
            r.y -= 2;
            r.w += 4;
            r.h += 4;
            rogue_ui_panel(ctx, r,
                           (i == ctx->inspector_selected_index) ? highlight_color : 0xFF00FF30u);
        }
    }
    return ctx->node_count - 1;
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

int rogue_ui_snapshot(const RogueUIContext* ctx, RogueUICrashSnapshot* out)
{
    if (!ctx || !out)
        return 0;
    out->node_count = ctx->node_count;
    out->tree_hash = ((RogueUIContext*) ctx)->last_serial_hash ? ctx->last_serial_hash : 0;
    out->input = ctx->input;
    return 1;
}

/* Headless */
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

/* Events already implemented in inventory module */

/* Radial selector */
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
void rogue_ui_radial_open(RogueUIContext* ctx, int count)
{
    if (!ctx || count <= 0 || count > 12)
        return;
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
static int radial_index_from_angle(const RogueUIRadialDesc* r, float angle)
{
    if (!r || r->count <= 0)
        return 0;
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
    float ax = ctx->controller.axis_x;
    float ay = ctx->controller.axis_y;
    if (fabsf(ax) > 0.35f || fabsf(ay) > 0.35f)
    {
        float ang = atan2f(ay, ax);
        ctx->radial.selection = radial_index_from_angle(&ctx->radial, ang);
    }
    else
    {
        if (ctx->input.key_right || ctx->input.key_down)
            ctx->radial.selection = (ctx->radial.selection + 1) % ctx->radial.count;
        else if (ctx->input.key_left || ctx->input.key_up)
            ctx->radial.selection =
                (ctx->radial.selection - 1 + ctx->radial.count) % ctx->radial.count;
    }
    if (ctx->input.key_activate || ctx->controller.button_a)
    {
        ui_enqueue(ctx, ROGUE_UI_EVENT_RADIAL_CHOOSE, ctx->radial.selection, 0, 0);
        ctx->radial.active = 0;
    }
    RogueUIRect root_rect = {cx - radius - 8, cy - radius - 8, radius * 2 + 16, radius * 2 + 16};
    int root = rogue_ui_panel(ctx, root_rect, 0x202028C0u);
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
    return root;
}
