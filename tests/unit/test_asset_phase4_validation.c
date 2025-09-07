/* test_asset_phase4_validation.c - Phase 4 asset validation & integrity tests */
#include "asset/asset_manager.h"
#include "asset/asset_validation.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_fallback_and_acquire(void)
{
    assert(rogue_asset_manager_init(NULL));
    /* Use an existing file as fallback (JSON file, loader is stub-safe) */
    const char* fallback = "assets/enemies.json";
    rogue_asset_set_fallback_texture(fallback);
    int idx = rogue_asset_manager_acquire_texture("does/not/exist/missing_texture.png");
    assert(idx >= 0);
    const RogueAssetTexture* tex = rogue_asset_manager_get(idx);
    assert(tex);
    assert(strcmp(tex->path, fallback) == 0);
    rogue_asset_manager_release_texture(idx);
    rogue_asset_manager_shutdown();
}

static void test_crc_and_checksum_registry(void)
{
    bool ok = false;
    uint32_t crc = rogue_asset_crc32_file("assets/enemies.json", &ok);
    assert(ok && crc != 0);
    assert(rogue_asset_checksum_register("assets/enemies.json", crc));
    assert(rogue_asset_checksum_verify_all());
    assert(!rogue_asset_checksum_verify_one("assets/enemies.json", crc + 1));
}

static void test_dependency_tracking(void)
{
    rogue_asset_dep_add("player_sheet", "player_idle_texture");
    rogue_asset_dep_add("player_sheet", "player_walk_texture");
    const char* deps[4];
    size_t count = rogue_asset_dep_get("player_sheet", deps, 4);
    assert(count == 2);
}

static void test_usage_stats(void)
{
    rogue_asset_manager_init(NULL);
    rogue_asset_set_fallback_texture("assets/enemies.json");
    int a = rogue_asset_manager_acquire_texture("does/not/exist/a.png");
    int b = rogue_asset_manager_acquire_texture("does/not/exist/b.png");
    assert(a >= 0 && b >= 0);
    RogueAssetUsageStats stats = rogue_asset_usage_stats();
    assert(stats.texture_records >= 2);
    assert(stats.textures_failed == 0);
    rogue_asset_manager_shutdown();
}

int main(void)
{
    test_fallback_and_acquire();
    test_crc_and_checksum_registry();
    test_dependency_tracking();
    test_usage_stats();
    rogue_asset_validation_shutdown();
    printf("test_asset_phase4_validation OK\n");
    return 0;
}
