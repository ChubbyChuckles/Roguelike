#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <string.h>
#include "game/combat.h"
#include "game/weapons.h"
#include "entities/player.h"
#include "entities/enemy.h"

/* Provide minimal attack def table with distinct startup/recovery so stance modifiers are observable */
extern int rogue_force_attack_active; extern int g_attack_frame_override; static int rogue_get_current_attack_frame(void){ return 3; }
static int rogue_buffs_get_total(int t){(void)t; return 0;}
static void rogue_add_damage_number(float x,float y,int amount,int from_player){(void)x;(void)y;(void)amount;(void)from_player;}
static void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){(void)x;(void)y;(void)amount;(void)from_player;(void)crit;}

/* startup=120ms, active=60ms, recovery=140ms so stance adjustments (~5-8%) are measurable */
static const RogueAttackDef g_stance_attack = {0,"stance_test",ROGUE_WEAPON_LIGHT,0,120,60,140,10,0,40,ROGUE_DMG_PHYSICAL,0.0f,0,0,1,{{0,60,ROGUE_CANCEL_ON_HIT,1.0f,0,0,0,0}},0,ROGUE_CANCEL_ON_HIT,0.40f,0,0};
const RogueAttackDef* rogue_attack_get(int a,int b){(void)a;(void)b; return &g_stance_attack;} int rogue_attack_chain_length(int a){(void)a; return 1;}

static void reset_player(RoguePlayer* p){ memset(p,0,sizeof *p); p->team_id=0; p->strength=30; p->dexterity=10; p->intelligence=5; p->facing=2; p->equipped_weapon_id=0; p->combat_stance=0; }

/* Advance update loop until we exit STRIKE -> RECOVER -> IDLE for one attack, capturing phase durations */
static int perform_attack_measure(RoguePlayer* player, int stance, float *out_windup, float *out_recover){
    RoguePlayerCombat pc; rogue_combat_init(&pc); g_attack_frame_override=3; rogue_force_attack_active=1; player->combat_stance=stance; g_exposed_player_for_stats.combat_stance=stance; RogueEnemy enemy; memset(&enemy,0,sizeof enemy); enemy.alive=1; enemy.team_id=1; enemy.base.pos.x=1.0f; enemy.base.pos.y=0.0f; enemy.health=500; enemy.max_health=500; enemy.facing=1;
    /* Initiate attack by simulating a frame with input */
    rogue_combat_update_player(&pc, 5.0f, 1); /* initial 5ms tick with press */
    float wind=0.0f, rec=0.0f; int in_wind=0, in_recover=0; if(pc.phase==ROGUE_ATTACK_WINDUP) in_wind=1;
    for(int step=0; step<2000; ++step){
        rogue_combat_update_player(&pc, 5.0f, 0);
        if(pc.phase==ROGUE_ATTACK_WINDUP){ wind += 5.0f; if(!in_wind) in_wind=1; }
        else if(in_wind && pc.phase!=ROGUE_ATTACK_WINDUP){ in_wind=0; }
        if(pc.phase==ROGUE_ATTACK_STRIKE){ RoguePlayer pl = *player; RogueEnemy earr[1]; earr[0]=enemy; rogue_combat_player_strike(&pc,&pl,earr,1); enemy=earr[0]; }
        if(pc.phase==ROGUE_ATTACK_RECOVER){ rec += 5.0f; in_recover=1; }
        if(pc.phase==ROGUE_ATTACK_IDLE && rec>0.0f){ break; }
    }
    if(out_windup) *out_windup = wind; if(out_recover) *out_recover = rec; return 0;
}

RoguePlayer g_exposed_player_for_stats = {0};

int main(){
    RoguePlayer player; reset_player(&player); g_exposed_player_for_stats = player; rogue_force_attack_active=1; g_attack_frame_override=3;
    float w_bal=0,r_bal=0; perform_attack_measure(&player,0,&w_bal,&r_bal);
    float w_ag=0,r_ag=0; perform_attack_measure(&player,1,&w_ag,&r_ag);
    float w_def=0,r_def=0; perform_attack_measure(&player,2,&w_def,&r_def);
    /* Expect aggressive shorter windup & recovery vs balanced, defensive longer */
    if(!(w_ag < w_bal*0.97f)){ printf("fail_aggressive_windup_only w_bal=%.1f w_ag=%.1f\n",w_bal,w_ag); return 1; }
    if(!(w_def > w_bal*1.04f)){ printf("fail_defensive_windup_only w_bal=%.1f w_def=%.1f\n",w_bal,w_def); return 2; }
    printf("phase7_stance_frame_timing: OK w_bal=%.1f w_ag=%.1f w_def=%.1f r_bal=%.1f r_ag=%.1f r_def=%.1f\n",w_bal,w_ag,w_def,r_bal,r_ag,r_def); return 0; }
