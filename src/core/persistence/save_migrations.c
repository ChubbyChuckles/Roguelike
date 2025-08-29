#include "save_internal.h"

static int migrate_v2_to_v3(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static int migrate_v3_to_v4(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static int migrate_v4_to_v5(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static int migrate_v5_to_v6(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static int migrate_v6_to_v7(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static int migrate_v7_to_v8(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}
static int migrate_v8_to_v9(unsigned char* data, size_t size)
{
    (void) data;
    (void) size;
    return 0;
}

static RogueSaveMigration MIG_V2_TO_V3 = {2u, 3u, migrate_v2_to_v3, "v2_to_v3_tlv_header"};
static RogueSaveMigration MIG_V3_TO_V4 = {3u, 4u, migrate_v3_to_v4, "v3_to_v4_varint_counts"};
static RogueSaveMigration MIG_V4_TO_V5 = {4u, 5u, migrate_v4_to_v5, "v4_to_v5_string_intern"};
static RogueSaveMigration MIG_V5_TO_V6 = {5u, 6u, migrate_v5_to_v6, "v5_to_v6_section_compress"};
static RogueSaveMigration MIG_V6_TO_V7 = {6u, 7u, migrate_v6_to_v7, "v6_to_v7_integrity"};
static RogueSaveMigration MIG_V7_TO_V8 = {7u, 8u, migrate_v7_to_v8, "v7_to_v8_replay_hash"};
static RogueSaveMigration MIG_V8_TO_V9 = {8u, 9u, migrate_v8_to_v9, "v8_to_v9_signature_opt"};

void rogue_register_core_migrations_internal(void)
{
    rogue_save_register_migration(&MIG_V2_TO_V3);
    rogue_save_register_migration(&MIG_V3_TO_V4);
    rogue_save_register_migration(&MIG_V4_TO_V5);
    rogue_save_register_migration(&MIG_V5_TO_V6);
    rogue_save_register_migration(&MIG_V6_TO_V7);
    rogue_save_register_migration(&MIG_V7_TO_V8);
    rogue_save_register_migration(&MIG_V8_TO_V9);
}
