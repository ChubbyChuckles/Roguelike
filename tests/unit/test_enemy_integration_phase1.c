/* Enemy Integration Phase 1: Deterministic encounter seed & replay hash tests */
#define SDL_MAIN_HANDLED
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "core/enemy_integration.h"
#include "core/encounter_composer.h"
#include "core/app_state.h"

static void write_encounters_file(void){
    FILE* f = fopen("encounters_det.cfg","wb"); assert(f);
    fputs("id=0\nname=Pack\ntype=swarm\nmin=3\nmax=4\nelite_spacing=2\nelite_chance=0.0\n\n",f);
    fclose(f);
}

static void fabricate_types(){
    g_app.enemy_type_count = 1; /* single type sufficient */
    memset(&g_app.enemy_types[0],0,sizeof(g_app.enemy_types[0]));
#if defined(_MSC_VER)
    strncpy_s(g_app.enemy_types[0].id,sizeof g_app.enemy_types[0].id,"goblin_grunt",_TRUNCATE);
    strncpy_s(g_app.enemy_types[0].name,sizeof g_app.enemy_types[0].name,"Goblin Grunt",_TRUNCATE);
#else
    strncpy(g_app.enemy_types[0].id,"goblin_grunt",sizeof g_app.enemy_types[0].id -1);
    strncpy(g_app.enemy_types[0].name,"Goblin Grunt",sizeof g_app.enemy_types[0].name -1);
#endif
    g_app.enemy_types[0].tier_id=0; g_app.enemy_types[0].base_level_offset=0; g_app.enemy_types[0].archetype_id=0;
}

static void test_seed_and_hash(){
    write_encounters_file(); int loaded = rogue_encounters_load_file("encounters_det.cfg"); assert(loaded==1);
    fabricate_types();
    int player_level=12; int difficulty_rating=12; int biome_id=0; int template_id=0;
    unsigned int world_seed = 555u; g_app.world_seed = world_seed;
    unsigned long long first_hashes[5]; unsigned int first_seeds[5]; int first_counts[5];
    for(int encounter_index=0; encounter_index<5; encounter_index++){
        unsigned int seed = rogue_enemy_integration_encounter_seed(world_seed,7,42,encounter_index);
        RogueEncounterComposition comp; memset(&comp,0,sizeof comp);
        int r = rogue_encounter_compose(template_id, player_level, difficulty_rating, biome_id, seed, &comp); assert(r==0);
        int levels[64]; for(int i=0;i<comp.unit_count;i++) levels[i]=comp.units[i].level;
        unsigned long long h = rogue_enemy_integration_replay_hash(template_id, levels, comp.unit_count, NULL, 0);
        first_hashes[encounter_index]=h; first_seeds[encounter_index]=seed; first_counts[encounter_index]=comp.unit_count;
    }
    /* Second pass */
    for(int encounter_index=0; encounter_index<5; encounter_index++){
        unsigned int seed = rogue_enemy_integration_encounter_seed(world_seed,7,42,encounter_index);
        RogueEncounterComposition comp; memset(&comp,0,sizeof comp);
        int r = rogue_encounter_compose(template_id, player_level, difficulty_rating, biome_id, seed, &comp); assert(r==0);
        int levels[64]; for(int i=0;i<comp.unit_count;i++) levels[i]=comp.units[i].level;
        unsigned long long h = rogue_enemy_integration_replay_hash(template_id, levels, comp.unit_count, NULL, 0);
        assert(seed==first_seeds[encounter_index]);
        assert(h==first_hashes[encounter_index]);
        assert(comp.unit_count==first_counts[encounter_index]);
    }
    /* Different world seed should change at least one hash */
    unsigned int diff_world_seed = 556u; int any_diff=0;
    for(int encounter_index=0; encounter_index<5; encounter_index++){
        unsigned int seed = rogue_enemy_integration_encounter_seed(diff_world_seed,7,42,encounter_index);
        RogueEncounterComposition comp; memset(&comp,0,sizeof comp);
        int r = rogue_encounter_compose(template_id, player_level, difficulty_rating, biome_id, seed, &comp); assert(r==0);
        int levels[64]; for(int i=0;i<comp.unit_count;i++) levels[i]=comp.units[i].level;
        unsigned long long h = rogue_enemy_integration_replay_hash(template_id, levels, comp.unit_count, NULL, 0);
        if(h!=first_hashes[encounter_index]) any_diff=1;
    }
    assert(any_diff==1);
}

int main(void){ test_seed_and_hash(); printf("OK test_enemy_integration_phase1\n"); return 0; }
