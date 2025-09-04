#include "world_gen.h"
#include "world_gen_dungeon_traps.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void assert_true(int cond, const char* msg)
{
    if (!cond)
    {
        fprintf(stderr, "ASSERT FAILED: %s\n", msg);
        exit(1);
    }
}

int main(void)
{
    /* 7.1: JSON loader */
    const char* js = "{\n"
                     "  \"id\": \"spike_plate\",\n"
                     "  \"trigger\": \"pressure\",\n"
                     "  \"telegraph_ms\": 250,\n"
                     "  \"damage_base\": 30,\n"
                     "  \"cooldown_ms\": 1200,\n"
                     "  \"disarm_diff\": 40\n"
                     "}";
    RogueTrapDef def;
    char err[64];
    int ok = rogue_trap_def_load_json_text(js, &def, err, sizeof err);
    assert_true(ok == 1, "trap json load ok");
    assert_true(strcmp(def.id, "spike_plate") == 0, "id parsed");
    assert_true(def.trigger == ROGUE_TRAP_TRIGGER_PRESSURE_PLATE, "trigger parsed");
    assert_true(def.telegraph_ms == 250 && def.damage_base == 30 && def.cooldown_ms == 1200 &&
                    def.disarm_diff == 40,
                "numeric fields parsed");

    /* 7.2: damage scaling monotonicity vs delta_level and avoidance */
    int d0 = rogue_trap_compute_damage(&def, 0, 0);
    int d1 = rogue_trap_compute_damage(&def, 5, 0);
    int d2 = rogue_trap_compute_damage(&def, 10, 0);
    assert_true(d1 >= d0 && d2 >= d1, "damage increases with delta_level");
    int a0 = rogue_trap_compute_damage(&def, 5, 0);
    int a1 = rogue_trap_compute_damage(&def, 5, 50);
    int a2 = rogue_trap_compute_damage(&def, 5, 100);
    assert_true(a0 >= a1 && a1 >= a2, "damage decreases with avoidance");

    /* 7.3: overlap resolver caps density in 3x3 */
    RogueTileMap map;
    assert_true(rogue_tilemap_init(&map, 12, 12), "tilemap init");
    for (int y = 4; y <= 6; ++y)
        for (int x = 4; x <= 6; ++x)
            rogue_tilemap_set(&map, x, y, ROGUE_TILE_DUNGEON_TRAP);
    int removed = rogue_trap_resolve_overlap(&map, 3 /*max_density*/);
    assert_true(removed >= 1, "overlap resolver removed some traps");
    int count = 0;
    for (int y = 4; y <= 6; ++y)
        for (int x = 4; x <= 6; ++x)
            if (rogue_tilemap_get(&map, x, y) == ROGUE_TILE_DUNGEON_TRAP)
                count++;
    assert_true(count <= 3, "post-resolve density within cap");
    rogue_tilemap_free(&map);

    /* 7.4: disarm probability envelope basic sanity */
    RogueWorldGenConfig cfg = {0};
    cfg.seed = 424242u; /* deterministic */
    RogueWorldGenContext ctx;
    rogue_worldgen_context_init(&ctx, &cfg);
    def.disarm_diff = 60;
    int success_low = 0, tries = 200;
    for (int i = 0; i < tries; ++i)
        success_low += rogue_trap_disarm_success(&ctx, &def, 20);
    /* Low skill should rarely pass (<~40%) */
    assert_true(success_low < (tries * 0.5), "low skill disarm rarely succeeds");

    RogueWorldGenConfig cfg2 = {0};
    cfg2.seed = 424242u; /* same seed, fresh context for fair sampling */
    RogueWorldGenContext ctx2;
    rogue_worldgen_context_init(&ctx2, &cfg2);
    int success_high = 0;
    for (int i = 0; i < tries; ++i)
        success_high += rogue_trap_disarm_success(&ctx2, &def, 95);
    assert_true(success_high > success_low, "high skill disarm succeeds more often");

    rogue_worldgen_context_shutdown(&ctx);
    rogue_worldgen_context_shutdown(&ctx2);
    printf("OK test_dungeon_phase7_traps\n");
    return 0;
}
