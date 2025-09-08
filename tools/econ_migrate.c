/* Phase 13.3: Migration tool for value model parameter changes
   Usage:
     econ_migrate --slot <n> --curve <ver> --margin <ver>

   This tool loads a save slot, updates the economy header versions, clears
   dynamic vendor pricing state (demand/scarcity), and clears per-vendor buyback
   buffers to avoid inconsistencies when the value/pricing model changes.

   It then writes the slot back using the registered save components.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Use includes relative to this file's location (tools/) so it builds both as a tool
    and when included directly into a unit test without relying on additional include dirs. */
#include "../src/core/persistence/save_manager.h"
#include "../src/core/vendor/economy_version.h"
#include "../src/core/vendor/vendor_buyback.h"
#include "../src/core/vendor/vendor_pricing.h"

static int parse_int_arg(const char* a, int* out)
{
    char* end = NULL;
    long v = strtol(a, &end, 10);
    if (!a || a[0] == '\0' || (end && *end != '\0'))
        return -1;
    *out = (int) v;
    return 0;
}

static void usage(const char* argv0)
{
    fprintf(stderr,
            "Usage: %s --slot <n> --curve <ver> --margin <ver>\n"
            "  Loads slot n, sets economy header to (curve,margin), clears vendor dynamic\n"
            "  pricing and buyback buffers, then saves the slot.\n",
            argv0);
}

/* Expose main logic for unit testing. When ECON_MIGRATE_NO_STANDALONE is defined, no main() is
    compiled and econ_migrate_main can be invoked directly. */
int econ_migrate_main(int argc, char** argv)
{
    int slot = 0;
    int have_slot = 0;
    int curve = -1, margin = -1;

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--slot") == 0 && i + 1 < argc)
        {
            if (parse_int_arg(argv[++i], &slot) != 0)
            {
                fprintf(stderr, "Invalid --slot value\n");
                usage(argv[0]);
                return 2;
            }
            have_slot = 1;
        }
        else if (strcmp(argv[i], "--curve") == 0 && i + 1 < argc)
        {
            if (parse_int_arg(argv[++i], &curve) != 0)
            {
                fprintf(stderr, "Invalid --curve value\n");
                usage(argv[0]);
                return 2;
            }
        }
        else if (strcmp(argv[i], "--margin") == 0 && i + 1 < argc)
        {
            if (parse_int_arg(argv[++i], &margin) != 0)
            {
                fprintf(stderr, "Invalid --margin value\n");
                usage(argv[0]);
                return 2;
            }
        }
        else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
        {
            usage(argv[0]);
            return 0;
        }
        else
        {
            fprintf(stderr, "Unknown arg: %s\n", argv[i]);
            usage(argv[0]);
            return 2;
        }
    }

    if (!have_slot || curve < 0 || margin < 0)
    {
        usage(argv[0]);
        return 2;
    }

    /* Init save manager and core components. */
    rogue_save_manager_init();
    rogue_register_core_save_components();

    /* Load the requested slot. */
    if (rogue_save_manager_load_slot(slot) != 0)
    {
        fprintf(stderr, "Failed to load slot %d\n", slot);
        return 3;
    }

    /* Apply migration transforms. */
    rogue_economy_version_set((unsigned) curve, (unsigned) margin);
    rogue_vendor_pricing_reset();
    /* Clear buyback buffers for all vendors. */
    rogue_vendor_buyback_reset();

    /* Save the slot back. */
    if (rogue_save_manager_save_slot(slot) != 0)
    {
        fprintf(stderr, "Failed to save slot %d after migration\n", slot);
        return 4;
    }

    printf("econ_migrate: migrated slot %d to curve=%d margin=%d and cleared dynamic state.\n",
           slot, curve, margin);
    return 0;
}

#ifndef ECON_MIGRATE_NO_STANDALONE
int main(int argc, char** argv) { return econ_migrate_main(argc, argv); }
#endif
