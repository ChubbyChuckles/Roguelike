#include "save_internal.h"
#include <stdlib.h> /* free */

static int g_compress_enabled = 0;
static int g_compress_min_bytes = 64;

void rogue_save_last_section_reuse(unsigned* reused, unsigned* written)
{
    if (reused)
        *reused = g_save_last_sections_reused;
    if (written)
        *written = g_save_last_sections_written;
}

int rogue_save_component_is_dirty(int component_id)
{
    if (component_id <= 0 || component_id >= 32)
        return -1;
    return (g_save_dirty_mask & (1u << component_id)) ? 1 : 0;
}

int rogue_save_set_incremental(int enabled)
{
    g_save_incremental_enabled = enabled ? 1 : 0;
    if (!g_save_incremental_enabled)
    {
        for (int i = 0; i < ROGUE_SAVE_MAX_COMPONENTS; i++)
        {
            free(g_save_cached_sections[i].data);
            g_save_cached_sections[i].data = NULL;
            g_save_cached_sections[i].valid = 0;
        }
        g_save_dirty_mask = 0xFFFFFFFFu;
    }
    return 0;
}

int rogue_save_mark_component_dirty(int component_id)
{
    if (component_id <= 0 || component_id >= 32)
        return -1;
    g_save_dirty_mask |= (1u << component_id);
    return 0;
}

int rogue_save_mark_all_dirty(void)
{
    g_save_dirty_mask = 0xFFFFFFFFu;
    return 0;
}

int rogue_save_set_compression(int enabled, int min_bytes)
{
    g_compress_enabled = enabled ? 1 : 0;
    if (min_bytes > 0)
        g_compress_min_bytes = min_bytes;
    return 0;
}

/* Accessors for IO module */
int rogue__compress_enabled(void) { return g_compress_enabled; }
int rogue__compress_min_bytes(void) { return g_compress_min_bytes; }
