/* Phase 11.5-11.6: Proc/DR oversaturation flags + A/B harness */
#define SDL_MAIN_HANDLED 1
#include "core/equipment/equipment_balance.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_proc_oversaturation(void)
{
    rogue_equipment_analytics_reset();
    /* Default threshold 20 triggers oversaturation when exceeded */
    for (int i = 0; i < 25; i++)
        rogue_equipment_analytics_record_proc_trigger(1);
    rogue_equipment_analytics_analyze();
    assert(rogue_equipment_analytics_flag_proc_oversat() == 1);
}

static void test_dr_chain_flag(void)
{
    rogue_equipment_analytics_reset();
    /* Apply several DR sources to push remaining fraction below 0.2 (default floor) */
    rogue_equipment_analytics_record_dr_source(50.f); /* remain 0.5 */
    rogue_equipment_analytics_record_dr_source(60.f); /* remain 0.5 * 0.4 = 0.2 */
    rogue_equipment_analytics_record_dr_source(10.f); /* remain 0.18 < 0.2 => flag */
    rogue_equipment_analytics_analyze();
    assert(rogue_equipment_analytics_flag_dr_chain() == 1);
}

static void test_variant_selection(void)
{
    rogue_equipment_analytics_reset();
    RogueBalanceParams vA = {0};
    strcpy(vA.id, "A");
    vA.outlier_mad_mult = 4;
    vA.proc_oversat_threshold = 10;
    vA.dr_chain_floor = 0.25f;
    RogueBalanceParams vB = {0};
    strcpy(vB.id, "B");
    vB.outlier_mad_mult = 6;
    vB.proc_oversat_threshold = 30;
    vB.dr_chain_floor = 0.15f;
    int ia = rogue_balance_register(&vA);
    assert(ia >= 0);
    int ib = rogue_balance_register(&vB);
    assert(ib >= 0 && ib != ia);
    int sel1 = rogue_balance_select_deterministic(12345u);
    int sel2 = rogue_balance_select_deterministic(12345u);
    assert(sel1 == sel2); /* deterministic */
    const RogueBalanceParams* cur = rogue_balance_current();
    assert(cur != NULL);
    /* Use selected config thresholds */
    for (int i = 0; i < cur->proc_oversat_threshold + 1; i++)
        rogue_equipment_analytics_record_proc_trigger(1);
    rogue_equipment_analytics_analyze();
    assert(rogue_equipment_analytics_flag_proc_oversat() == 1);
}

int main(void)
{
    test_proc_oversaturation();
    test_dr_chain_flag();
    test_variant_selection();
    char buf[128];
    int n = rogue_equipment_analytics_export_json(buf, sizeof buf);
    assert(n > 0);
    assert(strstr(buf, "proc_oversaturation"));
    printf("equipment_phase11_balance_ok\n");
    return 0;
}
