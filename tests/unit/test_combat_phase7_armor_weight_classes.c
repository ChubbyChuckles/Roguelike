#define SDL_MAIN_HANDLED
#include <stdio.h>
#include <string.h>
#include "game/combat.h"
#include "game/armor.h"
#include "entities/player.h"
#include "entities/enemy.h"

extern int rogue_force_attack_active; extern int g_attack_frame_override; int rogue_get_current_attack_frame(void){ return 3; }
int rogue_buffs_get_total(int t){(void)t; return 0;}
void rogue_add_damage_number(float x,float y,int amount,int from_player){(void)x;(void)y;(void)amount;(void)from_player;}
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){(void)x;(void)y;(void)amount;(void)from_player;(void)crit;}
static const RogueAttackDef g_test_attack = {0,"light",ROGUE_WEAPON_LIGHT,0,0,70,0,5,0,20,ROGUE_DMG_PHYSICAL,0.0f,0,0,1,{{0,70,ROGUE_CANCEL_ON_HIT,1.0f,0,0,0,0}},0,ROGUE_CANCEL_ON_HIT,0.40f,0,0};
const RogueAttackDef* rogue_attack_get(int a,int b){(void)a;(void)b; return &g_test_attack;} int rogue_attack_chain_length(int a){(void)a; return 1;}

int main(){
    rogue_force_attack_active=1; g_attack_frame_override=3;
    RoguePlayer p; memset(&p,0,sizeof p); p.vitality=20; p.strength=25; p.dexterity=15; p.intelligence=10; rogue_player_recalc_derived(&p);
    /* Equip light set */
    rogue_armor_equip_slot(ROGUE_ARMOR_HEAD,0); rogue_armor_equip_slot(ROGUE_ARMOR_CHEST,3); rogue_armor_equip_slot(ROGUE_ARMOR_LEGS,6); rogue_armor_equip_slot(ROGUE_ARMOR_HANDS,9); rogue_armor_equip_slot(ROGUE_ARMOR_FEET,12); rogue_armor_recalc_player(&p);
    float enc_light = p.encumbrance; int tier_light = p.encumbrance_tier; float regen_mult_light = p.cc_slow_pct; int armor_light = p.armor;
    /* Equip heavy set */
    rogue_armor_equip_slot(ROGUE_ARMOR_HEAD,2); rogue_armor_equip_slot(ROGUE_ARMOR_CHEST,5); rogue_armor_equip_slot(ROGUE_ARMOR_LEGS,8); rogue_armor_equip_slot(ROGUE_ARMOR_HANDS,11); rogue_armor_equip_slot(ROGUE_ARMOR_FEET,14); rogue_armor_recalc_player(&p);
    float enc_heavy = p.encumbrance; int tier_heavy = p.encumbrance_tier; float regen_mult_heavy = p.cc_slow_pct; int armor_heavy = p.armor;
    if(!(enc_heavy > enc_light && tier_heavy >= tier_light)){ printf("fail_encumbrance tier_light=%d tier_heavy=%d\n", tier_light, tier_heavy); return 1; }
    if(!(armor_heavy > armor_light)){ printf("fail_armor_gain light=%d heavy=%d\n", armor_light, armor_heavy); return 2; }
    if(!(regen_mult_heavy < regen_mult_light)){ printf("fail_regen_mult light=%.2f heavy=%.2f\n", regen_mult_light, regen_mult_heavy); return 3; }
    printf("phase7_armor_weight_classes: OK enc_light=%.1f enc_heavy=%.1f tier_light=%d tier_heavy=%d armor_light=%d armor_heavy=%d regen_light=%.2f regen_heavy=%.2f\n", enc_light, enc_heavy, tier_light, tier_heavy, armor_light, armor_heavy, regen_mult_light, regen_mult_heavy);
    return 0;
}
