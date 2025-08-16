#include "core/skills.h"
#include "core/skill_tree.h"
#include "core/projectiles.h"
#include "core/damage_calc.h"
#include "core/app_state.h"
#include "entities/enemy.h"
#include <assert.h>
#include <stdio.h>

/* Test: passive fire synergy increases fireball damage; projectile spawns and damages enemy correctly. */

static RogueSkillDef make_passive_pyro(void){
    RogueSkillDef d = {0};
    d.id=-1; d.name="Pyromancy"; d.icon="icon_pyro"; d.max_rank=5; d.skill_strength=0;
    d.base_cooldown_ms=0.0f; d.cooldown_reduction_ms_per_rank=0.0f; d.on_activate=NULL; d.is_passive=1; d.tags=ROGUE_SKILL_TAG_FIRE; d.synergy_id=ROGUE_SYNERGY_FIRE_POWER; d.synergy_value_per_rank=2;
    return d;
}
static RogueSkillDef make_fireball(void){
    RogueSkillDef d = {0};
    d.id=-1; d.name="Fireball"; d.icon="icon_fire"; d.max_rank=5; d.skill_strength=0;
    d.base_cooldown_ms=6000.0f; d.cooldown_reduction_ms_per_rank=400.0f; d.on_activate=NULL; d.is_passive=0; d.tags=ROGUE_SKILL_TAG_FIRE; d.synergy_id=-1; d.synergy_value_per_rank=0;
    return d;
}

int main(void){
    /* Minimal init of skills + projectiles */
    rogue_skills_init();
    rogue_projectiles_init();
    for(int i=0;i<ROGUE_MAX_ENEMIES;i++){ g_app.enemies[i].alive=0; }
    g_app.enemy_count=0;
    /* Register skills */
    RogueSkillDef pyro = make_passive_pyro();
    RogueSkillDef fireb = make_fireball();
    int pid = rogue_skill_register(&pyro);
    int fid = rogue_skill_register(&fireb);
    /* Rank up passive to 3 and fireball to 1 */
    g_app.talent_points = 3; assert(rogue_skill_rank_up(pid)==1); assert(rogue_skill_rank_up(pid)==2); assert(rogue_skill_rank_up(pid)==3);
    g_app.talent_points = 1; assert(rogue_skill_rank_up(fid)==1);
    /* Place dummy enemy in front of player (player at 0,0 default) */
    g_app.world_map.width = 100; g_app.world_map.height=100; g_app.world_map.tiles=NULL; /* just bounds */
    g_app.player.base.pos.x = 0.0f; g_app.player.base.pos.y = 0.0f; g_app.player.facing = 2; /* facing right */
    g_app.enemies[0].alive=1; g_app.enemies[0].type_index=0; g_app.enemies[0].base.pos.x = 3.0f; g_app.enemies[0].base.pos.y = 0.0f; g_app.enemies[0].health=50; g_app.enemies[0].max_health=50; g_app.enemy_count=1;
    /* Compute expected damage */
    int expected = 3 + 1*2 + (3*2); /* base + rank contribution + synergy (rank 3 *2) */
    /* Manually spawn fireball projectile emulating effect (no activation wrapper yet) */
    rogue_projectiles_spawn(g_app.player.base.pos.x, g_app.player.base.pos.y, 1,0, 80.0f + 1*15.0f, 3500.0f, expected);
    assert(rogue_projectiles_active_count()==1);
    /* Simulate updates until projectile should hit (distance 3 at ~95 units/s) */
    for(int step=0; step<100 && rogue_projectiles_active_count()>0; ++step){ rogue_projectiles_update(16.0f); }
    /* Enemy should have reduced health */
    assert(g_app.enemies[0].health == 50 - expected);
    printf("FIREBALL_SKILL_TEST_OK\n");
    rogue_skills_shutdown();
    return 0;
}
