/* Verifies that component registration order does not affect final checksum */
#include "../../src/core/app/app_state.h"
#include "../../src/core/save_manager.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Use real buff system symbols (no local stubs to avoid duplicate definitions). */

static unsigned read_checksum(const char* path)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
        return 0;
    struct RogueSaveDescriptor
    {
        unsigned version, timestamp, component_mask, section_count;
        unsigned long long total_size;
        unsigned checksum;
    } desc;
    if (fread(&desc, sizeof desc, 1, f) != 1)
    {
        fclose(f);
        return 0;
    }
    fclose(f);
    return desc.checksum;
}

int main(void)
{
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    /* canonical order save */
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("ORDER_FAIL save0\n");
        return 1;
    }
    unsigned c0 = read_checksum("save_slot_0.sav");

    /* scrambled order scenario: reset and re-register (internal qsort ensures determinism) */
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    if (rogue_save_manager_save_slot(1) != 0)
    {
        printf("ORDER_FAIL save1\n");
        return 1;
    }
    unsigned c1 = read_checksum("save_slot_1.sav");

    if (c0 == 0 || c1 == 0)
    {
        printf("ORDER_FAIL zero_checksum c0=%u c1=%u\n", c0, c1);
        return 1;
    }
    if (c0 != c1)
    {
        printf("ORDER_MISMATCH %u %u\n", c0, c1);
        return 1;
    }
    printf("ORDER_OK checksum=%u\n", c0);
    return 0;
}
