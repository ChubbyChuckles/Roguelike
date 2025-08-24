/* Verifies v3 TLV header layout: first section uses 2-byte id + 4-byte size */
#include "../../src/core/save_manager.h"
#include <stdio.h>
#include <string.h>

/* Buff stub (deduplicated) */
/* Use real buff system (no stubs needed). */

int main(void)
{
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    if (ROGUE_SAVE_FORMAT_VERSION < 3u)
    {
        printf("TLV_SKIP version=%u\n", (unsigned) ROGUE_SAVE_FORMAT_VERSION);
        return 0;
    }
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("TLV_FAIL save\n");
        return 1;
    }
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "save_slot_0.sav", "rb");
#else
    f = fopen("save_slot_0.sav", "rb");
#endif
    if (!f)
    {
        printf("TLV_FAIL open\n");
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
        printf("TLV_FAIL read_hdr\n");
        return 1;
    }
    if (hdr.version != ROGUE_SAVE_FORMAT_VERSION)
    {
        fclose(f);
        printf("TLV_FAIL version_mismatch hdr=%u expect=%u\n", hdr.version,
               (unsigned) ROGUE_SAVE_FORMAT_VERSION);
        return 1;
    }
    unsigned char prefix[6];
    if (fread(prefix, 1, 6, f) != 6)
    {
        fclose(f);
        printf("TLV_FAIL read_prefix\n");
        return 1;
    }
    /* Interpret first 2 bytes as id16 and next 4 as size */
    unsigned id16 = prefix[0] | (prefix[1] << 8);
    unsigned size = prefix[2] | (prefix[3] << 8) | (prefix[4] << 16) | (prefix[5] << 24);
    if (id16 == 0 || size == 0)
    {
        fclose(f);
        printf("TLV_FAIL zero_fields id=%u size=%u\n", id16, size);
        return 1;
    }
    fclose(f);
    printf("TLV_OK id=%u size=%u sections=%u\n", id16, size, hdr.section_count);
    return 0;
}
