#include "core/projectiles_config.h"
#include "util/hot_reload.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

static void write_temp(const char* path, const char* contents)
{
    FILE* f = fopen(path, "wb");
    assert(f);
    fputs(contents, f);
    fclose(f);
}

int main(void)
{
    /* Ensure baseline file exists relative to working directory (build dir) */
    /* Create assets directory if missing */
#ifdef _WIN32
    _mkdir("assets");
#else
    mkdir("assets", 0777);
#endif
    write_temp("assets/projectiles.cfg",
               "IMPACT_LIFE_MS=260\nSHARD_COUNT_HIT=10\nSHARD_COUNT_EXPIRE=6\nSHARD_LIFE_MIN_MS="
               "340\nSHARD_LIFE_VAR_MS=120\nSHARD_SPEED_MIN=2.5\nSHARD_SPEED_VAR=3.5\nSHARD_SIZE_"
               "MIN=4.0\nSHARD_SIZE_VAR=3.0\nGRAVITY=0.2\n");
    assert(rogue_projectiles_config_load_and_watch("assets/projectiles.cfg") == 0);
    const RogueProjectileTuning* t = rogue_projectiles_tuning();
    assert(t->impact_life_ms == 260.0f);
    /* Modify file to trigger reload */
    write_temp("assets/projectiles.cfg",
               "IMPACT_LIFE_MS=300\nSHARD_COUNT_HIT=8\nSHARD_COUNT_EXPIRE=5\nSHARD_LIFE_MIN_MS="
               "400\nSHARD_LIFE_VAR_MS=100\nSHARD_SPEED_MIN=3.0\nSHARD_SPEED_VAR=2.0\nSHARD_SIZE_"
               "MIN=5.0\nSHARD_SIZE_VAR=1.0\nGRAVITY=0.25\n");
    /* Force hot reload tick */
    rogue_hot_reload_force("projectiles_cfg");
    t = rogue_projectiles_tuning();
    assert(t->impact_life_ms == 300.0f);
    assert(t->shard_count_hit == 8);
    assert(t->shard_gravity == 0.25f);
    /* Restore original */
    write_temp("assets/projectiles.cfg",
               "IMPACT_LIFE_MS=260\nSHARD_COUNT_HIT=10\nSHARD_COUNT_EXPIRE=6\nSHARD_LIFE_MIN_MS="
               "340\nSHARD_LIFE_VAR_MS=120\nSHARD_SPEED_MIN=2.5\nSHARD_SPEED_VAR=3.5\nSHARD_SIZE_"
               "MIN=4.0\nSHARD_SIZE_VAR=3.0\nGRAVITY=0.2\n");
    rogue_hot_reload_force("projectiles_cfg");
    t = rogue_projectiles_tuning();
    assert(t->impact_life_ms == 260.0f);
    return 0;
}
