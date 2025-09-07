/* Reformatted multiselect bits, list editor, vec2 input */
#include "../../overlay_input.h"
#include "../../overlay_theme.h"
#include "../../overlay_tooltip.h"
#include "../overlay_widgets_internal.h"
#include "controls_shared.h"
#include <stdio.h>
#include <string.h>
#ifdef ROGUE_HAVE_SDL
#include <SDL.h>
#endif
#if ROGUE_ENABLE_DEBUG_OVERLAY
#include "../../../core/app/app_state.h"
#include "../../../graphics/font.h"

int overlay_multiselect_bits(const char* label_prefix, const char* const* items, int count,
                             unsigned int* mask)
{
    if (!g_ui.panel_active || !items || !mask || count <= 0 || count > 32)
        return 0;
    char head[128];
    snprintf(head, sizeof head, "%s:", label_prefix ? label_prefix : "Bits");
    overlay_label(head);
    ui_next_line();
    int changed = 0;
    for (int i = 0; i < count; ++i)
    {
        unsigned int bit = 1u << i;
        char line[128];
        snprintf(line, sizeof line, "[%c] %s", ((*mask & bit) ? 'x' : ' '),
                 items[i] ? items[i] : "?");
        if (overlay_button(line))
        {
            if (*mask & bit)
                *mask &= ~bit;
            else
                *mask |= bit;
            changed = 1;
        }
    }
    return changed;
}

int overlay_list_editor(const char* label, char entries[][64], int* count, int capacity,
                        int elem_size)
{
    (void) elem_size; /* fixed 64 now */
    if (!g_ui.panel_active || !entries || !count || capacity <= 0)
        return 0;
    if (*count > capacity)
        *count = capacity;
    int changed = 0;
    char head[128];
    snprintf(head, sizeof head, "%s (%d/%d)", label ? label : "List", *count, capacity);
    overlay_label(head);
    ui_next_line();
    for (int i = 0; i < *count; ++i)
    {
        char row[96];
        snprintf(row, sizeof row, "%d: %s", i, entries[i]);
        overlay_label(row);
        if (overlay_button("Del"))
        {
            for (int j = i; j < *count - 1; ++j)
                memcpy(entries[j], entries[j + 1], 64);
            --(*count);
            changed = 1;
            break;
        }
        ui_next_line();
    }
    if (*count < capacity)
    {
        if (overlay_button("Add"))
        {
            snprintf(entries[*count], 64, "item_%d", *count);
            ++(*count);
            changed = 1;
        }
    }
    return changed;
}

int overlay_input_vec2(const char* label, float* x, float* y, float minv, float maxv)
{
    if (!g_ui.panel_active || !x || !y)
        return 0;
    int changed = 0;
    char buf[96];
    snprintf(buf, sizeof buf, "%s X: %.3f", label ? label : "Vec2", *x);
    overlay_label(buf);
    if (overlay_button("+X"))
    {
        *x += (maxv - minv) * 0.01f;
        if (*x > maxv)
            *x = maxv;
        changed = 1;
    }
    if (overlay_button("-X"))
    {
        *x -= (maxv - minv) * 0.01f;
        if (*x < minv)
            *x = minv;
        changed = 1;
    }
    ui_next_line();
    snprintf(buf, sizeof buf, "%s Y: %.3f", label ? label : "Vec2", *y);
    overlay_label(buf);
    if (overlay_button("+Y"))
    {
        *y += (maxv - minv) * 0.01f;
        if (*y > maxv)
            *y = maxv;
        changed = 1;
    }
    if (overlay_button("-Y"))
    {
        *y -= (maxv - minv) * 0.01f;
        if (*y < minv)
            *y = minv;
        changed = 1;
    }
    ui_next_line();
    return changed;
}
#endif /* ROGUE_ENABLE_DEBUG_OVERLAY */
