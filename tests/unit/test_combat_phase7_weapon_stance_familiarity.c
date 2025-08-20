#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <string.h>
#include "game/combat.h"
#include "game/weapons.h"
#include "entities/player.h"
#include "entities/enemy.h"

extern int rogue_force_attack_active; extern int g_attack_frame_override; static int rogue_get_current_attack_frame(void){ return 3; }
static int rogue_buffs_get_total(int t){(void)t; return 0;}
static void rogue_add_damage_number(float x,float y,int amount,int from_player){(void)x;(void)y;(void)amount;(void)from_player;}
static void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){(void)x;(void)y;(void)amount;(void)from_player;(void)crit;}
static const RogueAttackDef g_test_attack = {0,"light",ROGUE_WEAPON_LIGHT,0,0,70,0,5,0,20,ROGUE_DMG_PHYSICAL,0.0f,0,0,1,{{0,70,ROGUE_CANCEL_ON_HIT,1.0f,0,0,0,0}},0,ROGUE_CANCEL_ON_HIT,0.40f,0,0};
const RogueAttackDef* rogue_attack_get(int a,int b){(void)a;(void)b; return &g_test_attack;} int rogue_attack_chain_length(int a){(void)a; return 1;}

static int strike_once(RoguePlayerCombat* pc, RoguePlayer* pl, RogueEnemy* e){ pc->phase=ROGUE_ATTACK_STRIKE; pc->strike_time_ms=10; pc->processed_window_mask=0; pc->emitted_events_mask=0; int hb=e->health; rogue_combat_player_strike(pc,pl,e,1); return hb - e->health; }

int main(){
    rogue_force_attack_active=1; g_attack_frame_override=3;
    RoguePlayerCombat pc; rogue_combat_init(&pc);
    RoguePlayer player; memset(&player,0,sizeof player); player.team_id=0; player.strength=40; player.dexterity=20; player.intelligence=10; player.facing=2; player.equipped_weapon_id=0; player.combat_stance=0; player.base.pos.x=0; player.base.pos.y=0;
    RogueEnemy enemy; memset(&enemy,0,sizeof enemy); enemy.alive=1; enemy.team_id=1; enemy.base.pos.x=1.0f; enemy.base.pos.y=0.0f; enemy.health=500; enemy.max_health=500; enemy.facing=1;

    int dmg_balanced = strike_once(&pc,&player,&enemy); if(dmg_balanced<=0){ printf("fail_balanced_damage=%d\n", dmg_balanced); return 1; }

    enemy.health=500; player.combat_stance=1; /* aggressive */ int dmg_aggr = strike_once(&pc,&player,&enemy); if(!(dmg_aggr > (int)(dmg_balanced*1.10f))){ printf("fail_aggressive_scaling base=%d aggr=%d\n", dmg_balanced, dmg_aggr); return 2; }

    /* Simulate familiarity accumulation */
    enemy.health=500; player.combat_stance=0; for(int i=0;i<30;i++){ strike_once(&pc,&player,&enemy); enemy.health=500; }
    enemy.health=500; int dmg_fam = strike_once(&pc,&player,&enemy); if(!(dmg_fam > dmg_balanced)){ printf("fail_familiarity_bonus base=%d fam=%d\n", dmg_balanced, dmg_fam); return 3; }

    /* Durability decreases */
    float dur_before = rogue_weapon_current_durability(0); if(dur_before<=0) dur_before=100.0f; strike_once(&pc,&player,&enemy); float dur_after = rogue_weapon_current_durability(0); if(!(dur_after < dur_before)){ printf("fail_durability_drop before=%.2f after=%.2f\n", dur_before, dur_after); return 4; }

    printf("phase7_weapon_stance_familiarity: OK base=%d aggr=%d fam=%d dur_before=%.1f dur_after=%.1f\n", dmg_balanced, dmg_aggr, dmg_fam, dur_before, dur_after);
    return 0;
}
