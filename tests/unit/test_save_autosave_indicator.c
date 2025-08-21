#include "core/save_manager.h"
#include <stdio.h>
#include <string.h>

/* Use real buff system API; provide minimal forward decls instead of duplicate definitions to avoid
 * LNK2005. */
typedef struct RogueBuff RogueBuff; /* opaque for this test */
int rogue_buffs_apply(int type, int magnitude, double duration_ms, double now_ms);
static int write_comp(FILE* f)
{
    int v = 1;
    fwrite(&v, sizeof v, 1, f);
    return 0;
}
static int read_comp(FILE* f, size_t s)
{
    (void) s;
    int v;
    fread(&v, sizeof v, 1, f);
    return 0;
}

int main(void)
{
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    RogueSaveComponent C = {1, write_comp, read_comp, "C"};
    rogue_save_manager_register(&C);
    rogue_save_set_autosave_interval_ms(500); /* 0.5s */
    rogue_save_set_autosave_throttle_ms(400);
    char buf[256];
    for (int t = 0; t < 3000; t += 200)
    {
        rogue_save_manager_update((uint32_t) t, 0);
        rogue_save_status_string(buf, sizeof buf);
        printf("STAT %d %s\n", t, buf);
    }
    if (rogue_save_autosave_count() == 0)
    {
        printf("INDICATOR_FAIL no autosaves\n");
        return 1;
    }
    printf("INDICATOR_OK count=%u\n", rogue_save_autosave_count());
    return 0;
}
