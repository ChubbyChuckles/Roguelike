#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../src/core/enemy/enemy_difficulty.h"
#include "../../src/core/enemy/enemy_modifiers.h"

/* Minimal synthetic modifier config for tests */
static const char* kTempModFile =
    "test_modifiers.cfg"; /* current working dir is build/ when running ctest */

static void write_temp_mod_file(void)
{
    FILE* f = fopen(kTempModFile, "wb");
    assert(f);
    /* id=0 baseline damage buff */
    fprintf(f, "id=0\nname=Frenzied\nweight=2\ntiers=012345\ndps=0.25\ncontrol=0.00\nmobility=0."
               "00\ntelegraph=icon_frenzy\n\n");
    /* id=1 control aura - incompatible with id0 for test (digit string) */
    fprintf(f, "id=1\nname=Chilling "
               "Aura\nweight=1\ntiers=12345\ndps=0.05\ncontrol=0.30\nincompat=0\ntelegraph=icon_"
               "chill\n\n");
    /* id=2 mobility dash */
    fprintf(f,
            "id=2\nname=Blinkstep\nweight=1\ntiers=2345\nmobility=0.40\ntelegraph=icon_dash\n\n");
    /* id=3 high dps cannot stack with 0 */
    fprintf(f, "id=3\nname=Overcharged\nweight=0.5\ntiers=345\ndps=0.40\nincompat=0\ntelegraph="
               "icon_over\n\n");
    fclose(f);
}

static void test_load_and_introspect(void)
{
    write_temp_mod_file();
    int n = rogue_enemy_modifiers_load_file(kTempModFile);
    assert(n == 4);
    assert(rogue_enemy_modifier_count() == 4);
    const RogueEnemyModifierDef* m0 = rogue_enemy_modifier_by_id(0);
    assert(m0 && strcmp(m0->name, "Frenzied") == 0);
}

static void test_roll_determinism(void)
{
    RogueEnemyModifierSet a, b;
    rogue_enemy_modifiers_roll(1234u, 2, 0.6f, &a);
    rogue_enemy_modifiers_roll(1234u, 2, 0.6f, &b);
    assert(a.count == b.count);
    for (int i = 0; i < a.count; i++)
    {
        assert(a.defs[i]->id == b.defs[i]->id);
    }
}

static int contains(const RogueEnemyModifierSet* s, int id)
{
    for (int i = 0; i < s->count; i++)
        if (s->defs[i]->id == id)
            return 1;
    return 0;
}

static void test_incompat_and_budget(void)
{
    RogueEnemyModifierSet set;
    rogue_enemy_modifiers_roll(4321u, 3, 0.5f, &set); /* limit budgets to 0.5 */
    /* No combined dps > 0.5 and incompat not violated */
    assert(set.total_dps_cost <= 0.5001f);
    assert(set.total_control_cost <= 0.5001f);
    assert(set.total_mobility_cost <= 0.5001f);
    /* If both Frenzied(0) and Overcharged(3) present that's a violation */
    assert(!(contains(&set, 0) && contains(&set, 3)));
}

int main(void)
{
    test_load_and_introspect();
    test_roll_determinism();
    test_incompat_and_budget();
    printf("OK test_enemy_difficulty_phase2 (%d mods)\n", rogue_enemy_modifier_count());
    return 0;
}
