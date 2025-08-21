#include "core/loot/loot_item_defs.h"
#include <stdio.h>
#include <string.h>

/* Simple test creating an in-memory temp file to validate malformed detection.
   We simulate by writing a small temporary file with good + bad lines. */
int main(void)
{
    const char* path = "temp_items_validation.cfg";
    FILE* f = fopen(path, "wb");
    if (!f)
    {
        fprintf(stderr, "FAIL: could not create temp file\n");
        return 1;
    }
    /* Good line: 14 mandatory + optional rarity */
    fprintf(f, "id_sword,Short Sword,cat,1,20,10,2,4,0,sheet,0,0,16,16,1\n");
    /* Malformed: missing fields (only 5) */
    fprintf(f, "bad_line,Only5,1,2,3\n");
    /* Comment + blank lines ignored */
    fprintf(f, "# comment\n\n");
    /* Another good line */
    fprintf(f, "id_potion,Health Potion,2,1,5,25,0,0,0,sheet,1,0,16,16,0\n");
    fclose(f);

    int lines[8];
    memset(lines, 0, sizeof lines);
    int malformed = rogue_item_defs_validate_file(path, lines, 8);
    if (malformed != 1)
    {
        fprintf(stderr, "FAIL: expected 1 malformed got %d\n", malformed);
        return 2;
    }
    if (lines[0] == 0)
    {
        fprintf(stderr, "FAIL: first malformed line not recorded\n");
        return 3;
    }
    /* Clean up */
    remove(path);
    printf("loot_tooling_phase21_2_validation_ok malformed=%d line=%d\n", malformed, lines[0]);
    return 0;
}
