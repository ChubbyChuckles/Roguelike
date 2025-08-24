/* Enemy Integration Phase 3: Stat & Modifier Application at Spawn tests */
#define SDL_MAIN_HANDLED
#include "../../src/core/app_state.h"
#include "../../src/core/enemy/encounter_composer.h"
#include "../../src/core/enemy/enemy_integration.h"
#include "../../src/core/enemy/enemy_modifiers.h"
#include "../../src/entities/enemy.h"
#include "../../src/world/world_gen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void write_test_encounters_file(void)
{
    FILE* f = fopen("encounters_phase3.cfg", "wb");
    assert(f);
    fputs("id=0\nname=Test Swarm\ntype=swarm\nmin=3\nmax=6\nelite_spacing=3\nelite_chance=0.1\n\n",
          f);
    fputs("id=2\nname=Test "
          "Champion\ntype=champion_pack\nmin=2\nmax=4\nelite_spacing=2\nelite_chance=0.5\n\n",
          f);
    fclose(f);
}

static void write_test_modifiers_file(void)
{
    FILE* f = fopen("modifiers_phase3.cfg", "wb");
    assert(f);
    fputs("id=1\nname=Swift\nweight=1.0\ntiers=0xFF\ndps_cost=0.2\ncontrol_cost=0.1\nmobility_cost="
          "0.3\nincompat_mask=0\ntelegraph=speed_aura\n\n",
          f);
    fputs("id=2\nname=Tough\nweight=1.0\ntiers=0xFF\ndps_cost=0.1\ncontrol_cost=0.2\nmobility_cost="
          "0.1\nincompat_mask=0\ntelegraph=defense_aura\n\n",
          f);
    fputs("id=3\nname=Berserk\nweight=0.8\ntiers=0xFF\ndps_cost=0.4\ncontrol_cost=0.3\nmobility_"
          "cost=0.2\nincompat_mask=0\ntelegraph=rage_aura\n\n",
          f);
    fclose(f);
}

static void fabricate_types()
{
    g_app.enemy_type_count = 2;
    memset(&g_app.enemy_types[0], 0, sizeof(g_app.enemy_types[0]));
    strcpy(g_app.enemy_types[0].id, "test_grunt");
    strcpy(g_app.enemy_types[0].name, "Test Grunt");
    g_app.enemy_types[0].tier_id = 0;
    g_app.enemy_types[0].base_level_offset = 0;
    g_app.enemy_types[0].archetype_id = 0;

    memset(&g_app.enemy_types[1], 0, sizeof(g_app.enemy_types[1]));
    strcpy(g_app.enemy_types[1].id, "test_elite");
    strcpy(g_app.enemy_types[1].name, "Test Elite");
    g_app.enemy_types[1].tier_id = 1;
    g_app.enemy_types[1].base_level_offset = 1;
    g_app.enemy_types[1].archetype_id = 0;
}

static void test_unit_stats_application()
{
    fabricate_types();

    /* Create test mapping */
    RogueEnemyTypeMapping mappings[2];
    int mapping_count = 0;
    rogue_enemy_integration_build_mappings(mappings, 2, &mapping_count);
    assert(mapping_count == 2);

    /* Create test encounter unit */
    RogueEncounterUnit unit;
    memset(&unit, 0, sizeof(unit));
    unit.enemy_type_id = 0;
    unit.level = 5;
    unit.is_elite = 0;

    /* Create test enemy */
    RogueEnemy enemy;
    memset(&enemy, 0, sizeof(enemy));
    enemy.type_index = 0;

    int player_level = 5;

    /* Apply stats */
    assert(rogue_enemy_integration_apply_unit_stats(&enemy, &unit, player_level, &mappings[0]));

    /* Verify basic properties */
    assert(enemy.level == unit.level);
    assert(enemy.tier_id == mappings[0].tier_id);
    assert(enemy.elite_flag == 0);
    assert(enemy.max_health > 0);
    assert(enemy.health == enemy.max_health);
    assert(enemy.final_hp > 0.0f);
    assert(enemy.final_damage >= 0.0f);
    assert(enemy.final_defense >= 0.0f);

    /* Test elite scaling */
    unit.is_elite = 1;
    RogueEnemy elite_enemy;
    memset(&elite_enemy, 0, sizeof(elite_enemy));
    assert(
        rogue_enemy_integration_apply_unit_stats(&elite_enemy, &unit, player_level, &mappings[0]));

    assert(elite_enemy.elite_flag == 1);
    assert(elite_enemy.final_hp > enemy.final_hp);         /* Elite should have more HP */
    assert(elite_enemy.final_damage > enemy.final_damage); /* Elite should have more damage */
}

