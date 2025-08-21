#ifdef ROGUE_HAVE_SDL
#define SDL_MAIN_HANDLED 1
#endif
#include <assert.h>
#include <stdio.h>
#include "game/combat.h"

/* Minimal stubs */
RoguePlayer g_exposed_player_for_stats = {0};
int rogue_get_current_attack_frame(void){ return 3; }
void rogue_app_add_hitstop(float ms){ (void)ms; }
void rogue_add_damage_number(float x,float y,int amount,int from_player){ (void)x;(void)y;(void)amount;(void)from_player; }
void rogue_add_damage_number_ex(float x,float y,int amount,int from_player,int crit){ (void)x;(void)y;(void)amount;(void)from_player;(void)crit; }

/* Compute expected final damage for a physical hit with penetration applied BEFORE mitigation. */
static int expected_after_pen(int raw,int armor,int phys_resist,int pen_flat,int pen_pct){
    if(raw < 0) raw = 0;
    /* Penetration order in strike code: flat -> percent(of original armor) -> result passed as new armor to mitigation */
    int eff_armor = armor;
    if(pen_flat > 0){ eff_armor -= pen_flat; if(eff_armor < 0) eff_armor = 0; }
    if(pen_pct > 100) pen_pct = 100;
    if(pen_pct > 0){ int reduce = (armor * pen_pct)/100; eff_armor -= reduce; if(eff_armor < 0) eff_armor = 0; }
    /* Mitigation: flat armor (capped so that at least 1 gets through) then percent physical resist */
    int dmg = raw;
    if(eff_armor > 0){ if(eff_armor >= dmg) dmg = (dmg>1?1:dmg); else dmg -= eff_armor; }
    if(phys_resist < 0) phys_resist = 0; if(phys_resist > 90) phys_resist = 90;
    if(phys_resist > 0){ int red = (dmg * phys_resist)/100; dmg -= red; }
    if(dmg < 1) dmg = 1;
    return dmg;
}

int main(){
    /* Force deterministic non-crit behavior for this test */
    extern int g_force_crit_mode; g_force_crit_mode = 0;
    extern int g_crit_layering_mode; g_crit_layering_mode = 0;
    RoguePlayer player; rogue_player_init(&player); player.strength = 60; /* produce some base raw */
    player.pen_flat = 5; player.pen_percent = 25; /* 5 flat then 25% of original armor */
    RoguePlayerCombat pc; rogue_combat_init(&pc); pc.phase = ROGUE_ATTACK_STRIKE; pc.chain_index = 0; pc.archetype = ROGUE_WEAPON_LIGHT; pc.strike_time_ms = 20.0f; /* inside first window */
    RogueEnemy enemy; enemy.alive=1; enemy.base.pos.x=1.0f; enemy.base.pos.y=0; enemy.health=10000; enemy.max_health=10000; enemy.armor=30; enemy.resist_physical=40; enemy.resist_fire=0; enemy.resist_frost=0; enemy.resist_arcane=0; enemy.resist_bleed=0; enemy.resist_poison=0;
    int before = enemy.health;
    rogue_combat_player_strike(&pc,&player,&enemy,1);
    int applied = before - enemy.health;
    /* Reconstruct approximate raw from attack def (light_1) similar to combat calculation */
    const RogueAttackDef* def = rogue_attack_get(ROGUE_WEAPON_LIGHT,0);
    int effective_strength = player.strength; float scaled = def->base_damage + (float)effective_strength * def->str_scale + (float)player.dexterity * def->dex_scale + (float)player.intelligence * def->int_scale; if(scaled < 1.0f) scaled = 1.0f;
    float combo_scale = 1.0f; float raw = scaled * combo_scale * 1.0f; int raw_i = (int)floorf(raw + 0.5f);
    int expect = expected_after_pen(raw_i, 30, 40, player.pen_flat, player.pen_percent);
    printf("penetration: raw=%d applied=%d expect=%d\n", raw_i, applied, expect);
    assert(applied == expect);
    printf("combat_penetration: OK\n");
    return 0;
}
