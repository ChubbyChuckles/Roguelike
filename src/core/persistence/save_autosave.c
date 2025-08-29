#include "save_internal.h"
#include "save_paths.h"
#include <stdio.h>

int rogue_save_set_autosave_interval_ms(int ms)
{
    g_save_autosave_interval_ms = ms;
    return 0;
}

uint32_t rogue_save_autosave_count(void) { return g_save_autosave_count; }
int rogue_save_last_save_rc(void) { return g_save_last_rc; }
uint32_t rogue_save_last_save_bytes(void) { return g_save_last_bytes; }
double rogue_save_last_save_ms(void) { return g_save_last_ms; }

int rogue_save_set_autosave_throttle_ms(int ms)
{
    g_save_autosave_throttle_ms = ms;
    return 0;
}

int rogue_save_status_string(char* buf, size_t cap)
{
    if (!buf || cap == 0)
        return -1;
    int n = snprintf(buf, cap, "save rc=%d bytes=%u ms=%.2f autosaves=%u interval=%d throttle=%d",
                     g_save_last_rc, g_save_last_bytes, g_save_last_ms, g_save_autosave_count,
                     g_save_autosave_interval_ms, g_save_autosave_throttle_ms);
    return (n < 0 || (size_t) n >= cap) ? -1 : 0;
}

/* forward to IO module */
int rogue_save_manager_autosave(int slot_index);

int rogue_save_manager_update(uint32_t now_ms, int in_combat)
{
    static uint32_t g_last_any_save_time = 0;
    if (g_save_autosave_interval_ms <= 0)
        return 0;
    if (in_combat)
        return 0;
    static uint32_t g_last_autosave_time = 0;
    if (g_last_autosave_time == 0)
        g_last_autosave_time = now_ms;
    if (now_ms - g_last_autosave_time >= (uint32_t) g_save_autosave_interval_ms)
    {
        if (g_last_any_save_time != 0 && g_save_autosave_throttle_ms > 0 &&
            now_ms - g_last_any_save_time < (uint32_t) g_save_autosave_throttle_ms)
        {
            return 0;
        }
        int rc = rogue_save_manager_autosave(g_save_autosave_count);
        if (rc == 0)
            g_save_autosave_count++;
        g_last_autosave_time = now_ms;
        g_last_any_save_time = now_ms;
        return rc;
    }
    return 0;
}
