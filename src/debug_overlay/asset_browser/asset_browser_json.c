#include "asset_browser_json.h"
#include <string.h>

#define AB_UNDO_STACK_CAP                                                                          \
    (int) (sizeof g_ab_state.json_undo_stack / sizeof g_ab_state.json_undo_stack[0])

/* Access the global state (macro mirrors panels file) */
#define g_ab_state (*rogue_asset_browser_state())

void rogue_asset_browser_json_undo_init(void)
{
    g_ab_state.json_undo_len = 0;
    g_ab_state.json_undo_pos = -1;
    if (g_ab_state.json_editor_buffer[0])
    {
        strncpy(g_ab_state.json_undo_stack[0], g_ab_state.json_editor_buffer,
                sizeof g_ab_state.json_undo_stack[0] - 1);
        g_ab_state.json_undo_stack[0][sizeof g_ab_state.json_undo_stack[0] - 1] = '\0';
        g_ab_state.json_undo_len = 1;
        g_ab_state.json_undo_pos = 0;
    }
}

void rogue_asset_browser_json_undo_push_current(void)
{
    /* Trim redo segment if we've undone */
    if (g_ab_state.json_undo_pos >= 0 && g_ab_state.json_undo_pos < g_ab_state.json_undo_len - 1)
        g_ab_state.json_undo_len = g_ab_state.json_undo_pos + 1;
    /* Full? shift left */
    if (g_ab_state.json_undo_len == AB_UNDO_STACK_CAP)
    {
        for (int i = 1; i < g_ab_state.json_undo_len; ++i)
            memcpy(g_ab_state.json_undo_stack[i - 1], g_ab_state.json_undo_stack[i],
                   sizeof g_ab_state.json_undo_stack[i]);
        g_ab_state.json_undo_len--;
        if (g_ab_state.json_undo_pos > 0)
            g_ab_state.json_undo_pos--;
    }
    strncpy(g_ab_state.json_undo_stack[g_ab_state.json_undo_len], g_ab_state.json_editor_buffer,
            sizeof g_ab_state.json_undo_stack[0] - 1);
    g_ab_state.json_undo_stack[g_ab_state.json_undo_len][sizeof g_ab_state.json_undo_stack[0] - 1] =
        '\0';
    g_ab_state.json_undo_len++;
    g_ab_state.json_undo_pos = g_ab_state.json_undo_len - 1;
}

static void ab_apply_pos(void)
{
    if (g_ab_state.json_undo_pos >= 0 && g_ab_state.json_undo_pos < g_ab_state.json_undo_len)
    {
        strncpy(g_ab_state.json_editor_buffer, g_ab_state.json_undo_stack[g_ab_state.json_undo_pos],
                sizeof g_ab_state.json_editor_buffer - 1);
        g_ab_state.json_editor_buffer[sizeof g_ab_state.json_editor_buffer - 1] = '\0';
        g_ab_state.json_editor_dirty = 1;
    }
}

int rogue_asset_browser_json_undo_can_undo(void) { return g_ab_state.json_undo_pos > 0; }
int rogue_asset_browser_json_undo_can_redo(void)
{
    return g_ab_state.json_undo_pos >= 0 && g_ab_state.json_undo_pos < g_ab_state.json_undo_len - 1;
}
void rogue_asset_browser_json_undo_do_undo(void)
{
    if (rogue_asset_browser_json_undo_can_undo())
    {
        g_ab_state.json_undo_pos--;
        ab_apply_pos();
    }
}
void rogue_asset_browser_json_undo_do_redo(void)
{
    if (rogue_asset_browser_json_undo_can_redo())
    {
        g_ab_state.json_undo_pos++;
        ab_apply_pos();
    }
}
