#include "vendor_run_summary_consumer.h"
#include "../../world/world_gen.h"
#include "../crafting/material_registry.h"
#include "vendor_crafting_integration.h"
#include <stdio.h>
#include <string.h>

static void on_run_summary(const char* mutator_manifest_csv, float reward_multiplier_accum,
                           void* user)
{
    (void) user;
    /* Minimal consumer: log the event and nudge scarcity on rare crafting materials when
       reward multiplier is high (placeholder heuristic to exercise callback plumbing). */
    fprintf(stderr, "vendor: run_summary manifest=%s reward_mul=%.2f\n", mutator_manifest_csv,
            reward_multiplier_accum);

    if (reward_multiplier_accum > 1.25f)
    {
        /* Nudge a few known rare materials if present (ids by registry index). Keep tiny impact. */
        int rare_ids[3] = {0, 1, 2};
        int cap = rogue_material_count();
        for (int i = 0; i < 3; ++i)
        {
            int id = (i < cap) ? rare_ids[i] : -1;
            if (id >= 0)
                rogue_vendor_scarcity_record(id, 1);
        }
    }
}

static int g_registered = 0;

void rogue_vendor_run_summary_listeners_init(void)
{
    if (g_registered)
        return;
    if (rogue_dungeon_register_run_summary_callback(on_run_summary, NULL))
        g_registered = 1;
}

void rogue_vendor_run_summary_listeners_shutdown(void)
{
    if (!g_registered)
        return;
    rogue_dungeon_clear_run_summary_callbacks();
    g_registered = 0;
}
