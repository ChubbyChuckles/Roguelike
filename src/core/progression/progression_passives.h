/* Character Progression Phase 5: Skill & Passive Unlock Integration
 * Implements roadmap items 5.1–5.6
 * 5.1 Unified effect DSL (active/passive stat deltas) -- minimal additive stat DSL
 * 5.2 Runtime compile to dispatch tables (parsed into per-node effect arrays)
 * 5.3 Unlock transaction journal (node_id,timestamp) with rolling FNV-1a hash chain
 * 5.4 Precomputed passive snapshot & incremental diff application
 * 5.5 Hot reload (dev) with migration: replays journal on new DSL build
 * 5.6 Tests: parsing, snapshot correctness, duplicate unlock guard, hash change
 */
#ifndef ROGUE_PROGRESSION_PASSIVES_H
#define ROGUE_PROGRESSION_PASSIVES_H
#ifdef __cplusplus
extern "C"
{
#endif

    struct RogueProgressionMaze; /* fwd */

    int rogue_progression_passives_init(const struct RogueProgressionMaze* maze);
    void rogue_progression_passives_shutdown(void);

    int rogue_progression_passives_load_dsl(const char* text);

    int rogue_progression_passive_unlock(int node_id, unsigned int timestamp_ms, int level, int str,
                                         int dex, int intel, int vit);

    int rogue_progression_passives_stat_total(int stat_id);
    int rogue_progression_passives_is_unlocked(int node_id);
    unsigned long long rogue_progression_passives_journal_hash(void);

    int rogue_progression_passives_reload(const struct RogueProgressionMaze* maze, const char* text,
                                          int level, int str, int dex, int intel, int vit);

    /* Phase 7.3 anti-stack: current keystone counts per category (offense, defense, utility) */
    int rogue_progression_passives_keystone_count_offense(void);
    int rogue_progression_passives_keystone_count_defense(void);
    int rogue_progression_passives_keystone_count_utility(void);

#ifdef __cplusplus
}
#endif
#endif
