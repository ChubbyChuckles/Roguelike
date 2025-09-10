/* Equipment Phase 2.4: JSON vs CFG item definition stat aggregation parity test
 * Verifies that implicit + affix derived stats aggregate identically whether the
 * underlying item definitions were loaded from legacy CFG or JSON sources.
 */
#include "../../src/core/app/app_state.h"
#include "../../src/core/equipment/equipment.h"
#include "../../src/core/equipment/equipment_stats.h"
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/game/stat_cache.h"
#include <assert.h>
#include <stdio.h>

RogueAppState g_app;                    /* minimal globals required by some equipment helpers */
RoguePlayer g_exposed_player_for_stats; /* referenced in equipment_stats */
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

static void seed_affixes(void)
{
    rogue_affixes_reset();
    const char* path = "affix_tmp.cfg"; /* reuse simple deterministic file */
    FILE* f = fopen(path, "wb");
    /* type,id,stat,min,max,w0..w4 (weights arbitrary but non-zero) */
    fprintf(f, "PREFIX,str_flat,strength_flat,2,2,10,10,10,10,10\n");
    fprintf(f, "PREFIX,dex_flat,dexterity_flat,3,3,10,10,10,10,10\n");
    fprintf(f, "PREFIX,vit_flat,vitality_flat,4,4,10,10,10,10,10\n");
    fprintf(f, "PREFIX,int_flat,intelligence_flat,5,5,10,10,10,10,10\n");
    fprintf(f, "SUFFIX,armor_flat,armor_flat,6,6,10,10,10,10,10\n");
    fclose(f);
    int loaded = rogue_affixes_load_from_cfg(path);
    assert(loaded == 5);
}

/* Create two items with implicit stats in legacy CFG format. */
static void make_cfg_items(void)
{
    rogue_item_defs_reset();
    const char* path = "parity_items.cfg";
    FILE* f = fopen(path, "wb");
    /* Columns (see loot_item_defs.c):
       id,name,category,level_req,stack_max,base_value,base_damage_min,base_damage_max,base_armor,
       sheet,tx,ty,tw,th,rarity,flags,
       implicit_strength,implicit_dexterity,implicit_intelligence,implicit_vitality,implicit_armor_flat
     */
    fprintf(f, "parity_blade_cfg,BladeCFG,2,1,1,10,4,6,0,sheet.png,0,0,1,1,0,0,1,2,0,3,5\n");
    fprintf(f, "parity_helm_cfg,HelmCFG,3,1,1,8,0,0,1,sheet.png,0,0,1,1,0,0,0,0,0,4,7\n");
    fclose(f);
    int added = rogue_item_defs_load_from_cfg(path);
    assert(added == 2);
}

/* JSON equivalents of the above items (different IDs, same numeric data). */
static void make_json_items(void)
{
    rogue_item_defs_reset();
    const char* path = "parity_items.json";
    FILE* f = fopen(path, "wb");
    fprintf(f, "[\n");
    fprintf(f,
            " {\"id\":\"parity_blade_json\",\"name\":\"BladeJSON\",\"category\":2,\"level_req\":1,"
            "\"stack_max\":1,\"base_value\":10,\"base_damage_min\":4,\"base_damage_max\":6,\"base_"
            "armor\":0,"
            "\"sprite_path\":\"sheet.png\",\"sprite_tx\":0,\"sprite_ty\":0,\"sprite_tw\":1,"
            "\"sprite_th\":1,"
            "\"rarity\":0,\"flags\":0,\"implicit_strength\":1,\"implicit_dexterity\":2,\"implicit_"
            "intelligence\":0,\"implicit_vitality\":3,\"implicit_armor_flat\":5 },\n");
    fprintf(f, " {\"id\":\"parity_helm_json\",\"name\":\"HelmJSON\",\"category\":3,\"level_req\":1,"
               "\"stack_max\":1,\"base_value\":8,\"base_damage_min\":0,\"base_damage_max\":0,"
               "\"base_armor\":1,"
               "\"sprite_path\":\"sheet.png\",\"sprite_tx\":0,\"sprite_ty\":0,\"sprite_tw\":1,"
               "\"sprite_th\":1,"
               "\"rarity\":0,\"flags\":0,\"implicit_strength\":0,\"implicit_dexterity\":0,"
               "\"implicit_intelligence\":0,\"implicit_vitality\":4,\"implicit_armor_flat\":7 }\n");
    fprintf(f, "]\n");
    fclose(f);
    int added = rogue_item_defs_load_from_json(path);
    assert(added == 2);
}

