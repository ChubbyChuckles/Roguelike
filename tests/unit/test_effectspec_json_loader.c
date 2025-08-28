#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include "../../src/graphics/effect_spec_load.h"
#include <assert.h>
#include <stdio.h>

static const char* JSON = "["
                          "{\"kind\":\"STAT_BUFF\",\"buff_type\":\"STAT_STRENGTH\",\"magnitude\":3,"
                          "\"duration_ms\":500,\"stack_rule\":\"ADD\"},"
                          "{\"kind\":\"STAT_BUFF\",\"buff_type\":\"POWER_STRIKE\",\"magnitude\":"
                          "200,\"duration_ms\":1000,\"stack_rule\":\"MULTIPLY\"},"
                          "{\"kind\":\"DOT\",\"damage_type\":\"FIRE\",\"magnitude\":7,\"duration_"
                          "ms\":900,\"pulse_period_ms\":300,\"crit_mode\":1,\"crit_chance_pct\":25}"
                          "]";

int main(void)
{
    rogue_buffs_init();
    /* Disable dampening so rapid re-applies stack additively without decay. */
    rogue_buffs_set_dampening(0.0);
    rogue_effect_reset();
    int ids[8];
    int n = rogue_effects_load_from_json_text(JSON, ids, 8);
    assert(n == 3);
    /* stacking: apply +3 STR twice -> 6 */
    rogue_effect_apply(ids[0], 0.0);
    rogue_effect_apply(ids[0], 1.0);
    int str_total = rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH);
    printf("STR total after two applies: %d\n", str_total);
    assert(str_total == 6);
    /* multiplicative POWER_STRIKE requires baseline */
    rogue_effect_apply(ids[1], 2.0);
    int ps_total = rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE);
    printf("POWER_STRIKE after first MULTIPLY with no baseline: %d\n", ps_total);
    assert(ps_total == 0);
    rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE, 10, 1000.0, 3.0, ROGUE_BUFF_STACK_ADD, 0);
    rogue_effect_apply(ids[1], 4.0);
    ps_total = rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE);
    printf("POWER_STRIKE after MULTIPLY with baseline 10: %d\n", ps_total);
    assert(ps_total == 20);
    puts("EFFECTSPEC_JSON_LOADER_OK");
    return 0;
}
