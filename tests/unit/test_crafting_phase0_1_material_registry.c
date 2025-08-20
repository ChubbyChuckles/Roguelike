/* Crafting & Gathering Phase 0–1 Tests
 * Validates material registry parsing, duplicate rejection, lookup APIs, deterministic ordering, and seed mixing.
 */
#include "core/material_registry.h"
#include "core/loot_item_defs.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#ifdef _MSC_VER
#include <direct.h>
#define CHDIR _chdir
#else
#include <unistd.h>
#define CHDIR chdir
#endif

static int file_exists(const char* p){ struct stat st; return stat(p,&st)==0; }
static void attempt_cd_to_root(void){
    /* Try up to 5 levels to locate assets/items/materials.cfg */
    for(int i=0;i<5;i++){
        if(file_exists("assets/items/materials.cfg")) return; /* found */
    CHDIR("..");
    }
}

static int ensure_item_defs(void){ if(rogue_item_defs_count()>0) return 1; /* attempt to load materials cfg directory */
    char p[256]; if(rogue_find_asset_path("items/materials.cfg", p, sizeof p)){
        for(int i=(int)strlen(p)-1;i>=0;i--){ if(p[i]=='/'||p[i]=='\\'){ p[i]='\0'; break; } }
        if(rogue_item_defs_load_directory(p)>0) return 1;
    }
    /* Fallback manual relative guesses (build dir -> project root) */
    const char* rels[] = { "../assets/items", "../../assets/items", "../../../assets/items" };
    for(int r=0;r< (int)(sizeof rels/sizeof rels[0]); r++){
        if(rogue_item_defs_load_directory(rels[r])>0) return 1;
    }
    char pj[256]; if(!rogue_find_asset_path("items/items.json", pj, sizeof pj)) rogue_find_asset_path("items.json", pj, sizeof pj);
    if(pj[0]) if(rogue_item_defs_load_from_json(pj)>0) return 1;
    return 0;
}

int main(void){
    printf("CRAFT_P0_DEBUG start\n"); fflush(stdout);
    attempt_cd_to_root();
    if(!ensure_item_defs()){ printf("CRAFT_P0_FAIL items load\n"); fflush(stdout); return 1; }
    rogue_material_registry_reset();
    /* Create a temporary materials file in working dir referencing known items */
    FILE* f = fopen("materials_test.cfg","wb"); if(!f){ printf("CRAFT_P0_FAIL temp file\n"); return 2; }
    /* Expect iron_ore, arcane_dust, primal_shard definitions exist */
    fprintf(f, "iron_ore_mat,iron_ore,0,ore,8\n");
    fprintf(f, "arcane_dust_mat,arcane_dust,1,essence,25\n");
    fprintf(f, "primal_shard_mat,primal_shard,3,essence,180\n");
    /* duplicate to test skip */
    fprintf(f, "arcane_dust_mat,arcane_dust,2,essence,30\n");
    fclose(f);
    int added = rogue_material_registry_load_path("materials_test.cfg"); if(added < 3){ printf("CRAFT_P0_FAIL added %d\n", added); return 3; }
    if(rogue_material_count() != 3){ printf("CRAFT_P0_FAIL count %d\n", rogue_material_count()); return 4; }
    const RogueMaterialDef* m0 = rogue_material_get(0); const RogueMaterialDef* m1 = rogue_material_get(1); if(!m0||!m1){ printf("CRAFT_P0_FAIL get null\n"); return 5; }
    if(strcmp(m0->id, "iron_ore_mat")!=0){ printf("CRAFT_P0_FAIL order %s\n", m0->id); return 6; }
    if(!rogue_material_find("arcane_dust_mat")){ printf("CRAFT_P0_FAIL find id\n"); return 7; }
    int idxs[4]; int pref_n = rogue_material_prefix_search("arcane", idxs, 4); if(pref_n != 1){ printf("CRAFT_P0_FAIL prefix %d\n", pref_n); return 8; }
    const RogueMaterialDef* dust = rogue_material_find("arcane_dust_mat"); if(!dust){ printf("CRAFT_P0_FAIL dust null\n"); return 9; }
    if(dust->tier != 1){ printf("CRAFT_P0_FAIL tier %d\n", dust->tier); fflush(stdout); return 10; }
    unsigned int mix_a = rogue_material_seed_mix(12345u, 0); unsigned int mix_b = rogue_material_seed_mix(12345u, 1); if(mix_a == mix_b){ printf("CRAFT_P0_FAIL seed mix collision\n"); fflush(stdout); return 11; }
    printf("CRAFT_P0_1_OK count=%d tier_dust=%d seed_mix_ok\n", rogue_material_count(), dust->tier); fflush(stdout); return 0;
}