static void test_modifier_application()
{
    write_test_modifiers_file();
    int loaded_mods = rogue_enemy_modifiers_load_file("modifiers_phase3.cfg");
    assert(loaded_mods > 0);

    /* Create test encounter unit */
    RogueEncounterUnit unit;
    memset(&unit, 0, sizeof(unit));
    unit.enemy_type_id = 0;
    unit.level = 5;
    unit.is_elite = 1; /* Elite for higher modifier chance */

    /* Create test enemy */
    RogueEnemy enemy;
    memset(&enemy, 0, sizeof(enemy));
    enemy.tier_id = 0;

    unsigned int modifier_seed = 12345u;

    /* Apply modifiers */
    assert(rogue_enemy_integration_apply_unit_modifiers(&enemy, &unit, modifier_seed, 1, 0));

    /* Elite should have a good chance of getting modifiers, but not guaranteed */
    /* We can't assert exact modifier count due to randomness, but the function should succeed */
    assert(enemy.modifier_count <= 8); /* Should not exceed maximum */
}

static void test_boss_modifier_application()
{
    write_test_modifiers_file();
    int loaded_mods = rogue_enemy_modifiers_load_file("modifiers_phase3.cfg");
    assert(loaded_mods > 0);

    /* Create boss encounter unit */
    RogueEncounterUnit unit;
    memset(&unit, 0, sizeof(unit));
    unit.enemy_type_id = 0;
    unit.level = 8;
    unit.is_elite = 0;

    /* Create test enemy */
    RogueEnemy enemy;
    memset(&enemy, 0, sizeof(enemy));
    enemy.tier_id = 0;

    unsigned int modifier_seed = 99999u;

    /* Apply modifiers as boss */
    assert(rogue_enemy_integration_apply_unit_modifiers(&enemy, &unit, modifier_seed, 0, 1));

    /* Bosses should always get modifiers (or at least have the chance) */
    assert(enemy.modifier_count <= 8);
}

static void test_finalize_spawn()
{
    fabricate_types();
    write_test_encounters_file();
    write_test_modifiers_file();

    int loaded_encounters = rogue_encounters_load_file("encounters_phase3.cfg");
    int loaded_mods = rogue_enemy_modifiers_load_file("modifiers_phase3.cfg");
    assert(loaded_encounters > 0);
    assert(loaded_mods > 0);

    /* Create test mappings */
    RogueEnemyTypeMapping mappings[2];
    int mapping_count = 0;
    rogue_enemy_integration_build_mappings(mappings, 2, &mapping_count);

    /* Create test room and encounter info */
    RogueDungeonRoom test_room = {3, 20, 25, 8, 7, 0, 0};
    RogueRoomEncounterInfo encounter_info;
    assert(rogue_enemy_integration_prepare_room_encounter(&test_room, 777, 1, &encounter_info));

    /* Create test encounter unit */
    RogueEncounterUnit unit;
    memset(&unit, 0, sizeof(unit));
    unit.enemy_type_id = 0;
    unit.level = 6;
    unit.is_elite = 1;

    /* Create test enemy */
    RogueEnemy enemy;
    memset(&enemy, 0, sizeof(enemy));
    enemy.type_index = 0;

    int player_level = 6;

    /* Finalize spawn */
    assert(rogue_enemy_integration_finalize_spawn(&enemy, &unit, &encounter_info, player_level,
                                                  &mappings[0]));

    /* Verify all aspects were applied */
    assert(enemy.encounter_id == encounter_info.room_id);
    assert(enemy.replay_hash_fragment != 0);
    assert(enemy.level == unit.level);
    assert(enemy.elite_flag == 1);
    assert(enemy.max_health > 0);
    assert(enemy.final_hp > 0.0f);

    /* Validate final stats */
    assert(rogue_enemy_integration_validate_final_stats(&enemy));
}

