#include "player_debug.h"
#include "../../entities/player.h"
#include "../app/app_state.h"

int rogue_player_debug_get_health(void) { return g_app.player.health; }
int rogue_player_debug_get_max_health(void) { return g_app.player.max_health; }
void rogue_player_debug_set_health(int v)
{
    if (v < 0)
        v = 0;
    if (v > g_app.player.max_health)
        v = g_app.player.max_health;
    g_app.player.health = v;
}
int rogue_player_debug_get_mana(void) { return g_app.player.mana; }
int rogue_player_debug_get_max_mana(void) { return g_app.player.max_mana; }
void rogue_player_debug_set_mana(int v)
{
    if (v < 0)
        v = 0;
    if (v > g_app.player.max_mana)
        v = g_app.player.max_mana;
    g_app.player.mana = v;
}
int rogue_player_debug_get_ap(void) { return g_app.player.action_points; }
int rogue_player_debug_get_max_ap(void) { return g_app.player.max_action_points; }
void rogue_player_debug_set_ap(int v)
{
    if (v < 0)
        v = 0;
    if (v > g_app.player.max_action_points)
        v = g_app.player.max_action_points;
    g_app.player.action_points = v;
}

int rogue_player_debug_get_stat(RoguePlayerStatKey k)
{
    switch (k)
    {
    case ROGUE_STAT_STRENGTH:
        return g_app.player.strength;
    case ROGUE_STAT_DEXTERITY:
        return g_app.player.dexterity;
    case ROGUE_STAT_VITALITY:
        return g_app.player.vitality;
    case ROGUE_STAT_INTELLIGENCE:
        return g_app.player.intelligence;
    default:
        return 0;
    }
}

void rogue_player_debug_set_stat(RoguePlayerStatKey k, int v)
{
    if (v < 0)
        v = 0;
    switch (k)
    {
    case ROGUE_STAT_STRENGTH:
        g_app.player.strength = v;
        break;
    case ROGUE_STAT_DEXTERITY:
        g_app.player.dexterity = v;
        break;
    case ROGUE_STAT_VITALITY:
        g_app.player.vitality = v;
        break;
    case ROGUE_STAT_INTELLIGENCE:
        g_app.player.intelligence = v;
        break;
    default:
        break;
    }
    /* Recompute derived to keep caps and bars consistent */
    rogue_player_recalc_derived(&g_app.player);
}

int rogue_player_debug_get_god_mode(void) { return g_app.god_mode_enabled; }
void rogue_player_debug_set_god_mode(int enabled) { g_app.god_mode_enabled = enabled ? 1 : 0; }
int rogue_player_debug_get_noclip(void) { return g_app.noclip_enabled; }
void rogue_player_debug_set_noclip(int enabled) { g_app.noclip_enabled = enabled ? 1 : 0; }

void rogue_player_debug_teleport(float x, float y)
{
    g_app.player.base.pos.x = x;
    g_app.player.base.pos.y = y;
}
