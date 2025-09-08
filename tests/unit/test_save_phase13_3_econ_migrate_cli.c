#include "../../src/core/persistence/save_manager.h"
#include "../../src/core/vendor/economy_version.h"
#include "../../src/core/vendor/vendor_buyback.h"
#include "../../src/core/vendor/vendor_pricing.h"
#include <stdio.h>
#include <string.h>

/* Call the tool logic directly by defining ECON_MIGRATE_NO_STANDALONE and declaring
 * econ_migrate_main. */
#define ECON_MIGRATE_NO_STANDALONE 1
int econ_migrate_main(int argc, char** argv);

static int floats_all_zero(const float* a, int n)
{
    for (int i = 0; i < n; ++i)
    {
        if (a[i] != 0.0f)
            return 0;
    }
    return 1;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    extern void rogue_register_core_save_components(void);
    rogue_register_core_save_components();

    /* Seed some dynamic vendor state: record a sale & buyback to perturb demand/scarcity. */
    rogue_vendor_pricing_reset();
    rogue_vendor_pricing_record_sale(2);
    rogue_vendor_pricing_record_buyback(3);

    /* Seed buyback entries to ensure reset clears them. */
    RogueVendorBuybackEntry tmp;
    memset(&tmp, 0, sizeof tmp);
    (void) tmp; /* structure comes from the header; we only need the API to create entries */
    /* Use public API to create an entry */
    (void) rogue_vendor_buyback_record(0, 0xABCULL, 1, 1, 1, 100, 50, 1000u);

    /* Save baseline slot */
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("FAIL:save0\n");
        return 1;
    }

    char* argvv[] = {"econ_migrate", "--slot", "0", "--curve", "5", "--margin", "9"};
    int rc = econ_migrate_main((int) (sizeof(argvv) / sizeof(argvv[0])), argvv);
    if (rc != 0)
    {
        printf("FAIL:econ_migrate rc=%d\n", rc);
        return 2;
    }

    /* Reload and verify header updated */
    if (rogue_save_manager_load_slot(0) != 0)
    {
        printf("FAIL:load\n");
        return 3;
    }
    RogueEconomyHeader hdr = rogue_economy_version_get();
    if (hdr.curve_version != 5u || hdr.margin_policy_version != 9u)
    {
        printf("FAIL:hdr %u/%u\n", hdr.curve_version, hdr.margin_policy_version);
        return 4;
    }

    /* Verify pricing arrays reset to zeros (scalars should be neutral 1.0). We can't directly read
       arrays, but we can check scalars near 1.0 after a reset. */
    float d2 = rogue_vendor_pricing_get_demand_scalar(2);
    float s3 = rogue_vendor_pricing_get_scarcity_scalar(3);
    if (d2 < 0.98f || d2 > 1.02f || s3 < 0.98f || s3 > 1.02f)
    {
        printf("FAIL:pricing_scalars d2=%f s3=%f\n", d2, s3);
        return 5;
    }

    /* Verify buyback cleared: listing should be 0. */
    RogueVendorBuybackEntry bb[8];
    int n = rogue_vendor_buyback_list(0, bb, 8, 2000u);
    if (n != 0)
    {
        printf("FAIL:buyback_not_cleared n=%d\n", n);
        return 6;
    }

    printf("OK:save_phase13_3_econ_migrate_cli\n");
    return 0;
}
