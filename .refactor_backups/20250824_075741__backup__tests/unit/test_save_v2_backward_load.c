/* Fabricate a v2 save (32-bit id + 32-bit size headers) and ensure v3 loader still reads sections
 * after migration chain (v2->v3 no-op). */
#include "../../src/core/save_manager.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Buff stub (deduplicated) */
/* Use real buff system */

static int migrate_v2_to_v3(unsigned char* data, size_t sz)
{
    (void) data;
    (void) sz;
    return 0;
}
static RogueSaveMigration MIG = {2u, 3u, migrate_v2_to_v3, "v2_to_v3"};

int main(void)
{
    if (ROGUE_SAVE_FORMAT_VERSION < 3u)
    {
        printf("BACKCOMP_SKIP current_version=%u\n", (unsigned) ROGUE_SAVE_FORMAT_VERSION);
        return 0;
    }
    rogue_save_manager_reset_for_tests();
    rogue_save_manager_init();
    rogue_register_core_save_components();
    /* Write a legitimate v3 save first then rewrite headers to emulate v2 layout */
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("BACKCOMP_FAIL save_v3\n");
        return 1;
    }
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "save_slot_0.sav", "r+b");
#else
    f = fopen("save_slot_0.sav", "r+b");
#endif
    if (!f)
    {
        printf("BACKCOMP_FAIL open\n");
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
        printf("BACKCOMP_FAIL read_hdr\n");
        return 1;
    }
    /* Only proceed if file actually v3 */
    if (hdr.version != ROGUE_SAVE_FORMAT_VERSION)
    {
        fclose(f);
        printf("BACKCOMP_FAIL unexpected_version hdr=%u\n", hdr.version);
        return 1;
    }
    /* Read entire payload after header */
    size_t payload_bytes = (size_t) (hdr.total_size - sizeof hdr);
    unsigned char* payload = (unsigned char*) malloc(payload_bytes);
    if (!payload)
    {
        fclose(f);
        printf("BACKCOMP_FAIL oom\n");
        return 1;
    }
    if (fread(payload, 1, payload_bytes, f) != payload_bytes)
    {
        free(payload);
        fclose(f);
        printf("BACKCOMP_FAIL read_payload\n");
        return 1;
    }
    fclose(f);
    /* Reconstruct a v2 style file: reuse same sections but reinterpret prefixes as 32-bit id+size
     * (expand id16 to id32) */
#if defined(_MSC_VER)
    fopen_s(&f, "save_slot_0_v2.sav", "wb");
#else
    f = fopen("save_slot_0_v2.sav", "wb");
#endif
    if (!f)
    {
        free(payload);
        printf("BACKCOMP_FAIL create_v2\n");
        return 1;
    }
    unsigned v2 = 2u;
    hdr.version = v2; /* We'll reconstruct payload into temp buffer for checksum */
    unsigned char* p = payload;
    size_t written_payload = 0;
    unsigned char* rebuilt = (unsigned char*) malloc(payload_bytes * 2 + 16);
    if (!rebuilt)
    {
        free(payload);
        fclose(f);
        printf("BACKCOMP_FAIL oom2\n");
        return 1;
    }
    size_t rpos = 0;
    unsigned sections = 0;
    while (sections < hdr.section_count)
    {
        if (written_payload >= payload_bytes)
            break;
        if (payload_bytes - written_payload < 6)
            break;
        unsigned id16 = p[0] | (p[1] << 8);
        unsigned size = p[2] | (p[3] << 8) | (p[4] << 16) | (p[5] << 24);
        p += 6;
        written_payload += 6;
        if (payload_bytes - written_payload < size)
            break;
        unsigned id32 = id16;
        memcpy(rebuilt + rpos, &id32, 4);
        memcpy(rebuilt + rpos + 4, &size, 4);
        memcpy(rebuilt + rpos + 8, p, size);
        rpos += 8 + size;
        p += size;
        written_payload += size;
        sections++;
    }
    hdr.section_count = sections;
    hdr.component_mask = hdr.component_mask;
    hdr.total_size = (unsigned long long) (sizeof hdr + rpos); /* temp checksum 0 first */
    hdr.checksum = 0;                                          /* placeholder */
    /* compute crc32 similar to runtime */
    unsigned crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < rpos; i++)
    {
        unsigned char b = rebuilt[i];
        for (int k = 0; k < 8; k++)
        {
            unsigned mask = (crc ^ b) & 1;
            crc >>= 1;
            if (mask)
                crc ^= 0xEDB88320u;
            b >>= 1;
        }
    }
    hdr.checksum = crc ^ 0xFFFFFFFFu;
    if (fwrite(&hdr, sizeof hdr, 1, f) != 1)
    {
        free(payload);
        free(rebuilt);
        fclose(f);
        printf("BACKCOMP_FAIL write_hdr\n");
        return 1;
    }
    if (fwrite(rebuilt, 1, rpos, f) != rpos)
    {
        free(payload);
        free(rebuilt);
        fclose(f);
        printf("BACKCOMP_FAIL write_payload_v2\n");
        return 1;
    }
    fclose(f);
    free(payload);
    free(rebuilt);
    /* Now attempt to load that v2 file by renaming over slot path */
#if defined(_MSC_VER)
    remove("save_slot_0.sav");
    rename("save_slot_0_v2.sav", "save_slot_0.sav");
#else
    rename("save_slot_0_v2.sav", "save_slot_0.sav");
#endif
    /* Register migration and load */
    rogue_save_register_migration(&MIG);
    int rc = rogue_save_manager_load_slot(0);
    if (rc != 0)
    {
        printf("BACKCOMP_FAIL load_rc=%d\n", rc);
        return 1;
    }
    printf("BACKCOMP_OK rc=%d steps=%d failed=%d\n", rc, rogue_save_last_migration_steps(),
           rogue_save_last_migration_failed());
    return 0;
}
