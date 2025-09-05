#include "../../src/core/vendor/vendor_crafting_integration.h"
#include "../../src/core/vendor/vendor_run_summary_consumer.h"
#include "../../src/world/world_gen.h"
#include <stdio.h>

static int test_run(void)
{
    rogue_vendor_run_summary_listeners_init();
    /* fabricate two mutators indices (not used by consumer directly) */
    int idx[2] = {0, 1};
    /* Fire event with high reward multiplier to trigger scarcity nudge. */
    rogue_dungeon_emit_run_summary(idx, 2, 1.5f);
    /* No observable assertion yet; success if no crash and listener wired. */
    rogue_vendor_run_summary_listeners_shutdown();
    return 0;
}

int main(void) { return test_run(); }
