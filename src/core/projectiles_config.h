/* projectiles_config.h - Phase M3.5 externalized projectile & impact tuning */
#ifndef ROGUE_CORE_PROJECTILES_CONFIG_H
#define ROGUE_CORE_PROJECTILES_CONFIG_H
#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct RogueProjectileTuning
    {
        float impact_life_ms;
        int shard_count_hit;
        int shard_count_expire;
        float shard_life_min_ms;
        float shard_life_var_ms;
        float shard_speed_min;
        float shard_speed_var;
        float shard_size_min;
        float shard_size_var;
        float shard_gravity;
    } RogueProjectileTuning;

    int rogue_projectiles_config_load(const char* path);
    int rogue_projectiles_config_load_and_watch(const char* path);
    const RogueProjectileTuning* rogue_projectiles_tuning(void);
    void rogue_projectiles_config_reset(void);
    int rogue_projectiles_spawn_test_shards(int hit);

#ifdef __cplusplus
}
#endif
#endif
