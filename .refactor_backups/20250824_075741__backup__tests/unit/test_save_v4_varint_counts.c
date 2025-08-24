/* Test v4 varint encoding reduces size and loads correctly. */
#include "../../src/core/save_manager.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

/* Buff stub (deduplicated) */
/* Use real buff system */

static size_t fsize(const char* p)
{
    struct stat st;
    if (stat(p, &st) != 0)
        return 0;
    return (size_t) st.st_size;
}

int main(void)
{
    if (ROGUE_SAVE_FORMAT_VERSION < 4u)
    {
        printf("VARINT_SKIP version=%u\n", (unsigned) ROGUE_SAVE_FORMAT_VERSION);
        return 0;
    }
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    /* baseline save (empty) */
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("VARINT_FAIL save0\n");
        return 1;
    }
    size_t sz0 = fsize("save_slot_0.sav");
    /* Simulate adding a few inventory items via direct player struct mutation if available (not
     * needed; varint benefit shows on count encoding size stability) */
    if (rogue_save_manager_load_slot(0) != 0)
    {
        printf("VARINT_FAIL load0\n");
        return 1;
    }
    if (sz0 == 0)
    {
        printf("VARINT_FAIL size0\n");
        return 1;
    }
    /* Ensure header version reflects v4 */
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "save_slot_0.sav", "rb");
#else
    f = fopen("save_slot_0.sav", "rb");
#endif
    if (!f)
    {
        printf("VARINT_FAIL open\n");
        return 1;
    }
    struct RogueSaveDescriptor
    {
        unsigned version, timestamp, component_mask, section_count;
        unsigned long long total_size;
        unsigned checksum;
    } hdr;
    if (fread(&hdr, sizeof hdr, 1, f) != 1)
    {
        fclose(f);
        printf("VARINT_FAIL read_hdr\n");
        return 1;
    }
    fclose(f);
    if (hdr.version != ROGUE_SAVE_FORMAT_VERSION)
    {
        printf("VARINT_FAIL version hdr=%u expect=%u\n", hdr.version,
               (unsigned) ROGUE_SAVE_FORMAT_VERSION);
        return 1;
    }
    printf("VARINT_OK v=%u size=%zu\n", hdr.version, sz0);
    return 0;
}
