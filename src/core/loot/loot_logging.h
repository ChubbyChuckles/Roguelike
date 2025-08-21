// Loot logging verbosity control (Phase 6.2)
#ifndef ROGUE_LOOT_LOGGING_H
#define ROGUE_LOOT_LOGGING_H

#include "util/log.h"

/* Levels:
   0 = silent (no loot logs)
   1 = info (INFO + WARN/ERROR)
   2 = debug (DEBUG + INFO + WARN/ERROR)
*/
extern int g_rogue_loot_log_level;

static inline void rogue_loot_log_set_level(int lvl)
{
    if (lvl < 0)
        lvl = 0;
    if (lvl > 2)
        lvl = 2;
    g_rogue_loot_log_level = lvl;
}
static inline int rogue_loot_log_level(void) { return g_rogue_loot_log_level; }
void rogue_loot_logging_init_from_env(void); /* Reads ROGUE_LOOT_LOG env (optional) */

/* Gated macros (avoid evaluating format if below level) */
#define ROGUE_LOOT_LOG_DEBUG(fmt, ...)                                                             \
    do                                                                                             \
    {                                                                                              \
        if (g_rogue_loot_log_level >= 2)                                                           \
        {                                                                                          \
            ROGUE_LOG_DEBUG(fmt, ##__VA_ARGS__);                                                   \
        }                                                                                          \
    } while (0)
#define ROGUE_LOOT_LOG_INFO(fmt, ...)                                                              \
    do                                                                                             \
    {                                                                                              \
        if (g_rogue_loot_log_level >= 1)                                                           \
        {                                                                                          \
            ROGUE_LOG_INFO(fmt, ##__VA_ARGS__);                                                    \
        }                                                                                          \
    } while (0)

#endif
