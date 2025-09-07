#include "../../src/world/world_gen.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char* kJson = "[\n"
                           "  { \"id\": \"more_traps\", \"weight\": 2.0, \"reward_multiplier\": "
                           "1.25, \"effect\": \"trap_dmg%:+20\" },\n"
                           "  { \"id\": \"fast_enemies\", \"weight\": 1.0, \"reward_multiplier\": "
                           "1.10, \"effect\": \"enemy_spd%:+10\" }\n"
                           "]\n";

int main(void)
{
    char err[128] = {0};
    rogue_mutator_clear_registry();
    int added = rogue_mutator_registry_load_json_text(kJson, err, sizeof err);
    if (added <= 0)
    {
        fprintf(stderr, "loader failed: %s\n", err);
        return 1;
    }
    if (rogue_mutator_registry_count() != 2)
    {
        fprintf(stderr, "count mismatch: %d\n", rogue_mutator_registry_count());
        return 2;
    }
    int idx = rogue_mutator_registry_find("more_traps");
    assert(idx >= 0);
    const RogueMutatorDesc* d = rogue_mutator_get_desc(idx);
    if (!d || strcmp(d->id, "more_traps") != 0)
        return 3;
    if (d->reward_multiplier < 1.24f || d->reward_multiplier > 1.26f)
        return 4;
    return 0;
}
