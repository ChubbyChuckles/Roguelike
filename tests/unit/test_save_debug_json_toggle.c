#include "../../src/core/persistence/save_manager.h"
#include <stdio.h>
#include <string.h>

/* Buff stub (deduplicated) */
/* Use real buff system */

int main(void)
{
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    rogue_save_set_debug_json(1);
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("DEBUG_JSON_FAIL save\n");
        return 1;
    }
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "save_slot_0.json", "rb");
#else
    f = fopen("save_slot_0.json", "rb");
#endif
    if (!f)
    {
        printf("DEBUG_JSON_FAIL missing_json\n");
        return 1;
    }
    char buf[32] = {0};
    size_t n = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    if (n == 0 || !strstr(buf, "\"version\""))
    {
        printf("DEBUG_JSON_FAIL contents\n");
        return 1;
    }
    printf("DEBUG_JSON_OK bytes=%zu\n", n);
    return 0;
}
