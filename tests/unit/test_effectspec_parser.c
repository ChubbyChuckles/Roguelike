/* test_effectspec_parser.c - Unit tests for EffectSpec parser (Phase 3.1) */
#include "../../src/game/buffs.h"
#include "../../src/graphics/effect_spec.h"
#include "../../src/graphics/effect_spec_parser.h"
#include <assert.h>
#include <stdio.h>

static const char* TEXT = "# two effects\n"
                          "effect.0.kind = STAT_BUFF\n"
                          "effect.0.buff_type = STAT_STRENGTH\n"
                          "effect.0.magnitude = 2\n"
                          "effect.0.duration_ms = 200\n"
                          "effect.0.stack_rule = ADD\n"
                          "effect.1.kind = STAT_BUFF\n"
                          "effect.1.buff_type = POWER_STRIKE\n"
                          "effect.1.magnitude = 150\n"
                          "effect.1.duration_ms = 1000\n"
                          "effect.1.stack_rule = MULTIPLY\n";

int main(void)
{
    rogue_effect_reset();
    rogue_buffs_init();
    /* Disable dampening so rapid re-applies in this test stack as expected. */
    rogue_buffs_set_dampening(0.0);
    char err[128];
    int ids[4];
    int n = rogue_effects_parse_text(TEXT, ids, 4, err, sizeof err);
    assert(n == 2);
    double now = 0.0;
    /* apply effect 0 twice -> +2 then +2 => total 4 */
    rogue_effect_apply(ids[0], now);
    rogue_effect_apply(ids[0], now + 10.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_STAT_STRENGTH) == 4);
    /* apply multiplicative 150% to POWER_STRIKE=0 -> remains 0 (no base); now add 10 -> 15 */
    rogue_effect_apply(ids[1], now + 20.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE) == 0);
    /* add baseline */
    rogue_buffs_apply(ROGUE_BUFF_POWER_STRIKE, 10, 1000.0, now + 21.0, ROGUE_BUFF_STACK_ADD, 0);
    rogue_effect_apply(ids[1], now + 22.0);
    assert(rogue_buffs_get_total(ROGUE_BUFF_POWER_STRIKE) == 15);
    puts("EFFECTSPEC_PARSER_OK");
    return 0;
}
