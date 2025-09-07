#include "ui_context.h"
#include "ui_context_internal.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static int rect_contains(const RogueUIRect* r, float x, float y)
{
    return x >= r->x && y >= r->y && x <= r->x + r->w && y <= r->y + r->h;
}

/* Forward decls */
uint32_t rogue_ui_make_id(const char* label);

/* Duplicate helpers removed in favor of ui_context_internal.h */
static int interactive_push(RogueUIContext* ctx, RogueUINode* node)
{
    int idx = rogue_ui_internal_push_node(ctx, *node);
    if (idx < 0)
        return -1;
    float mx = ctx->input.mouse_x, my = ctx->input.mouse_y;
    if (rect_contains(&node->rect, mx, my))
        ctx->hot_index = idx;
    return idx;
}

void rogue_ui_set_input(RogueUIContext* ctx, const RogueUIInputState* in)
{
    if (!ctx || !in)
        return;
    ctx->input = *in;
    ctx->hot_index = -1;
    if (ctx->replay_recording &&
        ctx->replay_count < (int) (sizeof(ctx->replay_buffer) / sizeof(ctx->replay_buffer[0])))
        ctx->replay_buffer[ctx->replay_count++] = *in;
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
    rogue_ui_internal_assign_id(&n);
    int idx = interactive_push(ctx, &n);
    if (idx < 0)
        return -1;
    int clicked = 0;
    if (ctx->modal_index >= 0 && ctx->modal_index != idx)
        return idx;
    if (ctx->hot_index == idx)
    {
        if (ctx->input.mouse_pressed)
            ctx->active_index = idx;
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
    rogue_ui_internal_assign_id(&n);
    int idx = interactive_push(ctx, &n);
    if (idx < 0)
        return -1;
    if (ctx->modal_index >= 0 && ctx->modal_index != idx)
        return idx;
    if (ctx->hot_index == idx && ctx->input.mouse_pressed)
        ctx->active_index = idx;
    if (ctx->input.mouse_released && (ctx->active_index == idx || ctx->hot_index == idx))
    {
        float mx = ctx->input.mouse_x, my = ctx->input.mouse_y;
        if (rect_contains(&n.rect, mx, my))
        {
            *state = !*state;
            ctx->nodes[idx].color = *state ? on_color : off_color;
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
        max_v = min_v + 1.0f;
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
    rogue_ui_internal_assign_id(&n);
    int idx = interactive_push(ctx, &n);
    if (idx < 0)
        return -1;
    if (ctx->modal_index >= 0 && ctx->modal_index != idx)
        return idx;
    if (ctx->hot_index == idx)
    {
        if (ctx->input.mouse_pressed)
            ctx->active_index = idx;
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
            ctx->active_index = -1;
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
    rogue_ui_internal_assign_id(&n);
    int idx = interactive_push(ctx, &n);
    if (idx < 0)
        return -1;
    int hovered = (ctx->hot_index == idx);
    if (hovered && ctx->input.mouse_pressed)
        ctx->focus_index = idx;
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
                    buffer[len++] = clip[i];
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
                buffer[len - 1] = '\0';
        }
        if (ctx->input.key_tab)
        {
            ctx->focus_index = (idx + 1 < ctx->node_count) ? idx + 1 : 0;
        }
    }
    return idx;
}

/* Layout */
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
    rogue_ui_internal_assign_id(&n);
    return rogue_ui_internal_push_node(ctx, n);
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
    rogue_ui_internal_assign_id(&n);
    return rogue_ui_internal_push_node(ctx, n);
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
    rogue_ui_internal_assign_id(&n);
    return rogue_ui_internal_push_node(ctx, n);
}

/* Scroll */
int rogue_ui_scroll_begin(RogueUIContext* ctx, RogueUIRect r, float content_height)
{
    if (!ctx || !ctx->frame_active)
        return -1;
    if (content_height < r.h)
        content_height = r.h;
    int idx = rogue_ui_panel(ctx, r, 0);
    if (idx < 0)
        return -1;
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
    ctx->nodes[idx].value_max = content_height;
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

/* Tooltip */
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
                rogue_ui_text(ctx, tip, text, text_color);
            }
            return panel;
        }
    }
    else
    {
        if (ctx->last_hover_index == target_index)
            ctx->last_hover_index = -1;
    }
    return -1;
}

/* Clipboard */
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

/* Key repeat config */
void rogue_ui_key_repeat_config(RogueUIContext* ctx, double initial_delay_ms, double interval_ms)
{
    if (!ctx)
        return;
    ctx->key_repeat_initial_ms = initial_delay_ms;
    ctx->key_repeat_interval_ms = interval_ms;
}

/* Chords */
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

/* Replay */
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
    if (!ctx || !ctx->replay_playing)
        return 0;
    if (ctx->replay_cursor >= ctx->replay_count)
    {
        ctx->replay_playing = 0;
        return 0;
    }
    ctx->input = ctx->replay_buffer[ctx->replay_cursor++];
    return 1;
}

/* Modal & controller */
void rogue_ui_set_modal(RogueUIContext* ctx, int modal_index)
{
    if (ctx)
        ctx->modal_index = modal_index;
}
void rogue_ui_set_controller(RogueUIContext* ctx, const RogueUIControllerState* st)
{
    if (ctx && st)
        ctx->controller = *st;
}

/* Reduced motion, narration, focus audit */
void rogue_ui_set_reduced_motion(RogueUIContext* ctx, int enabled)
{
    if (ctx)
        ctx->reduced_motion = enabled ? 1 : 0;
}
int rogue_ui_reduced_motion(const RogueUIContext* ctx) { return ctx ? ctx->reduced_motion : 0; }
void rogue_ui_narrate(RogueUIContext* ctx, const char* text)
{
    if (!ctx)
        return;
    if (!text)
        text = "";
    size_t i = 0;
    for (; i < sizeof(ctx->narration_last) - 1 && text[i]; ++i)
        ctx->narration_last[i] = text[i];
    ctx->narration_last[i] = '\0';
}
const char* rogue_ui_last_narration(const RogueUIContext* ctx)
{
    return ctx ? ctx->narration_last : NULL;
}
void rogue_ui_focus_audit_enable(RogueUIContext* ctx, int enabled)
{
    if (ctx)
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
            RogueUIRect r = ctx->nodes[i].rect;
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
