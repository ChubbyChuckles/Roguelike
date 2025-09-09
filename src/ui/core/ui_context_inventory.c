#include "ui_context.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

/* Helpers forward */
int rogue_ui_panel(RogueUIContext* ctx, RogueUIRect r, uint32_t color);
int rogue_ui_text_dup(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color);
int rogue_ui_text(RogueUIContext* ctx, RogueUIRect r, const char* text, uint32_t color);

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

/* Virtual list helper (inventory depends) */
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
    int first = 0, count = 0, emitted = 0;
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

int rogue_ui_inventory_grid(RogueUIContext* ctx, RogueUIRect rect, const char* id,
                            int slot_capacity, int columns, int* item_ids, int* item_counts,
                            int cell_size, int* first_visible, int* visible_count)
{
    if (!ctx || !ctx->frame_active || slot_capacity <= 0 || columns <= 0)
        return -1;
    if (ctx->node_capacity == 0)
    {
        fprintf(stderr, "FATAL: node_capacity==0 (corrupted context)\n");
        return -1;
    }
    static int dbg_counter = 0;
    dbg_counter++;
    if (cell_size <= 0)
        cell_size = 32;
    if (columns > slot_capacity)
        columns = slot_capacity;
    int root = rogue_ui_panel(ctx, rect, ctx->theme.panel_bg_color);
    (void) id;
    static int s_scroll_row = 0;
    if (ctx->input.wheel_delta > 0)
        s_scroll_row--;
    else if (ctx->input.wheel_delta < 0)
        s_scroll_row++;
    if (s_scroll_row < 0)
        s_scroll_row = 0;
    float spacing = 2.0f;
    float pad = 2.0f;
    int item_pitch = cell_size + (int) spacing;
    int total_rows = (slot_capacity + columns - 1) / columns;
    int max_row = total_rows - 1;
    if (max_row < 0)
        max_row = 0;
    if (s_scroll_row > max_row)
        s_scroll_row = max_row;
    int view_height = (int) rect.h;
    int scroll_offset = s_scroll_row * item_pitch;
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
            int rarity = item_ids[s] % 5;
            unsigned char rr = 240, rg = 210, rb = 60;
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
            RogueUIRect outer = cell_r;
            RogueUIRect inner = {cell_r.x + 1, cell_r.y + 1, cell_r.w - 2, cell_r.h - 2};
            rogue_ui_panel(ctx, outer, (uint32_t) ((rr << 24) | (rg << 16) | (rb << 8) | 0xFFu));
            rogue_ui_panel(ctx, inner, base_col);
            /* Inclusive bounds on right/bottom edges to avoid off-by-one misses */
            if (mx >= cell_r.x && my >= cell_r.y && mx <= cell_r.x + cell_r.w &&
                my <= cell_r.y + cell_r.h)
                hovered_slot = s;
            if (item_counts)
            {
                char tmp[32];
                snprintf(tmp, sizeof tmp, "%d", item_counts[s]);
                rogue_ui_text_dup(ctx,
                                  (RogueUIRect){inner.x + 2, inner.y + 2, inner.w - 4, inner.h - 4},
                                  tmp, (uint32_t) ((rr << 24) | (rg << 16) | (rb << 8) | 0xFFu));
            }
        }
        else
        {
            rogue_ui_panel(ctx, cell_r, base_col);
            if (mx >= cell_r.x && my >= cell_r.y && mx <= cell_r.x + cell_r.w &&
                my <= cell_r.y + cell_r.h)
                hovered_slot = s;
        }
    }
    if (!ctx->drag_active && hovered_slot >= 0 && ctx->input.mouse_pressed && item_ids &&
        item_ids[hovered_slot])
    {
        ctx->drag_active = 1;
        ctx->drag_from_slot = hovered_slot;
        ctx->drag_item_id = item_ids[hovered_slot];
        ctx->drag_item_count = item_counts ? item_counts[hovered_slot] : 1;
        ui_enqueue(ctx, ROGUE_UI_EVENT_DRAG_BEGIN, hovered_slot, ctx->drag_item_id,
                   ctx->drag_item_count);
    }
    if (ctx->drag_active && ctx->input.mouse_released)
    {
        int target = hovered_slot >= 0 ? hovered_slot : ctx->drag_from_slot;
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
    /* TEMP debug: report hover on right-click even if blocked */

    if (!ctx->ctx_menu_active && hovered_slot >= 0 && ctx->input.mouse2_pressed && item_ids &&
        item_ids[hovered_slot])
    {
        /* TEMP debug: trace context open conditions */
        ctx->ctx_menu_active = 1;
        ctx->ctx_menu_slot = hovered_slot;
        ctx->ctx_menu_selection = 0;
        ui_enqueue(ctx, ROGUE_UI_EVENT_CONTEXT_OPEN, hovered_slot, 0, 0);
    }
    static const char* menu_items[] = {"Equip", "Salvage", "Compare", "Cancel"};
    int menu_count = (int) (sizeof(menu_items) / sizeof(menu_items[0]));
    if (ctx->ctx_menu_active)
    {
        if (ctx->input.key_down)
            ctx->ctx_menu_selection = (ctx->ctx_menu_selection + 1) % menu_count;
        if (ctx->input.key_up)
            ctx->ctx_menu_selection = (ctx->ctx_menu_selection - 1 + menu_count) % menu_count;
        if (ctx->input.key_activate)
        {
            int sel = ctx->ctx_menu_selection;
            if (sel == menu_count - 1)
                ui_enqueue(ctx, ROGUE_UI_EVENT_CONTEXT_CANCEL, ctx->ctx_menu_slot, 0, 0);
            else
                ui_enqueue(ctx, ROGUE_UI_EVENT_CONTEXT_SELECT, ctx->ctx_menu_slot, sel, 0);
            ctx->ctx_menu_active = 0;
        }
        else if (ctx->input.mouse_pressed && !ctx->input.mouse2_pressed)
        {
            ui_enqueue(ctx, ROGUE_UI_EVENT_CONTEXT_CANCEL, ctx->ctx_menu_slot, 0, 0);
            ctx->ctx_menu_active = 0;
        }
        RogueUIRect mrect = {rect.x + rect.w + 8, rect.y + 16, 100, (float) (menu_count * 16 + 4)};
        rogue_ui_panel(ctx, mrect, 0x202028FFu);
        for (int i = 0; i < menu_count; i++)
        {
            uint32_t col = (i == ctx->ctx_menu_selection) ? 0x5050A0FFu : 0x303038FFu;
            RogueUIRect ir = {mrect.x + 2, mrect.y + 2 + i * 16, mrect.w - 4, 14};
            rogue_ui_panel(ctx, ir, col);
            rogue_ui_text(ctx, (RogueUIRect){ir.x + 2, ir.y, ir.w - 4, ir.h}, menu_items[i],
                          ctx->theme.text_color);
        }
    }
    if (!ctx->stack_split_active && ctx->input.key_ctrl && hovered_slot >= 0 &&
        hovered_slot < slot_capacity && ctx->input.mouse_pressed && item_ids &&
        item_ids[hovered_slot] && item_counts && item_counts[hovered_slot] > 1)
    {
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
                        ui_enqueue(ctx, ROGUE_UI_EVENT_STACK_SPLIT_APPLY, from, i, move);
                        break;
                    }
                }
            }
            ctx->stack_split_active = 0;
        }
        else if (ctx->input.mouse_released && ctx->input.mouse_down == 0)
        {
            ui_enqueue(ctx, ROGUE_UI_EVENT_STACK_SPLIT_CANCEL, ctx->stack_split_from_slot, 0, 0);
            ctx->stack_split_active = 0;
        }
        RogueUIRect m = {rect.x + rect.w + 8, rect.y, 120, 48};
        rogue_ui_panel(ctx, m, 0x404048FFu);
        char tmp[32];
        snprintf(tmp, sizeof tmp, "Split %d/%d", ctx->stack_split_value, ctx->stack_split_total);
        rogue_ui_text_dup(ctx, (RogueUIRect){m.x + 4, m.y + 4, m.w - 8, 16}, tmp,
                          ctx->theme.text_color);
    }
    int show_preview = (hovered_slot >= 0 && item_ids && item_ids[hovered_slot]);
    if (show_preview)
    {
        int cur_slot = hovered_slot;
        if (ctx->stat_preview_slot != cur_slot)
        {
            ui_enqueue(ctx, ROGUE_UI_EVENT_STAT_PREVIEW_SHOW, cur_slot, 0, 0);
            ctx->stat_preview_slot = cur_slot;
        }
        int item_id = item_ids[cur_slot];
        int dmg = item_id % 100;
        int prev_item_id = (ctx->drag_active && ctx->drag_from_slot >= 0)
                               ? item_ids[ctx->drag_from_slot]
                               : item_id;
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
        rogue_ui_panel(ctx, pr, 0x202028FFu);
        rogue_ui_text_dup(ctx, (RogueUIRect){pr.x + 4, pr.y + 2, pr.w - 8, pr.h - 4}, line, col);
    }
    else if (ctx->stat_preview_slot != -1)
    {
        ui_enqueue(ctx, ROGUE_UI_EVENT_STAT_PREVIEW_HIDE, ctx->stat_preview_slot, 0, 0);
        ctx->stat_preview_slot = -1;
    }
    return root;
}