static void test_stat_validation()
{
    RogueEnemy valid_enemy;
    memset(&valid_enemy, 0, sizeof(valid_enemy));
    valid_enemy.final_hp = 100.0f;
    valid_enemy.final_damage = 10.0f;
    valid_enemy.final_defense = 5.0f;
    valid_enemy.max_health = 100;
    valid_enemy.health = 100;
    valid_enemy.level = 5;
    valid_enemy.modifier_count = 2;

    assert(rogue_enemy_integration_validate_final_stats(&valid_enemy));

    /* Test invalid cases */
    RogueEnemy invalid_enemy = valid_enemy;
    invalid_enemy.final_hp = -1.0f; /* Negative HP */
    assert(!rogue_enemy_integration_validate_final_stats(&invalid_enemy));

    invalid_enemy = valid_enemy;
    invalid_enemy.max_health = 0; /* Zero max health */
    assert(!rogue_enemy_integration_validate_final_stats(&invalid_enemy));

    invalid_enemy = valid_enemy;
    invalid_enemy.level = 0; /* Zero level */
    assert(!rogue_enemy_integration_validate_final_stats(&invalid_enemy));

    invalid_enemy = valid_enemy;
    invalid_enemy.modifier_count = 10; /* Too many modifiers */
    assert(!rogue_enemy_integration_validate_final_stats(&invalid_enemy));
}

static void test_deterministic_modifier_application()
{
    write_test_modifiers_file();
    int loaded_mods = rogue_enemy_modifiers_load_file("modifiers_phase3.cfg");
    assert(loaded_mods > 0);

    /* Create identical test scenarios */
    RogueEncounterUnit unit;
    memset(&unit, 0, sizeof(unit));
    unit.enemy_type_id = 0;
    unit.level = 5;
    unit.is_elite = 1;

    unsigned int modifier_seed = 555u;

    /* Apply modifiers twice with same parameters */
    RogueEnemy enemy1, enemy2;
    memset(&enemy1, 0, sizeof(enemy1));
    memset(&enemy2, 0, sizeof(enemy2));
    enemy1.tier_id = enemy2.tier_id = 0;

    assert(rogue_enemy_integration_apply_unit_modifiers(&enemy1, &unit, modifier_seed, 1, 0));
    assert(rogue_enemy_integration_apply_unit_modifiers(&enemy2, &unit, modifier_seed, 1, 0));

    /* Should produce identical results */
    assert(enemy1.modifier_count == enemy2.modifier_count);
    for (int i = 0; i < enemy1.modifier_count; i++)
    {
        assert(enemy1.modifier_ids[i] == enemy2.modifier_ids[i]);
    }
}

int main()
{
    test_unit_stats_application();
    printf("✓ Unit stats application test passed\n");

    test_modifier_application();
    printf("✓ Modifier application test passed\n");

    test_boss_modifier_application();
    printf("✓ Boss modifier application test passed\n");

    test_finalize_spawn();
    printf("✓ Finalize spawn test passed\n");

    test_stat_validation();
    printf("✓ Stat validation test passed\n");

    test_deterministic_modifier_application();
    printf("✓ Deterministic modifier application test passed\n");

    printf("OK test_enemy_integration_phase3\n");
    return 0;
}
