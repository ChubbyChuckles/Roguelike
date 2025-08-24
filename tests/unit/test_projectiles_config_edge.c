/* Phase M4.1 additional tests: projectile config edge cases */
#include "../../src/core/projectiles_config.h"
#include "../../src/util/hot_reload.h"
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
    /* Ensure assets dir exists */
#ifdef _WIN32
    _mkdir("assets");
#else
    mkdir("assets", 0777);
#endif
    /* Reset to known defaults and snapshot */
    rogue_projectiles_config_reset();
    const RogueProjectileTuning* def = rogue_projectiles_tuning();
    RogueProjectileTuning baseline = *def;

    /* Loading a non-existent file should fail and not mutate state */
    assert(rogue_projectiles_config_load("assets/does_not_exist.cfg") == -1);
    const RogueProjectileTuning* t = rogue_projectiles_tuning();
    assert(memcmp(&baseline, t, sizeof baseline) == 0);

    /* Partial file: only override a subset of keys; unspecified retain previous values */
    write_temp("assets/projectiles_partial.cfg",
               "IMPACT_LIFE_MS=999\nSHARD_COUNT_HIT=42\n# missing others on purpose\n");
    assert(rogue_projectiles_config_load("assets/projectiles_partial.cfg") == 0);
    t = rogue_projectiles_tuning();
    assert(t->impact_life_ms == 999.0f);
    assert(t->shard_count_hit == 42);
    /* Unspecified keys unchanged */
    assert(t->shard_count_expire == baseline.shard_count_expire);
    assert(t->shard_life_min_ms == baseline.shard_life_min_ms);
    assert(t->shard_speed_min == baseline.shard_speed_min);

    /* Now perform hot reload watch on full file to ensure previously missing fields can change */
    write_temp("assets/projectiles_full_edge.cfg",
               "IMPACT_LIFE_MS=123\nSHARD_COUNT_HIT=3\nSHARD_COUNT_EXPIRE=2\nSHARD_LIFE_MIN_MS="
               "111\nSHARD_LIFE_VAR_MS=11\nSHARD_SPEED_MIN=1.1\nSHARD_SPEED_VAR=2.2\nSHARD_SIZE_"
               "MIN=3.3\nSHARD_SIZE_VAR=4.4\nGRAVITY=0.55\n");
    assert(rogue_projectiles_config_load_and_watch("assets/projectiles_full_edge.cfg") == 0);
    rogue_hot_reload_force("projectiles_cfg");
    t = rogue_projectiles_tuning();
    assert(t->impact_life_ms == 123.0f && t->shard_count_hit == 3 && t->shard_count_expire == 2);
    assert(t->shard_life_min_ms == 111.0f && t->shard_life_var_ms == 11.0f);
    assert(t->shard_speed_min == 1.1f && t->shard_speed_var == 2.2f);
    assert(t->shard_size_min == 3.3f && t->shard_size_var == 4.4f);
    assert(t->shard_gravity == 0.55f);

    return 0;
}