static int spawn_with(const char* id, const char* prefix, const char* suffix)
{
    int def = rogue_item_def_index(id);
    assert(def >= 0);
    int inst = rogue_items_spawn(def, 1, 0.f, 0.f);
    assert(inst >= 0);
    RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst);
    assert(it);
    if (prefix)
    {
        int ai = rogue_affix_index(prefix);
        assert(ai >= 0);
        it->prefix_index = ai;
        it->prefix_value = rogue_affix_at(ai)->min_value;
    }
    if (suffix)
    {
        int ai = rogue_affix_index(suffix);
        assert(ai >= 0);
        it->suffix_index = ai;
        it->suffix_value = rogue_affix_at(ai)->min_value;
    }
    return inst;
}

typedef struct AggSnapshot
{
    int total_str, total_dex, total_vit, total_int;
    int affix_str, affix_dex, affix_vit, affix_int;
    int implicit_str, implicit_dex, implicit_vit, implicit_int;
    int armor_flat;
} AggSnapshot;

static AggSnapshot capture(RoguePlayer* p)
{
    rogue_equipment_apply_stat_bonuses(p);
    rogue_stat_cache_mark_dirty();
    rogue_stat_cache_force_update(p);
    AggSnapshot s = {0};
    s.total_str = g_player_stat_cache.total_strength;
    s.total_dex = g_player_stat_cache.total_dexterity;
    s.total_vit = g_player_stat_cache.total_vitality;
    s.total_int = g_player_stat_cache.total_intelligence;
    s.affix_str = g_player_stat_cache.affix_strength;
    s.affix_dex = g_player_stat_cache.affix_dexterity;
    s.affix_vit = g_player_stat_cache.affix_vitality;
    s.affix_int = g_player_stat_cache.affix_intelligence;
    s.implicit_str = g_player_stat_cache.implicit_strength;
    s.implicit_dex = g_player_stat_cache.implicit_dexterity;
    s.implicit_vit = g_player_stat_cache.implicit_vitality;
    s.implicit_int = g_player_stat_cache.implicit_intelligence;
    s.armor_flat = g_player_stat_cache.affix_armor_flat; /* shared field */
    return s;
}

static void assert_equal(const AggSnapshot* a, const AggSnapshot* b)
{
    assert(a->total_str == b->total_str);
    assert(a->total_dex == b->total_dex);
    assert(a->total_vit == b->total_vit);
    assert(a->total_int == b->total_int);
    assert(a->affix_str == b->affix_str);
    assert(a->affix_dex == b->affix_dex);
    assert(a->affix_vit == b->affix_vit);
    assert(a->affix_int == b->affix_int);
    assert(a->implicit_str == b->implicit_str);
    assert(a->implicit_dex == b->implicit_dex);
    assert(a->implicit_vit == b->implicit_vit);
    assert(a->implicit_int == b->implicit_int);
    assert(a->armor_flat == b->armor_flat);
}

int main(void)
{
    seed_affixes();

    /* CFG path */
    make_cfg_items();
    RoguePlayer player_cfg = {0};
    player_cfg.strength = player_cfg.dexterity = player_cfg.vitality = player_cfg.intelligence = 10;
    int blade_cfg = spawn_with("parity_blade_cfg", "str_flat", "dex_flat");
    int helm_cfg = spawn_with("parity_helm_cfg", "vit_flat", "armor_flat");
    assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, blade_cfg) == 0);
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_HEAD, helm_cfg) == 0);
    AggSnapshot snap_cfg = capture(&player_cfg);

    /* JSON path */
    make_json_items();
    rogue_equip_reset();
    g_player_stat_cache.affix_strength = g_player_stat_cache.affix_dexterity = 0;
    g_player_stat_cache.affix_vitality = g_player_stat_cache.affix_intelligence = 0;
    g_player_stat_cache.affix_armor_flat = 0;
    RoguePlayer player_json = {0};
    player_json.strength = player_json.dexterity = player_json.vitality = player_json.intelligence =
        10;
    int blade_json = spawn_with("parity_blade_json", "str_flat", "dex_flat");
    int helm_json = spawn_with("parity_helm_json", "vit_flat", "armor_flat");
    assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, blade_json) == 0);
    assert(rogue_equip_try(ROGUE_EQUIP_ARMOR_HEAD, helm_json) == 0);
    AggSnapshot snap_json = capture(&player_json);

    assert_equal(&snap_cfg, &snap_json);
    printf("phase2_4_stat_aggregation_parity_ok\n");
    return 0;
}
