/* Validates that a legacy v1 save (header downgrade) migrates to v2 via registered chain */
#include "../../src/core/app/app_state.h"
#include "../../src/core/persistence/save_manager.h"
#include <stdio.h>
#include <string.h>

/* Use real buff system (removed local stubs to avoid duplicate symbols). */

static int migrate_v1_to_v2(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static RogueSaveMigration MIG1 = {1u, 2u, migrate_v1_to_v2, "v1_to_v2"};

int main(void)
{
    rogue_save_manager_reset_for_tests();
    /* Register legacy v1->v2 migration before init so internal chain includes it */
    rogue_save_register_migration(&MIG1);
    rogue_save_manager_init();
    rogue_register_core_save_components();
    if (rogue_save_manager_save_slot(0) != 0)
    {
        printf("MIGRATION_FAIL initial_save\n");
        return 1;
    }
    /* Downgrade header version to 1 and recompute descriptor checksum so tamper check passes */
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, "save_slot_0.sav", "r+b");
#else
    f = fopen("save_slot_0.sav", "r+b");
#endif
    if (!f)
    {
        printf("MIGRATION_FAIL open\n");
        return 1;
    }
    /* Read full descriptor + rest of file */
    RogueSaveDescriptor desc;
    if (fread(&desc, sizeof desc, 1, f) != 1)
    {
        fclose(f);
        printf("MIGRATION_FAIL read_desc\n");
        return 1;
    }
    long file_end = 0;
    fseek(f, 0, SEEK_END);
    file_end = ftell(f);
    size_t rest = (size_t) (file_end - (long) sizeof desc);
    unsigned char* buf = (unsigned char*) malloc(rest);
    if (!buf)
    {
        fclose(f);
        printf("MIGRATION_FAIL alloc\n");
        return 1;
    }
    fseek(f, sizeof desc, SEEK_SET);
    if (fread(buf, 1, rest, f) != rest)
    {
        free(buf);
        fclose(f);
        printf("MIGRATION_FAIL read_payload\n");
        return 1;
    }
    /* Downgrade version and recompute checksum over payload (pre-footer hashable region heuristic:
     * if v>=7, exclude SHA/footer) */
    desc.version = 1u;        /* legacy */
    size_t crc_region = rest; /* for version <7 entire payload is hashed */
    uint32_t crc = rogue_crc32(buf, crc_region);
    desc.checksum = crc;
    /* Rewrite file */
    fseek(f, 0, SEEK_SET);
    fwrite(&desc, sizeof desc, 1, f);
    fwrite(buf, 1, rest, f);
    fflush(f);
    free(buf);
    fclose(f);
    /* Attempt load which should invoke v1->v2 migration */
    int rc = rogue_save_manager_load_slot(0);
    if (rc != 0)
    {
        printf("MIGRATION_FAIL rc=%d\n", rc);
        return 1;
    }
    printf("MIGRATION_OK v1_to_v2 rc=%d\n", rc);
    return 0;
}
