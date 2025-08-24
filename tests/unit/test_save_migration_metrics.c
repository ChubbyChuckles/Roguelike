/* Simplified metrics test: validates baseline (no migration needed) metrics remain zero and
 * non-negative timings */
#include "../../src/core/persistence/save_manager.h"
#include <stdio.h>

int main(void)
{
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("MIG_METRIC_FAIL save\n");
        return 1;
    }
    if (rogue_save_manager_load_slot(0) != 0)
    {
        printf("MIG_METRIC_FAIL load\n");
        return 1;
    }
    if (rogue_save_last_migration_failed())
    {
        printf("MIG_METRIC_FAIL unexpected_failed_flag\n");
        return 1;
    }
    if (rogue_save_last_migration_steps() != 0)
    {
        printf("MIG_METRIC_FAIL steps=%d expected0\n", rogue_save_last_migration_steps());
        return 1;
    }
    double ms = rogue_save_last_migration_ms();
    if (ms < 0.0)
    {
        printf("MIG_METRIC_FAIL ms=%f\n", ms);
        return 1;
    }
    printf("MIG_METRIC_OK steps=%d ms=%.3f failed=%d\n", rogue_save_last_migration_steps(), ms,
           rogue_save_last_migration_failed());
    return 0;
}
