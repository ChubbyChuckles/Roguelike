#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include <assert.h>
#include <stdio.h>
#include "game/combat.h"

RoguePlayer g_exposed_player_for_stats = {0};
int rogue_get_current_attack_frame(void){ return 3; }
void rogue_app_add_hitstop(float ms){ (void)ms; }
void rogue_add_damage_number(float x,float y,int amount,int from_player){ (void)x;(void)y;(void)amount;(void)from_player; }
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){ (void)x;(void)y;(void)amount;(void)from_player;(void)crit; }

static void simulate_ms(RoguePlayerCombat* c, float ms){ float acc=0; while(acc<ms){ float step=5.0f; if(acc+step>ms) step=ms-acc; rogue_combat_update_player(c, step, 0); acc+=step; } }

int main(){
    RoguePlayerCombat combat; rogue_combat_init(&combat);
    RoguePlayer player; rogue_player_init(&player); player.strength=40; player.dexterity=20; player.intelligence=10; player.base.pos.x=0; player.base.pos.y=0; player.facing=2;
    RogueEnemy enemies[1]; enemies[0].alive=1; enemies[0].base.pos.x=0.8f; enemies[0].base.pos.y=0; enemies[0].health=500; enemies[0].max_health=500;
    /* Advance to light_3 (multi-hit) */
    /* light_1 durations: 110 + 70 + 120 = 300 */
    rogue_combat_update_player(&combat,0,1); simulate_ms(&combat, 310);
    /* light_2 durations: 95 + 65 + 115 = 275 */
    rogue_combat_update_player(&combat,0,1); simulate_ms(&combat, 285);
    rogue_combat_update_player(&combat,0,1); /* attempt to start next, but enforce chain manually for test */
    combat.chain_index = 2; /* light_3 multi-hit definition */
    combat.archetype = ROGUE_WEAPON_LIGHT;
    combat.phase = ROGUE_ATTACK_STRIKE; combat.processed_window_mask = 0; combat.strike_time_ms = 10.0f;
    int health_before = enemies[0].health;
    rogue_combat_player_strike(&combat,&player,enemies,1); /* first window */
    int after_first = enemies[0].health;
    assert(after_first < health_before);
    combat.strike_time_ms = 45.0f; /* inside second window */
    rogue_combat_player_strike(&combat,&player,enemies,1);
    int after_second = enemies[0].health;
    assert(after_second < after_first); /* second hit applied */
    /* Duplicate prevention: invoke again at same window */
    int before_dup = enemies[0].health;
    rogue_combat_player_strike(&combat,&player,enemies,1);
    assert(enemies[0].health == before_dup);
    printf("combat_multi_hit: OK (initial=%d first=%d second=%d final=%d)\n", health_before, after_first, after_second, enemies[0].health);
    return 0;
}
