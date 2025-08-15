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
void rogue_add_damage_number(float x,float y,int amount,int from_player){(void)x;(void)y;(void)amount;(void)from_player;}
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){(void)x;(void)y;(void)amount;(void)from_player;(void)crit;}

static int strike_once_vs_enemy(int armor,int resist_fire,int resist_frost,int resist_arcane,int raw_expect_less_than_raw){
    RoguePlayer p; rogue_player_init(&p); p.strength=50; p.dexterity=10; p.intelligence=10; p.base.pos.x=0; p.base.pos.y=0; p.facing=2;
    RoguePlayerCombat c; rogue_combat_init(&c); rogue_combat_set_archetype(&c, ROGUE_WEAPON_LIGHT); c.phase=ROGUE_ATTACK_STRIKE; c.combo=0;
    RogueEnemy e; e.alive=1; e.base.pos.x=1.0f; e.base.pos.y=0; e.health=10000; e.max_health=10000; e.armor=armor; e.resist_fire=resist_fire; e.resist_frost=resist_frost; e.resist_arcane=resist_arcane;
    int before = e.health; rogue_combat_player_strike(&c,&p,&e,1); int applied = before - e.health; (void)raw_expect_less_than_raw; return applied; }

int main(){
    /* Baseline with no armor/resist */
    int dmg_no_mit = strike_once_vs_enemy(0,0,0,0,0);
    /* Armor should reduce (physical) */
    int dmg_armor = strike_once_vs_enemy(20,0,0,0,1);
    printf("no_mit=%d armor=%d\n", dmg_no_mit, dmg_armor);
    assert(dmg_armor < dmg_no_mit);
    /* Fire resist should reduce when using a fire attack: choose a fire archetype attack (spell focus uses arcane so reuse by forcing damage type via override if needed) */
    /* For now we just compare by manually calling mitigation helper for fire scenario */
    RogueEnemy e = {0}; e.alive=1; e.health=100; e.armor=0; e.resist_fire=50; int overkill=0; int mitig = rogue_apply_mitigation_enemy(&e, 40, ROGUE_DMG_FIRE, &overkill); assert(mitig == 20);
    /* Minimum floor test: huge armor */
    e.health=5; e.armor=999; int f2 = rogue_apply_mitigation_enemy(&e, 30, ROGUE_DMG_PHYSICAL, &overkill); assert(f2 >=1); assert(f2 <=5);
    printf("combat_mitigation: OK\n");
    return 0;
}
