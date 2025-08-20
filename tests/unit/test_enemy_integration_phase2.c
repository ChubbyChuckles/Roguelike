/* Enemy Integration Phase 2: Encounter Template → Room Placement tests */
#define SDL_MAIN_HANDLED
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/enemy_integration.h"
#include "core/encounter_composer.h"
#include "world/world_gen.h"
#include "core/app_state.h"

static void write_test_encounters_file(void){
    FILE* f = fopen("encounters_phase2.cfg","wb"); assert(f);
    fputs("id=0\nname=Test Swarm\ntype=swarm\nmin=3\nmax=6\nelite_spacing=3\nelite_chance=0.1\n\n",f);
    fputs("id=1\nname=Test Mixed\ntype=mixed\nmin=4\nmax=7\nelite_spacing=3\nelite_chance=0.2\n\n",f);
    fputs("id=2\nname=Test Champion\ntype=champion_pack\nmin=2\nmax=4\nelite_spacing=2\nelite_chance=0.5\n\n",f);
    fputs("id=3\nname=Test Boss\ntype=boss_room\nmin=1\nmax=1\nboss=1\nsupport_min=2\nsupport_max=4\n\n",f);
    fclose(f);
}

static void fabricate_types(){
    g_app.enemy_type_count = 1;
    memset(&g_app.enemy_types[0],0,sizeof(g_app.enemy_types[0]));
    strcpy(g_app.enemy_types[0].id, "test_grunt");
    strcpy(g_app.enemy_types[0].name, "Test Grunt");
    g_app.enemy_types[0].tier_id=0; 
    g_app.enemy_types[0].base_level_offset=0; 
    g_app.enemy_types[0].archetype_id=0;
}

static void test_template_selection(){
    write_test_encounters_file(); 
    int loaded = rogue_encounters_load_file("encounters_phase2.cfg"); 
    assert(loaded == 4);
    
    int template_id;
    unsigned int seed = 12345u;
    
    /* Test shallow room - should get basic template */
    assert(rogue_enemy_integration_choose_template(1, 1, seed, &template_id));
    assert(template_id == 0); /* swarm */
    
    /* Test medium depth - should get mixed or champion */
    assert(rogue_enemy_integration_choose_template(5, 1, seed + 1, &template_id));
    assert(template_id >= 0 && template_id <= 3);
    
    /* Test deep room - high chance for boss */
    int boss_count = 0;
    for(int i = 0; i < 10; i++){
        if(rogue_enemy_integration_choose_template(10, 1, seed + i, &template_id) && template_id == 3){
            boss_count++;
        }
    }
    assert(boss_count > 0); /* At least one boss room should be selected */
}

static void test_room_difficulty_calculation(){
    /* Small room, no tags */
    int difficulty = rogue_enemy_integration_compute_room_difficulty(3, 25, 0);
    assert(difficulty == 3); /* base depth */
    
    /* Large room */
    difficulty = rogue_enemy_integration_compute_room_difficulty(3, 100, 0);
    assert(difficulty == 4); /* depth + large room bonus */
    
    /* Elite room */
    difficulty = rogue_enemy_integration_compute_room_difficulty(3, 25, ROGUE_DUNGEON_ROOM_ELITE);
    assert(difficulty == 5); /* depth + elite bonus */
    
    /* Puzzle room (easier combat) */
    difficulty = rogue_enemy_integration_compute_room_difficulty(3, 25, ROGUE_DUNGEON_ROOM_PUZZLE);
    assert(difficulty == 2); /* depth - puzzle penalty */
    
    /* Ensure minimum difficulty */
    difficulty = rogue_enemy_integration_compute_room_difficulty(1, 10, ROGUE_DUNGEON_ROOM_PUZZLE);
    assert(difficulty >= 1);
}

static void test_template_validation(){
    write_test_encounters_file(); 
    int loaded = rogue_encounters_load_file("encounters_phase2.cfg"); 
    assert(loaded == 4);
    
    /* Create test rooms */
    RogueDungeonRoom small_room = {0, 10, 10, 3, 3, 0, 0}; /* 3x3 = 9 area */
    RogueDungeonRoom medium_room = {1, 20, 20, 6, 6, 0, 0}; /* 6x6 = 36 area */
    RogueDungeonRoom tiny_room = {2, 5, 5, 2, 2, 0, 0};   /* 2x2 = 4 area */
    
    /* Boss template (id=3) should require larger room */
    assert(!rogue_enemy_integration_validate_template_placement(3, &small_room));
    assert(rogue_enemy_integration_validate_template_placement(3, &medium_room));
    
    /* Basic templates should work in small rooms */
    assert(rogue_enemy_integration_validate_template_placement(0, &small_room));
    assert(rogue_enemy_integration_validate_template_placement(1, &small_room));
    
    /* Tiny rooms should reject all templates */
    assert(!rogue_enemy_integration_validate_template_placement(0, &tiny_room));
    assert(!rogue_enemy_integration_validate_template_placement(3, &tiny_room));
}

static void test_room_encounter_preparation(){
    write_test_encounters_file(); 
    int loaded = rogue_encounters_load_file("encounters_phase2.cfg"); 
    assert(loaded == 4);
    
    /* Create test room */
    RogueDungeonRoom test_room = {5, 25, 30, 8, 7, ROGUE_DUNGEON_ROOM_ELITE, 0}; /* room id 5, elite tagged */
    
    RogueRoomEncounterInfo encounter_info;
    int world_seed = 999;
    int region_id = 2;
    
    assert(rogue_enemy_integration_prepare_room_encounter(&test_room, world_seed, region_id, &encounter_info));
    
    /* Verify computed values */
    assert(encounter_info.room_id == 5);
    assert(encounter_info.depth_level == 6); /* room_id + 1 */
    assert(encounter_info.biome_id == 1); /* plains */
    assert(encounter_info.encounter_template_id >= 0 && encounter_info.encounter_template_id <= 3);
    
    /* Verify seed is deterministic */
    unsigned int expected_seed = rogue_enemy_integration_encounter_seed((unsigned int)world_seed, region_id, test_room.id, 0);
    assert(encounter_info.encounter_seed == expected_seed);
}

static void test_deterministic_template_selection(){
    write_test_encounters_file(); 
    int loaded = rogue_encounters_load_file("encounters_phase2.cfg"); 
    assert(loaded == 4);
    
    /* Same inputs should produce same results */
    int template1, template2;
    unsigned int seed = 555u;
    
    assert(rogue_enemy_integration_choose_template(7, 1, seed, &template1));
    assert(rogue_enemy_integration_choose_template(7, 1, seed, &template2));
    assert(template1 == template2);
    
    /* Different seeds should potentially produce different results (at least sometimes) */
    int different_count = 0;
    for(int i = 0; i < 20; i++){
        int t1, t2;
        if(rogue_enemy_integration_choose_template(5, 1, seed + i, &t1) &&
           rogue_enemy_integration_choose_template(5, 1, seed + i + 1000, &t2) &&
           t1 != t2){
            different_count++;
        }
    }
    assert(different_count > 0); /* Should see some variation */
}

int main(){
    fabricate_types();
    
    test_template_selection();
    printf("✓ Template selection test passed\n");
    
    test_room_difficulty_calculation();
    printf("✓ Room difficulty calculation test passed\n");
    
    test_template_validation();
    printf("✓ Template validation test passed\n");
    
    test_room_encounter_preparation();
    printf("✓ Room encounter preparation test passed\n");
    
    test_deterministic_template_selection();
    printf("✓ Deterministic template selection test passed\n");
    
    printf("OK test_enemy_integration_phase2\n");
    return 0;
}
