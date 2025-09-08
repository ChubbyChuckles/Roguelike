#include "../../src/core/persistence/save_manager.h"
#include "../../src/core/vendor/economy_version.h"
#include <stdio.h>

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    extern void rogue_register_core_save_components(void);
    rogue_register_core_save_components();

    /* Set a non-zero header */
    rogue_economy_version_reset();
    rogue_economy_version_set(3u, 7u);

    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("FAIL:save\n");
        return 1;
    }

    /* Mutate runtime to ensure load restores */
    rogue_economy_version_set(0u, 0u);

    if (rogue_save_manager_load_slot(0) != 0)
    {
        printf("FAIL:load\n");
        return 2;
    }

    RogueEconomyHeader hdr = rogue_economy_version_get();
    if (hdr.curve_version != 3u || hdr.margin_policy_version != 7u)
    {
        printf("FAIL:mismatch cv=%u mv=%u\n", hdr.curve_version, hdr.margin_policy_version);
        return 3;
    }

    printf("OK:save_phase13_2_economy_header_roundtrip\n");
    return 0;
}
