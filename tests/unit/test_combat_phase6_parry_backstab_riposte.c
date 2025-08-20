#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "game/combat.h"
#include "entities/player.h"
#include "entities/enemy.h"

extern int rogue_force_attack_active; extern int g_attack_frame_override; static int rogue_get_current_attack_frame(void){ return 3; }
static int rogue_buffs_get_total(int t){(void)t; return 0;}
static void rogue_add_damage_number(float x,float y,int amount,int from_player){(void)x;(void)y;(void)amount;(void)from_player;}
static void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){(void)x;(void)y;(void)amount;(void)from_player;(void)crit;}
/* Minimal attack def reused */
static const RogueAttackDef g_test_attack = {0,"light",ROGUE_WEAPON_LIGHT,0,0,60,0,5,0,15,ROGUE_DMG_PHYSICAL,0.0f,0,0,1,{{0,60,ROGUE_CANCEL_ON_HIT,1.0f,0,0,0,0}},0,ROGUE_CANCEL_ON_HIT,0.40f,0,0};
const RogueAttackDef* rogue_attack_get(int a,int b){(void)a;(void)b; return &g_test_attack;} int rogue_attack_chain_length(int a){(void)a; return 1;}

static int do_strike(RoguePlayerCombat* pc, RoguePlayer* player, RogueEnemy* enemy){ pc->phase=ROGUE_ATTACK_STRIKE; pc->strike_time_ms=10; pc->processed_window_mask=0; pc->emitted_events_mask=0; return rogue_combat_player_strike(pc,player,enemy,1); }

int main(){
    printf("BEGIN phase6_parry_backstab_riposte test\n");
    rogue_force_attack_active=1; g_attack_frame_override=3;
    RoguePlayerCombat pc; rogue_combat_init(&pc);
    RoguePlayer player; memset(&player,0,sizeof player); player.team_id=0; player.strength=50; player.facing=2; player.poise_max=50; player.poise=25; player.base.pos.x=0; player.base.pos.y=0;
    RogueEnemy enemy; memset(&enemy,0,sizeof enemy); enemy.alive=1; enemy.team_id=1; enemy.base.pos.x=1.0f; enemy.base.pos.y=0.0f; enemy.health=200; enemy.max_health=200; enemy.facing=1; /* enemy facing left (-x) */

    /* Backstab: place player behind enemy (enemy facing left, player to right of enemy) */
    player.base.pos.x = 1.8f; player.base.pos.y = 0.0f; /* ~0.8 right of enemy */
    int bs = rogue_combat_try_backstab(&player,&pc,&enemy); printf("after_backstab_attempt bs=%d cooldown=%.1f\n", bs, pc.backstab_cooldown_ms); if(!bs){ printf("fail_backstab_detect pos=(%.2f,%.2f) enemy=(%.2f,%.2f) facing=%d\n", player.base.pos.x, player.base.pos.y, enemy.base.pos.x, enemy.base.pos.y, enemy.facing); return 1; }
    /* Simulate strike using backstab bonus: expect higher damage when we manually apply multiplier */
    enemy.health=200; player.base.pos.x=0.2f; player.base.pos.y=0.0f; /* move in front for consistent baseline strike */
    int dmg_no_bonus; do_strike(&pc,&player,&enemy); dmg_no_bonus = 200 - enemy.health; if(dmg_no_bonus<=0){ printf("fail_base_strike pos=(%.2f,%.2f) enemy_health=%d\n", player.base.pos.x, player.base.pos.y, enemy.health); return 2; }

    /* Parry: activate then register incoming attack */
    rogue_player_begin_parry(&player,&pc); printf("after_begin_parry active=%d\n", pc.parry_active); if(!rogue_player_is_parry_active(&pc)){ printf("fail_parry_not_active parry_active=%d timer=%.2f\n", pc.parry_active, pc.parry_timer_ms); return 3; }
    /* Attack coming from in front of player (player facing=2 -> +x) */
    int parry_ok = rogue_player_register_incoming_attack_parry(&player,&pc,1,0); printf("after_register_parry ok=%d riposte_ready=%d window=%.1f iframes=%.1f\n", parry_ok, pc.riposte_ready, pc.riposte_window_ms, player.iframes_ms); if(!parry_ok){ printf("fail_parry_register facing=%d active=%d\n", player.facing, pc.parry_active); return 4; }
    if(!pc.riposte_ready){ printf("fail_riposte_not_ready\n"); return 5; }

    /* Riposte consumption */
    int riposte = rogue_player_try_riposte(&player,&pc,&enemy); if(!riposte){ printf("fail_riposte_consume\n"); return 6; }
    if(pc.riposte_ready){ printf("fail_riposte_flag_persist\n"); return 7; }

    printf("phase6_parry_backstab_riposte: OK base=%d\n", dmg_no_bonus);
    return 0;
}
