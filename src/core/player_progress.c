#include "core/player_progress.h"
#include "core/app_state.h"
#include "entities/player.h"
#ifdef ROGUE_HAVE_SDL_MIXER
#include <SDL_mixer.h>
#endif
#include <stdio.h>

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
    /* Autosave every ~5s when dirty */
    static double stats_save_timer = 0.0; stats_save_timer += dt_seconds;
    if(g_app.stats_dirty && stats_save_timer > 5.0){
        FILE* f=NULL;
    #if defined(_MSC_VER)
        fopen_s(&f, "player_stats.cfg", "wb");
    #else
        f=fopen("player_stats.cfg","wb");
    #endif
        if(f){
            fprintf(f,"# Saved player progression\n");
            fprintf(f,"LEVEL=%d\n", g_app.player.level);
            fprintf(f,"XP=%d\n", g_app.player.xp);
            fprintf(f,"XP_TO_NEXT=%d\n", g_app.player.xp_to_next);
            fprintf(f,"STR=%d\n", g_app.player.strength);
            fprintf(f,"DEX=%d\n", g_app.player.dexterity);
            fprintf(f,"VIT=%d\n", g_app.player.vitality);
            fprintf(f,"INT=%d\n", g_app.player.intelligence);
            fprintf(f,"CRITC=%d\n", g_app.player.crit_chance);
            fprintf(f,"CRITD=%d\n", g_app.player.crit_damage);
            fprintf(f,"UNSPENT=%d\n", g_app.unspent_stat_points);
            fprintf(f,"HP=%d\n", g_app.player.health);
            fprintf(f,"MP=%d\n", g_app.player.mana);
            fclose(f);
            g_app.stats_dirty=0;
        }
        stats_save_timer = 0.0;
    }
}
