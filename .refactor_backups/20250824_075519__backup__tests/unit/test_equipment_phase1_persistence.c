/* Equipment System Phase 1.7 Tests:
 * - Multi-slot atomicity: equipping two-handed weapon clears OFFHAND; equipping shield after
 * two-hand fails; swapping back to one-hand allows OFFHAND.
 * - Persistence roundtrip: equip items across several slots, save, reset runtime, reload, verify
 * equipped instance indices restored. Test uses dedicated test_equipment_items.cfg with a flagged
 * two-handed weapon and a shield.
 */
#include "../../src/core/app/app_state.h"
#include "../../src/core/equipment/equipment.h"
#include "../../src/core/loot/loot_instances.h"
#include "../../src/core/loot/loot_item_defs.h"
#include "../../src/core/path_utils.h"
#include "../../src/core/save_manager.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

RogueAppState g_app;
RoguePlayer g_exposed_player_for_stats;
void rogue_player_recalc_derived(RoguePlayer* p) { (void) p; }

static int load_defs(void)
{
    char p[256];
    if (!rogue_find_asset_path("test_equipment_items.cfg", p, sizeof p))
        return -1;
    if (rogue_item_defs_load_from_cfg(p) <= 0)
        return -2;
    return 0;
}

static int find_def(const char* id) { return rogue_item_def_index(id); }

static void register_save_components(void)
{
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
}

static void spawn_and_basic_setup(void) { rogue_items_init_runtime(); }

static void test_atomicity(void)
{
    rogue_equip_reset();
    int gs = find_def("greatsword");
    int shield = find_def("shield_small");
    int sword =
        find_def("long_sword"); /* long_sword may come from base test_items if loaded elsewhere */
    if (gs >= 0)
    {
        int inst_gs = rogue_items_spawn(gs, 1, 0, 0);
        assert(inst_gs >= 0);
        int rc = rogue_equip_try(ROGUE_EQUIP_WEAPON, inst_gs);
        assert(rc == 0);
        assert(rogue_equip_get(ROGUE_EQUIP_OFFHAND) == -1); /* two-hand clears offhand */
        /* Attempt to equip shield should fail while two-hand weapon equipped */
        if (shield >= 0)
        {
            int inst_sh = rogue_items_spawn(shield, 1, 0, 0);
            int rcs = rogue_equip_try(ROGUE_EQUIP_OFFHAND, inst_sh);
            assert(rcs != 0);
        }
    }
    /* Swap to one-hand weapon (if available) then equip shield */
    if (sword >= 0)
    {
        int inst_sw = rogue_items_spawn(sword, 1, 0, 0);
        assert(inst_sw >= 0);
        int rcw = rogue_equip_try(ROGUE_EQUIP_WEAPON, inst_sw);
        assert(rcw == 0);
    }
    if (shield >= 0)
    {
        int inst_sh2 = rogue_items_spawn(shield, 1, 0, 0);
        int rco = rogue_equip_try(ROGUE_EQUIP_OFFHAND, inst_sh2);
        assert(rco == 0);
    }
}

static void test_persistence_roundtrip(void)
{
    rogue_equip_reset();
    int gs = find_def("greatsword");
    int shield = find_def("shield_small");
    int ring_def = -1; /* attempt to locate any armor to stand in for ring (category reuse) */
    for (int i = 0; i < rogue_item_defs_count(); i++)
    {
        const RogueItemDef* d = rogue_item_def_at(i);
        if (d && d->category == ROGUE_ITEM_ARMOR)
        {
            ring_def = i;
            break;
        }
    }
    int inst_gs = -1, inst_sh = -1, inst_ring = -1;
    if (gs >= 0)
    {
        inst_gs = rogue_items_spawn(gs, 1, 0, 0);
        assert(inst_gs >= 0);
        assert(rogue_equip_try(ROGUE_EQUIP_WEAPON, inst_gs) == 0);
    }
    if (shield >= 0)
    { /* should fail because greatsword two-hand */
        int tmp = rogue_items_spawn(shield, 1, 0, 0);
        assert(tmp >= 0);
        int rcf = rogue_equip_try(ROGUE_EQUIP_OFFHAND, tmp);
        assert(rcf != 0);
    }
    if (ring_def >= 0)
    {
        inst_ring = rogue_items_spawn(ring_def, 1, 0, 0);
        assert(inst_ring >= 0); /* equip ring into RING1 slot (category presently armor) */
        assert(rogue_equip_try(ROGUE_EQUIP_RING1, inst_ring) == 0);
    }
    /* Save */
    int save_rc = rogue_save_manager_save_slot(0);
    assert(save_rc == 0);
    /* Capture expected equipped instances */
    int expected[ROGUE_EQUIP__COUNT];
    for (int s = 0; s < ROGUE_EQUIP__COUNT; s++)
        expected[s] = rogue_equip_get((enum RogueEquipSlot) s);
    /* Simulate restart: reset equipment + item defs + instances and reload defs then load slot */
    rogue_equip_reset();
    rogue_item_defs_reset();
    char ptest[256];
    assert(rogue_find_asset_path("test_equipment_items.cfg", ptest, sizeof ptest));
    assert(rogue_item_defs_load_from_cfg(ptest) >
           0); /* reload only equipment test defs (other base defs absent) */
    /* load */
    int load_rc = rogue_save_manager_load_slot(0);
    assert(load_rc == 0);
    for (int s = 0; s < ROGUE_EQUIP__COUNT; s++)
    {
        int got = rogue_equip_get(
            (enum RogueEquipSlot) s); /* If def missing, instance spawn earlier may become invalid,
                                         so only assert weapon slot equality */
        if (s == ROGUE_EQUIP_WEAPON || s == ROGUE_EQUIP_RING1)
        {
            assert(got == expected[s]);
        }
    }
}

int main(void)
{
    /* Load both baseline test_items (for long_sword) and equipment-specific defs */
    rogue_item_defs_reset();
    char pbase[256];
    if (rogue_find_asset_path("test_items.cfg", pbase, sizeof pbase))
    {
        rogue_item_defs_load_from_cfg(pbase);
    }
    assert(load_defs() == 0);
    spawn_and_basic_setup();
    register_save_components();
    test_atomicity();
    test_persistence_roundtrip();
    printf("equipment_phase1_persistence_ok\n");
    return 0;
}
