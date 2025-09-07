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
    rogue_mutator_clear_registry();
    RogueMutatorDesc a = {0};
    strncpy(a.id, "alpha", sizeof a.id - 1);
    a.weight = 3.0f;
    a.reward_multiplier = 1.1f;
    strncpy(a.effect_dsl, "noop", sizeof a.effect_dsl - 1);
    RogueMutatorDesc b = {0};
    strncpy(b.id, "beta", sizeof b.id - 1);
    b.weight = 2.0f;
    b.reward_multiplier = 1.2f;
    strncpy(b.effect_dsl, "noop", sizeof b.effect_dsl - 1);
    RogueMutatorDesc c = {0};
    strncpy(c.id, "gamma", sizeof c.id - 1);
    c.weight = 1.0f;
    c.reward_multiplier = 1.3f;
    strncpy(c.effect_dsl, "noop", sizeof c.effect_dsl - 1);
    if (rogue_mutator_register(&a) < 0 || rogue_mutator_register(&b) < 0 ||
        rogue_mutator_register(&c) < 0)
        return 1;

    RogueWorldGenContext ctx;
    seed_ctx(&ctx, 1234u);
    int out[4] = {0};
    int n = rogue_mutator_roll_k_choose_n(&ctx, 3, 2, out, 4);
    if (n != 2)
        return 2;
    /* Should be sorted lexicographically by id for stability */
    const char* id0 = rogue_mutator_get_desc(out[0])->id;
    const char* id1 = rogue_mutator_get_desc(out[1])->id;
    if (strcmp(id0, id1) >= 0)
        return 3;
    char csv[64];
    if (!rogue_mutator_manifest_csv(out, n, csv, sizeof csv))
        return 4;
    /* simple sanity: contains a comma */
    if (strchr(csv, ',') == NULL)
        return 5;
    return 0;
}
