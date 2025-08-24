#include "../../src/core/save_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal component for scheduling */
static int dummy_val = 7;
static int write_dummy(FILE* f)
{
    fwrite(&dummy_val, sizeof dummy_val, 1, f);
    return 0;
}
static int read_dummy(FILE* f, size_t sz)
{
    if (sz < sizeof(int))
        return -1;
    fread(&dummy_val, sizeof dummy_val, 1, f);
    return 0;
}
/* Use real buff system (no local stub). */

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    RogueSaveComponent C = {1, write_dummy, read_dummy, "C"};
    rogue_save_manager_register(&C);
    rogue_save_set_autosave_interval_ms(1000); /* 1s */
    /* Simulate 5 seconds of idle time */
    for (int t = 0; t <= 5000; t += 250)
    {
        rogue_save_manager_update((uint32_t) t, 0);
    }
    if (rogue_save_autosave_count() < 4)
    {
        printf("AUTO_FAIL count=%u\n", (unsigned) rogue_save_autosave_count());
        return 1;
    }
    printf("AUTO_OK count=%u last_bytes=%u ms=%.2f\n", (unsigned) rogue_save_autosave_count(),
           (unsigned) rogue_save_last_save_bytes(), rogue_save_last_save_ms());
    return 0;
}
