#include "loot_logging.h"
#include <stdlib.h>

int g_rogue_loot_log_level = 1; /* default INFO */

void rogue_loot_logging_init_from_env(void)
{
#if defined(_WIN32)
    char* buf = NULL;
    size_t sz = 0;
    int r = _dupenv_s(&buf, &sz, "ROGUE_LOOT_LOG");
    if (r == 0 && buf)
    {
        int lvl = atoi(buf);
        if (lvl < 0)
            lvl = 0;
        if (lvl > 2)
            lvl = 2;
        g_rogue_loot_log_level = lvl;
        free(buf);
    }
#else
    const char* v = getenv("ROGUE_LOOT_LOG");
    if (v && *v)
    {
        int lvl = atoi(v);
        if (lvl < 0)
            lvl = 0;
        if (lvl > 2)
            lvl = 2;
        g_rogue_loot_log_level = lvl;
    }
#endif
}
