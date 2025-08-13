#include "core/player_progress.h"
#include "core/app_state.h"
#include "entities/player.h"
#ifdef ROGUE_HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif
#include <stdio.h>
#include "core/persistence.h"

void rogue_player_progress_update(double dt_seconds){
    float raw_dt_ms = (float)dt_seconds * 1000.0f;
    /* Level-up loop */
    while(g_app.player.xp >= g_app.player.xp_to_next){
        g_app.player.xp -= g_app.player.xp_to_next;
        g_app.player.level++;
        g_app.unspent_stat_points += 3;
        g_app.player.xp_to_next = (int)(g_app.player.xp_to_next * 1.35f + 15);
        rogue_player_recalc_derived(&g_app.player);
        g_app.player.health = g_app.player.max_health;
        g_app.player.mana = g_app.player.max_mana;
        g_app.levelup_aura_timer_ms = 2000.0f;
    #ifdef ROGUE_HAVE_SDL_MIXER
        if(g_app.sfx_levelup){ Mix_PlayChannel(-1, g_app.sfx_levelup, 0); }
    #endif
    g_app.stats_dirty = 1;
    /* Immediate save on each level-up to ensure persistence even if player quits before autosave interval */
    rogue_persistence_save_player_stats();
    }
    g_app.difficulty_scalar = 1.0 + (double)g_app.player.level * 0.15 + (double)g_app.total_kills * 0.002;
    /* Passive regen */
    g_app.time_since_player_hit_ms += raw_dt_ms;
    if(g_app.player.health > 0 && g_app.player.health < g_app.player.max_health){
        if(g_app.time_since_player_hit_ms > 4000.0f){
            g_app.health_regen_accum_ms += raw_dt_ms;
            float interval = 900.0f - (g_app.player.vitality * 4.0f); if(interval < 250.0f) interval = 250.0f;
            while(g_app.health_regen_accum_ms >= interval){
                g_app.health_regen_accum_ms -= interval;
                g_app.player.health += 1 + g_app.player.vitality/25; if(g_app.player.health>g_app.player.max_health) g_app.player.health=g_app.player.max_health;
            }
        }
    } else { g_app.health_regen_accum_ms = 0.0f; }
    if(g_app.player.mana < g_app.player.max_mana){
        g_app.mana_regen_accum_ms += raw_dt_ms;
        float interval_mp = 520.0f - g_app.player.intelligence * 6.5f; if(interval_mp < 120.0f) interval_mp = 120.0f;
        if(g_app.time_since_player_hit_ms > 4000.0f) interval_mp *= 0.85f;
        while(g_app.mana_regen_accum_ms >= interval_mp){
            g_app.mana_regen_accum_ms -= interval_mp;
            g_app.player.mana += 1 + g_app.player.intelligence/12; if(g_app.player.mana>g_app.player.max_mana) g_app.player.mana=g_app.player.max_mana;
        }
    } else { g_app.mana_regen_accum_ms = 0.0f; }
    /* Autosave moved to persistence_autosave module */
}
