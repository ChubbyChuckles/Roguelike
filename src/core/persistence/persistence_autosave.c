#include "persistence_autosave.h"
#include "../app/app_state.h"
#include "persistence.h"

static double s_stats_save_timer = 0.0;

void rogue_persistence_autosave_update(double dt_seconds)
{
    s_stats_save_timer += dt_seconds;
    if (g_app.stats_dirty && s_stats_save_timer > 5.0)
    {
        rogue_persistence_save_player_stats();
        s_stats_save_timer = 0.0;
    }
}

void rogue_persistence_autosave_force(void)
{
    if (g_app.stats_dirty)
    {
        rogue_persistence_save_player_stats();
        s_stats_save_timer = 0.0;
    }
}
