/* Equipment Phase 2.3 Resist breakdown + soft cap integration test */
#include "../../src/core/app/app_state.h"
#include "../../src/core/equipment/equipment.h"
#include "../../src/core/equipment/equipment_stats.h"
#include "../../src/core/loot/loot_affixes.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/stat_cache.h"
#include <assert.h>
#include <stdio.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

static void seed_affixes(void)
{
    rogue_affixes_reset();
    FILE* f = fopen("affix_resist_tmp.cfg", "wb");
    /* Provide resist affixes with deterministic values (all min=max). */
    fprintf(f, "PREFIX,rphys,resist_physical,30,30,10,10,10,10,10\n");
    fprintf(f, "PREFIX,rfire,resist_fire,40,40,10,10,10,10,10\n");
    fprintf(f, "PREFIX,rcold,resist_cold,50,50,10,10,10,10,10\n");
    fprintf(f, "PREFIX,rlight,resist_lightning,80,80,10,10,10,10,10\n");
    fprintf(f, "PREFIX,rpoison,resist_poison,95,95,10,10,10,10,10\n");
    fprintf(f, "PREFIX,rstatus,resist_status,120,120,10,10,10,10,10\n");
    fclose(f);
    rogue_affixes_load_from_cfg("affix_resist_tmp.cfg");
}

static void make_item_defs(void)
{
    rogue_item_defs_reset();
    FILE* f = fopen("item_resist_tmp.cfg", "wb");
    fprintf(f, "ring_a,RingA,3,1,1,5,0,0,0,sheet.png,0,0,1,1,1\n");
    fprintf(f, "ring_b,RingB,3,1,1,5,0,0,0,sheet.png,0,0,1,1,1\n");
    fprintf(f, "ring_c,RingC,3,1,1,5,0,0,0,sheet.png,0,0,1,1,1\n");
    fprintf(f, "ring_d,RingD,3,1,1,5,0,0,0,sheet.png,0,0,1,1,1\n");
    fprintf(f, "ring_e,RingE,3,1,1,5,0,0,0,sheet.png,0,0,1,1,1\n");
    fprintf(f, "ring_f,RingF,3,1,1,5,0,0,0,sheet.png,0,0,1,1,1\n");
    fclose(f);
    rogue_item_defs_load_from_cfg("item_resist_tmp.cfg");
}

static int spawn_item(const char* id, const char* affix)
{
    int def = rogue_item_def_index(id);
    assert(def >= 0);
    int inst = rogue_items_spawn(def, 1, 0, 0);
    assert(inst >= 0);
    if (affix)
    {
        int ai = rogue_affix_index(affix);
        assert(ai >= 0);
        RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst);
        it->prefix_index = ai;
        it->prefix_value = rogue_affix_at(ai)->min_value;
    }
    return inst;
}

static void test_resist_softcaps(void)
{
    RoguePlayer player = {0};
    player.max_health = 100;
    player.strength = 5;
    player.dexterity = 5;
    player.vitality = 5;
    player.intelligence = 5;
    player.crit_chance = 5;
    player.crit_damage = 150;
    int i1 = spawn_item("ring_a", "rphys");
    int i2 = spawn_item("ring_b", "rfire");
    int i3 = spawn_item("ring_c", "rcold");
    int i4 = spawn_item("ring_d", "rlight");
    int i5 = spawn_item("ring_e", "rpoison");
    int i6 = spawn_item("ring_f", "rstatus");
    assert(rogue_equip_try(ROGUE_EQUIP_RING1, i1) == 0);
    assert(rogue_equip_try(ROGUE_EQUIP_RING2, i2) == 0);
    /* Use amulet/belt/cloak/charm slots for remaining (all armor category currently) */
    assert(rogue_equip_try(ROGUE_EQUIP_AMULET, i3) == 0);
    assert(rogue_equip_try(ROGUE_EQUIP_BELT, i4) == 0);
    assert(rogue_equip_try(ROGUE_EQUIP_CLOAK, i5) == 0);
    assert(rogue_equip_try(ROGUE_EQUIP_CHARM1, i6) == 0);
    rogue_equipment_apply_stat_bonuses(&player);
    rogue_stat_cache_mark_dirty();
    rogue_stat_cache_force_update(&player);
    /* Raw values: 30,40,50,80,95,120 -> after soft cap (75) + diminishing + hard cap (90) */
    assert(g_player_stat_cache.resist_physical == 30);
    assert(g_player_stat_cache.resist_fire == 40);
    assert(g_player_stat_cache.resist_cold == 50);
    assert(g_player_stat_cache.resist_lightning <= 85 &&
           g_player_stat_cache.resist_lightning >= 75); /* diminished from 80 */
    assert(g_player_stat_cache.resist_poison <= 90);    /* capped from 95 */
    assert(g_player_stat_cache.resist_status == 90);    /* 120 reduced to hard cap 90 */
}

int main(void)
{
    seed_affixes();
    make_item_defs();
    test_resist_softcaps();
    printf("phase2_resists_ok\n");
    return 0;
}
