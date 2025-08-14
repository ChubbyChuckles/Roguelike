#include "core/skills.h"
#include "core/damage_calc.h"
#include "core/projectiles.h"
#include "core/app_state.h"
#include <assert.h>
#include <stdio.h>

/* Minimal enemy stub insert: reuse global enemy array in app_state (assumed zeroed). */

static int effect_fireball(const struct RogueSkillDef* def, struct RogueSkillState* st, const struct RogueSkillCtx* ctx){ (void)ctx; float dx=1,dy=0; /* simple rightward */ rogue_projectiles_spawn(g_app.player.base.pos.x, g_app.player.base.pos.y, dx,dy, 100.0f, 500.0f, rogue_damage_fireball(def->id)); return 1; }

int main(void){
    rogue_skills_init();
    rogue_projectiles_init();
    /* Initialize player position baseline */
    g_app.player.base.pos.x = 10.0f; g_app.player.base.pos.y = 10.0f;
    /* Register passive fire mastery contributing synergy bucket 0 */
    RogueSkillDef fire_mastery = {-1,"FireMastery","icon_fm",5,0,0,NULL,1,ROGUE_SKILL_TAG_FIRE,0,2};
    RogueSkillDef fireball = {-1,"Fireball","icon_fire",5,6000,400,effect_fireball,0,ROGUE_SKILL_TAG_FIRE,-1,0};
    int mastery_id = rogue_skill_register(&fire_mastery);
    int fireball_id = rogue_skill_register(&fireball);
    g_app.talent_points = 4; assert(rogue_skill_rank_up(mastery_id)==1); assert(rogue_skill_rank_up(mastery_id)==2); assert(rogue_skill_rank_up(mastery_id)==3); /* rank 3 => synergy 3*2=6 */
    g_app.talent_points = 2; assert(rogue_skill_rank_up(fireball_id)==1);
    int dmg_expected = rogue_damage_fireball(fireball_id); /* base 3 + rank*2 (1*2=2) + synergy 6 => 11 */
    assert(dmg_expected==11);
    struct RogueSkillCtx ctx={.now_ms=0};
    int act = rogue_skill_try_activate(fireball_id,&ctx); assert(act==1);
    assert(rogue_projectiles_active_count()==1);
    assert(rogue_projectiles_last_damage()==dmg_expected);
    /* Place a dummy enemy directly in path at x+1,y */
    g_app.world_map.width = 100; g_app.world_map.height=100; /* bounds for projectile */
    g_app.enemies[0].alive=1; g_app.enemies[0].health=50; g_app.enemies[0].base.pos.x = g_app.player.base.pos.x + 1.0f; g_app.enemies[0].base.pos.y = g_app.player.base.pos.y; g_app.enemies[0].type_index=0; g_app.enemy_count=1;
    /* Advance updates until projectile would collide (speed 100 units/s -> 0.1 units per ms; we need ~10ms to move 1 unit) */
    for(int i=0;i<20;i++){ rogue_projectiles_update(1.0f); }
    assert(g_app.enemies[0].health == 50 - dmg_expected);
    printf("FIREBALL_SKILL_TEST_OK\n");
    rogue_skills_shutdown();
    return 0;
}
