#include "../../src/graphics/effect_spec.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    rogue_effect_reset();
    RogueEffectSpec b = {0};
    b.kind = ROGUE_EFFECT_STAT_BUFF;
    b.magnitude = 5;
    b.duration_ms = 1000.0f;
    int idb = rogue_effect_register(&b);
    assert(idb >= 0);
    assert(rogue_effect_spec_is_debuff(idb) == 0);

    RogueEffectSpec d = {0};
    d.kind = ROGUE_EFFECT_DOT;
    d.magnitude = 10;
    d.damage_type = 0;
    int idd = rogue_effect_register(&d);
    assert(idd >= 0);
    assert(rogue_effect_spec_is_debuff(idd) == 1);

    RogueEffectSpec e = {0};
    e.kind = ROGUE_EFFECT_STAT_BUFF;
    e.debuff = 1;
    e.magnitude = 1;
    int ide = rogue_effect_register(&e);
    assert(rogue_effect_spec_is_debuff(ide) == 1);

    puts("EFFECTSPEC_DEBUFF_FLAG_OK");
    return 0;
}
