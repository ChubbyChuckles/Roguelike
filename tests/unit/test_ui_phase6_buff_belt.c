#define SDL_MAIN_HANDLED 1
#include <stdio.h>
#include "core/buffs.h"
#include "core/hud_buff_belt.h"
#include "core/app_state.h"

/* Minimal g_app stub fields used */
RogueAppState g_app; RoguePlayer g_exposed_player_for_stats;

int main(){
    g_app.game_time_ms = 0.0; rogue_buffs_init();
    double now=0.0; /* apply two buffs */
    rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE, 5, 5000.0, now);
    rogue_buffs_apply(ROGUE_BUFF_STAT_STRENGTH, 3, 3000.0, now);
    RogueHUDBuffBeltState belt={0};
    rogue_hud_buff_belt_refresh(&belt, now);
    if(belt.count != 2){ printf("FAIL expected 2 buffs got %d\n", belt.count); return 1; }
    if(belt.icons[0].magnitude + belt.icons[1].magnitude != 8){ printf("FAIL magnitudes mismatch\n"); return 1; }
    /* Advance time past first buff only */
    now = 3200.0; g_app.game_time_ms = now; rogue_buffs_update(now); rogue_hud_buff_belt_refresh(&belt, now);
    if(belt.count != 1){ printf("FAIL expected 1 buff after expiry got %d\n", belt.count); return 1; }
    if(belt.icons[0].type != ROGUE_BUFF_POWER_STRIKE){ printf("FAIL remaining buff wrong type %d\n", belt.icons[0].type); return 1; }
    printf("test_ui_phase6_buff_belt: OK\n");
    return 0;
}
