#include "core/loot/loot_item_defs.h"
#include "core/loot/loot_item_defs_sort.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    const char* in = "./temp_items_unsorted.cfg";
    const char* out = "./temp_items_sorted.cfg";
    FILE* f = fopen(in, "wb");
    if (!f)
    {
        fprintf(stderr, "FAIL open unsorted\n");
        return 1;
    }
    fprintf(f, "# Test header\n");
    fprintf(f, "zeta_item,Zeta,0,0,1,1,0,0,0,../assets/sheet.png,4,1,1,1,0\n");
    fprintf(f, "alpha_item,Alpha,0,0,1,1,0,0,0,../assets/sheet.png,1,1,1,1,0\n");
    fprintf(f, "mid_item,Mid,0,0,1,1,0,0,0,../assets/sheet.png,3,1,1,1,0\n");
    fflush(f);
    fclose(f);
    int sorted = rogue_item_defs_sort_cfg(in, out);
    if (sorted != 3)
    {
        fprintf(stderr, "FAIL expected 3 sorted got %d\n", sorted);
        return 2;
    }
    rogue_item_defs_reset();
    int added = rogue_item_defs_load_from_cfg(out);
    if (added != 3)
    {
        fprintf(stderr, "FAIL expected 3 added got %d\n", added);
        return 3;
    }
    int ia = rogue_item_def_index("alpha_item");
    int im = rogue_item_def_index("mid_item");
    int iz = rogue_item_def_index("zeta_item");
    if (!(ia < im && im < iz))
    {
        fprintf(stderr, "FAIL ordering ia=%d im=%d iz=%d\n", ia, im, iz);
        return 4;
    }
    FILE* fs = fopen(out, "rb");
    if (!fs)
    {
        fprintf(stderr, "FAIL reopen sorted\n");
        return 5;
    }
    char first[256];
    fgets(first, sizeof first, fs); /* header */
    fgets(first, sizeof first, fs);
    fclose(fs);
    if (strncmp(first, "alpha_item", 10) != 0)
    {
        fprintf(stderr, "FAIL first data line %s\n", first);
        return 6;
    }
    remove(in);
    remove(out);
    printf("loot_tooling_phase21_5_sort_ok\n");
    return 0;
}
