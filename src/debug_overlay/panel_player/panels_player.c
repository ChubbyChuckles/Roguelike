#include "../../core/app/app_state.h"
#include "../../core/player/player_debug.h"
#include "../overlay_core.h"
#include "../widgets/overlay_widgets.h"

#if ROGUE_ENABLE_DEBUG_OVERLAY

static void panel_player(void* user)
{
    (void) user;
    if (!overlay_begin_panel_auto("player", "Player", 10, 220, 360))
        return;
    int hp = rogue_player_debug_get_health();
    int hp_max = rogue_player_debug_get_max_health();
    if (overlay_slider_int("Health", &hp, 0, hp_max))
        rogue_player_debug_set_health(hp);
    int mp = rogue_player_debug_get_mana();
    int mp_max = rogue_player_debug_get_max_mana();
    if (overlay_slider_int("Mana", &mp, 0, mp_max))
        rogue_player_debug_set_mana(mp);
    int ap = rogue_player_debug_get_ap();
    int ap_max = rogue_player_debug_get_max_ap();
    if (overlay_slider_int("Action Points", &ap, 0, ap_max))
        rogue_player_debug_set_ap(ap);

    if (overlay_columns_begin(2, NULL))
    {
        int v;
        v = rogue_player_debug_get_stat(ROGUE_STAT_STRENGTH);
        if (overlay_slider_int("STR", &v, 1, 200))
            rogue_player_debug_set_stat(ROGUE_STAT_STRENGTH, v);
        overlay_next_column();
        v = rogue_player_debug_get_stat(ROGUE_STAT_DEXTERITY);
        if (overlay_slider_int("DEX", &v, 1, 200))
            rogue_player_debug_set_stat(ROGUE_STAT_DEXTERITY, v);
        overlay_next_column();
        v = rogue_player_debug_get_stat(ROGUE_STAT_VITALITY);
        if (overlay_slider_int("VIT", &v, 1, 300))
            rogue_player_debug_set_stat(ROGUE_STAT_VITALITY, v);
        overlay_next_column();
        v = rogue_player_debug_get_stat(ROGUE_STAT_INTELLIGENCE);
        if (overlay_slider_int("INT", &v, 1, 200))
            rogue_player_debug_set_stat(ROGUE_STAT_INTELLIGENCE, v);
        overlay_columns_end();
    }

    int god = rogue_player_debug_get_god_mode();
    if (overlay_checkbox("God Mode", &god))
        rogue_player_debug_set_god_mode(god);
    int noclip = rogue_player_debug_get_noclip();
    if (overlay_checkbox("No-clip", &noclip))
        rogue_player_debug_set_noclip(noclip);

    if (overlay_button("Teleport to Spawn"))
    {
        rogue_player_debug_teleport(2.5f, 2.5f);
    }
    if (overlay_button("Teleport to Center"))
    {
        float cx = 0.5f * (float) g_app.world_map.width;
        float cy = 0.5f * (float) g_app.world_map.height;
        rogue_player_debug_teleport(cx, cy);
    }

    overlay_end_panel();
}

void rogue_overlay_register_panel_player(void)
{
    overlay_register_panel("player", "Player", panel_player, NULL);
}

#endif
