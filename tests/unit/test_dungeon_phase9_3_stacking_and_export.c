#include "../../src/world/world_gen.h"
#include <stdio.h>
#include <string.h>

static void seed_ctx(RogueWorldGenContext* ctx, unsigned int seed)
{
    RogueWorldGenConfig cfg;
    memset(&cfg, 0, sizeof cfg);
    cfg.seed = seed;
    rogue_worldgen_context_init(ctx, &cfg);
}

int main(void)
{
    /* Prepare registry */
    rogue_mutator_clear_registry();
    rogue_mutator_clear_rules();
    RogueMutatorDesc a = {0};
    strncpy(a.id, "alpha", sizeof a.id - 1);
    a.weight = 3.0f;
    a.reward_multiplier = 1.1f;
    strncpy(a.effect_dsl, "noop", sizeof a.effect_dsl - 1);
    strncpy(a.group, "hazards", sizeof a.group - 1);
    RogueMutatorDesc b = {0};
    strncpy(b.id, "beta", sizeof b.id - 1);
    b.weight = 2.0f;
    b.reward_multiplier = 1.2f;
    strncpy(b.effect_dsl, "noop", sizeof b.effect_dsl - 1);
    strncpy(b.group, "hazards", sizeof b.group - 1);
    RogueMutatorDesc c = {0};
    strncpy(c.id, "gamma", sizeof c.id - 1);
    c.weight = 1.0f;
    c.reward_multiplier = 1.3f;
    strncpy(c.effect_dsl, "noop", sizeof c.effect_dsl - 1);
    strncpy(c.group, "economy", sizeof c.group - 1);
    if (rogue_mutator_register(&a) < 0 || rogue_mutator_register(&b) < 0 ||
        rogue_mutator_register(&c) < 0)
        return 1;

    /* Define rules: alpha incompatible with gamma; hazards group cap=1 */
    if (!rogue_mutator_define_incompatible("alpha", "gamma"))
        return 2;
    if (!rogue_mutator_define_group_cap("hazards", 1))
        return 3;

    RogueWorldGenContext ctx;
    seed_ctx(&ctx, 42u);
    int out[4] = {0};
    int n = rogue_mutator_roll_k_choose_n(&ctx, 3, 2, out, 4);
    if (n != 2)
        return 4;
    /* Enforce rules */
    if (!rogue_mutator_is_compatible_set(out, n))
        return 5;
    const char* id0 = rogue_mutator_get_desc(out[0])->id;
    const char* id1 = rogue_mutator_get_desc(out[1])->id;
    /* Should not pick both alpha and beta together due to hazards cap=1 */
    if ((strcmp(id0, "alpha") == 0 && strcmp(id1, "beta") == 0) ||
        (strcmp(id0, "beta") == 0 && strcmp(id1, "alpha") == 0))
        return 6;
    /* Should not include both alpha and gamma due to incompat */
    if ((strcmp(id0, "alpha") == 0 && strcmp(id1, "gamma") == 0) ||
        (strcmp(id0, "gamma") == 0 && strcmp(id1, "alpha") == 0))
        return 7;

    /* Export depth profile to a temp file */
    const char* path = "build/depth_profile_test.json";
    if (!rogue_dungeon_export_depth_profile(path, 10))
        return 8;
    /* Optionally, we could read it back; basic existence is enough here. */
    FILE* f = fopen(path, "rb");
    if (!f)
        return 9;
    fclose(f);

    return 0;
}
