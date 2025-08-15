#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include <assert.h>
#include <stdio.h>
#include "game/combat.h"

/* Stubs */
RoguePlayer g_exposed_player_for_stats = {0};
int rogue_get_current_attack_frame(void){ return 3; }
void rogue_app_add_hitstop(float ms){ (void)ms; }
void rogue_add_damage_number(float x,float y,int amount,int from_player){ (void)x;(void)y;(void)amount;(void)from_player; }
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){ (void)x;(void)y;(void)amount;(void)from_player;(void)crit; }

static void setup_enemy(RogueEnemy* e,float x){ e->alive=1; e->base.pos.x=x; e->base.pos.y=0; e->health=500; e->max_health=500; }

int main(){
    RoguePlayer player; rogue_player_init(&player); player.strength=50; player.base.pos.x=0; player.base.pos.y=0; player.facing=2;
    RoguePlayerCombat combat; rogue_combat_init(&combat);
    /* Use heavy_2 overlapping windows to test boundary inclusive/exclusive semantics */
    combat.archetype = ROGUE_WEAPON_HEAVY; combat.chain_index=1; combat.phase=ROGUE_ATTACK_STRIKE; combat.processed_window_mask=0; combat.emitted_events_mask=0; combat.event_count=0;
    const RogueAttackDef* def = rogue_attack_get(combat.archetype, combat.chain_index); assert(def && def->num_windows==3);
    RogueEnemy enemy; setup_enemy(&enemy,1.0f);

    /* Just before first window start (negative time) expect no damage */
    combat.strike_time_ms = -0.1f; int hp = enemy.health; rogue_combat_player_strike(&combat,&player,&enemy,1); assert(enemy.health==hp);
    /* At first window start (0) -> damage */
    combat.strike_time_ms = 0.0f; rogue_combat_player_strike(&combat,&player,&enemy,1); int after_w0 = enemy.health; assert(after_w0 < hp);
    /* At exact overlap boundary (40ms) both window0 (still active) & window1 (starts) but should only process new window1 since window0 already processed; damage increases. */
    combat.strike_time_ms = 40.0f; int prev = enemy.health; rogue_combat_player_strike(&combat,&player,&enemy,1); assert(enemy.health < prev);
    int after_w1 = enemy.health;
    /* At window0 end (50ms) and inside window1; should not reapply window0; window2 not yet active */
    combat.strike_time_ms = 50.0f; prev = enemy.health; rogue_combat_player_strike(&combat,&player,&enemy,1); assert(enemy.health == prev);
    /* At window2 start 80ms -> new damage */
    combat.strike_time_ms = 80.0f; prev = enemy.health; rogue_combat_player_strike(&combat,&player,&enemy,1); assert(enemy.health < prev);
    int after_w2 = enemy.health;
    /* Re-enter already processed windows region (90ms within windows2) no extra damage */
    combat.strike_time_ms = 90.0f; prev = enemy.health; rogue_combat_player_strike(&combat,&player,&enemy,1); assert(enemy.health == prev);
    /* After strike end (>=105) no damage */
    combat.strike_time_ms = 106.0f; prev = enemy.health; rogue_combat_player_strike(&combat,&player,&enemy,1); assert(enemy.health == prev);

    printf("combat_window_boundary: OK dmg_seq=(w0:%d w1:%d w2:%d)\n", (hp-after_w0),(after_w0-after_w1),(after_w1-after_w2));
    return 0;
}
