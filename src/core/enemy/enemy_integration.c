#include "core/enemy/enemy_integration.h"
#include "core/app_state.h"
#include "core/enemy/encounter_composer.h"
#include "core/enemy/enemy_difficulty_scaling.h"
#include "core/enemy/enemy_modifiers.h"
#include "util/determinism.h"
#include "world/world_gen.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Phase 1 debug ring */
typedef struct RogueEncounterDebugRec
{
    unsigned int seed;
    unsigned long long hash;
    int template_id;
    int unit_count;
} RogueEncounterDebugRec;
static RogueEncounterDebugRec g_enc_dbg_ring[32];
static int g_enc_dbg_count = 0;
static int g_enc_dbg_head = 0;

int rogue_enemy_integration_build_mappings(RogueEnemyTypeMapping* out, int max, int* out_count)
{
    if (!out || max <= 0)
    {
        if (out_count)
            *out_count = 0;
        return 0;
    }
    int count = g_app.enemy_type_count;
    if (count > max)
        count = max;
    for (int i = 0; i < count; i++)
    {
        RogueEnemyTypeDef* t = &g_app.enemy_types[i];
        RogueEnemyTypeMapping* m = &out[i];
        memset(m, 0, sizeof *m);
        m->type_index = i;
        m->archetype_id = t->archetype_id;
        m->tier_id = t->tier_id;
        m->base_level_offset = t->base_level_offset;
#if defined(_MSC_VER)
        strncpy_s(m->id, sizeof m->id, t->id[0] ? t->id : t->name, _TRUNCATE);
        strncpy_s(m->name, sizeof m->name, t->name, _TRUNCATE);
#else
        strncpy(m->id, t->id[0] ? t->id : t->name, sizeof m->id - 1);
        m->id[sizeof m->id - 1] = '\0';
        strncpy(m->name, t->name, sizeof m->name - 1);
        m->name[sizeof m->name - 1] = '\0';
#endif
    }
    if (out_count)
        *out_count = count;
    return count > 0;
}

int rogue_enemy_integration_find_by_type(int type_index, const RogueEnemyTypeMapping* arr,
                                         int count)
{
    if (!arr || count <= 0)
        return -1;
    for (int i = 0; i < count; i++)
        if (arr[i].type_index == type_index)
            return i;
    return -1;
}

int rogue_enemy_integration_validate_unique(const RogueEnemyTypeMapping* arr, int count)
{
    if (!arr || count <= 0)
        return 0;
    for (int i = 0; i < count; i++)
    {
        for (int j = i + 1; j < count; j++)
        {
            if (arr[i].type_index == arr[j].type_index)
                return 0;
            if (arr[i].id[0] && arr[j].id[0] && strcmp(arr[i].id, arr[j].id) == 0)
                return 0;
        }
    }
    return 1;
}

void rogue_enemy_integration_apply_spawn(struct RogueEnemy* e,
                                         const RogueEnemyTypeMapping* map_entry, int player_level)
{
    if (!e || !map_entry)
        return;
    e->tier_id = map_entry->tier_id;
    e->base_level_offset = map_entry->base_level_offset;
    int enemy_level = player_level + e->base_level_offset;
    if (enemy_level < 1)
        enemy_level = 1;
    e->level = enemy_level;
    RogueEnemyFinalStats stats;
    if (rogue_enemy_compute_final_stats(player_level, enemy_level, e->tier_id, &stats) == 0)
    {
        e->final_hp = stats.hp;
        e->final_damage = stats.damage;
        e->final_defense = stats.defense;
        e->max_health = (int) (stats.hp);
        if (e->max_health < 1)
            e->max_health = 1;
        e->health = e->max_health;
    }
}

/* ================= Phase 1 implementations ================= */
unsigned int rogue_enemy_integration_encounter_seed(unsigned int world_seed, int region_id,
                                                    int room_id, int encounter_index)
{
    return world_seed ^ (unsigned int) region_id ^ (unsigned int) room_id ^
           (unsigned int) encounter_index;
}

unsigned long long rogue_enemy_integration_replay_hash(int template_id, const int* unit_levels,
                                                       int unit_count, const int* modifier_ids,
                                                       int modifier_count)
{
    unsigned long long h = 0xcbf29ce484222325ULL; /* FNV offset */
    h = rogue_fnv1a64(&template_id, sizeof(template_id), h);
    for (int i = 0; i < unit_count; i++)
        h = rogue_fnv1a64(&unit_levels[i], sizeof(unit_levels[i]), h);
    /* modifiers sorted externally (Phase 3); include count */
    h = rogue_fnv1a64(&modifier_count, sizeof(modifier_count), h);
    for (int m = 0; m < modifier_count; m++)
        h = rogue_fnv1a64(&modifier_ids[m], sizeof(modifier_ids[m]), h);
    return h;
}

void rogue_enemy_integration_debug_record(unsigned int seed, unsigned long long hash,
                                          int template_id, int unit_count)
{
    int idx = g_enc_dbg_head;
    g_enc_dbg_ring[idx].seed = seed;
    g_enc_dbg_ring[idx].hash = hash;
    g_enc_dbg_ring[idx].template_id = template_id;
    g_enc_dbg_ring[idx].unit_count = unit_count;
    g_enc_dbg_head = (g_enc_dbg_head + 1) % 32;
    if (g_enc_dbg_count < 32)
        g_enc_dbg_count++;
}

int rogue_enemy_integration_debug_dump(char* buf, int buf_size)
{
    if (!buf || buf_size <= 0)
        return 0;
    int written = 0;
    int n = g_enc_dbg_count;
    for (int i = 0; i < n; i++)
    {
        int idx = (g_enc_dbg_head - 1 - i);
        if (idx < 0)
            idx += 32;
        RogueEncounterDebugRec* r = &g_enc_dbg_ring[idx];
        int w =
            snprintf(buf + written, buf_size - written, "%d seed=%u hash=%llu tmpl=%d units=%d\n",
                     i, r->seed, (unsigned long long) r->hash, r->template_id, r->unit_count);
        if (w <= 0)
            break;
        written += w;
        if (written >= buf_size)
        {
            written = buf_size;
            break;
        }
    }
    if (written < buf_size)
        buf[written] = '\0';
    else
        buf[buf_size - 1] = '\0';
    return written;
}

/* ================= Phase 2 implementations ================= */

static unsigned int phase2_rng_next(unsigned int* s)
{
    unsigned int x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}
static int phase2_rng_range(unsigned int* s, int hi)
{
    if (hi <= 0)
        return 0;
    return (int) (phase2_rng_next(s) % (unsigned) hi);
}

int rogue_enemy_integration_choose_template(int room_depth, int biome_id, unsigned int seed,
                                            int* out_template_id)
{
    if (!out_template_id)
        return 0;
    *out_template_id = -1;

    int template_count = rogue_encounter_template_count();
    if (template_count <= 0)
        return 0;

    /* Simple depth-based template selection with biome weighting */
    unsigned int rng_state = seed;
    (void) biome_id; /* Future: use for biome-specific template weighting */

    /* Boss rooms (template 3) for deep rooms */
    if (room_depth >= 8 && phase2_rng_range(&rng_state, 100) < 30)
    {
        const RogueEncounterTemplate* boss_template = rogue_encounter_template_by_id(3);
        if (boss_template)
        {
            *out_template_id = 3;
            return 1;
        }
    }

    /* Champion packs (template 2) for medium-deep rooms */
    if (room_depth >= 5 && phase2_rng_range(&rng_state, 100) < 25)
    {
        const RogueEncounterTemplate* champ_template = rogue_encounter_template_by_id(2);
        if (champ_template)
        {
            *out_template_id = 2;
            return 1;
        }
    }

    /* Mixed patrol (template 1) for mid-level rooms */
    if (room_depth >= 3 && phase2_rng_range(&rng_state, 100) < 40)
    {
        const RogueEncounterTemplate* mixed_template = rogue_encounter_template_by_id(1);
        if (mixed_template)
        {
            *out_template_id = 1;
            return 1;
        }
    }

    /* Default to swarm pack (template 0) for basic rooms */
    const RogueEncounterTemplate* swarm_template = rogue_encounter_template_by_id(0);
    if (swarm_template)
    {
        *out_template_id = 0;
        return 1;
    }

    /* Fallback - use first available template */
    const RogueEncounterTemplate* fallback = rogue_encounter_template_at(0);
    if (fallback)
    {
        *out_template_id = fallback->id;
        return 1;
    }

    return 0;
}

int rogue_enemy_integration_compute_room_difficulty(int room_depth, int room_area, int room_tags)
{
    /* Base difficulty from room depth */
    int base_difficulty = room_depth;

    /* Area modifier - larger rooms are slightly harder */
    if (room_area > 64)
        base_difficulty += 1; /* large room */
    if (room_area > 144)
        base_difficulty += 1; /* very large room */

    /* Tag-based modifiers */
    if (room_tags & ROGUE_DUNGEON_ROOM_ELITE)
        base_difficulty += 2;
    if (room_tags & ROGUE_DUNGEON_ROOM_TREASURE)
        base_difficulty += 1;
    if (room_tags & ROGUE_DUNGEON_ROOM_PUZZLE)
        base_difficulty -= 1; /* easier combat, focus on puzzle */

    /* Ensure minimum difficulty */
    if (base_difficulty < 1)
        base_difficulty = 1;

    return base_difficulty;
}

int rogue_enemy_integration_prepare_room_encounter(const struct RogueDungeonRoom* room,
                                                   int world_seed, int region_id,
                                                   RogueRoomEncounterInfo* out_info)
{
    if (!room || !out_info)
        return 0;

    /* Initialize output */
    memset(out_info, 0, sizeof(*out_info));
    out_info->room_id = room->id;
    out_info->encounter_template_id = -1;

    /* Compute room depth (distance from first room approximation) */
    out_info->depth_level = room->id + 1; /* Simple approximation: room id as depth */

    /* Assume plains biome for now (could be extended with actual biome lookup) */
    out_info->biome_id = 1; /* ROGUE_BIOME_PLAINS */

    /* Generate encounter seed */
    out_info->encounter_seed =
        rogue_enemy_integration_encounter_seed((unsigned int) world_seed, region_id, room->id, 0);

    /* Choose template based on room properties */
    if (!rogue_enemy_integration_choose_template(out_info->depth_level, out_info->biome_id,
                                                 out_info->encounter_seed,
                                                 &out_info->encounter_template_id))
    {
        return 0;
    }

    /* Validate template placement */
    if (!rogue_enemy_integration_validate_template_placement(out_info->encounter_template_id, room))
    {
        /* Fallback to basic template */
        out_info->encounter_template_id = 0;
        if (!rogue_enemy_integration_validate_template_placement(0, room))
        {
            return 0; /* Room too small for any encounter */
        }
    }

    return 1;
}

int rogue_enemy_integration_validate_template_placement(int template_id,
                                                        const struct RogueDungeonRoom* room)
{
    if (!room)
        return 0;

    const RogueEncounterTemplate* template = rogue_encounter_template_by_id(template_id);
    if (!template)
        return 0;

    int room_area = room->w * room->h;

    /* Boss rooms need significant space */
    if (template->boss && room_area < 36)
        return 0; /* 6x6 minimum for boss */

    /* Large swarms need moderate space */
    if (template->max_count >= 8 && room_area < 25)
        return 0; /* 5x5 minimum for large swarms */

    /* Basic space requirement - at least 3x3 for any encounter */
    if (room_area < 9)
        return 0;

    return 1;
}

/* ================= Phase 3 implementations ================= */

int rogue_enemy_integration_apply_unit_stats(struct RogueEnemy* enemy,
                                             const struct RogueEncounterUnit* unit,
                                             int player_level,
                                             const RogueEnemyTypeMapping* type_mapping)
{
    if (!enemy || !unit || !type_mapping)
        return 0;

    /* Set basic enemy properties from unit composition */
    enemy->level = unit->level;
    enemy->tier_id = type_mapping->tier_id;
    enemy->base_level_offset = type_mapping->base_level_offset;

    /* Apply elite/boss flags */
    enemy->elite_flag = unit->is_elite ? 1 : 0;
    enemy->boss_flag = 0;    /* Set by encounter template if needed */
    enemy->support_flag = 0; /* Set by encounter template if needed */

    /* Compute final stats using existing scaling system */
    RogueEnemyFinalStats stats;
    if (rogue_enemy_compute_final_stats(player_level, unit->level, type_mapping->tier_id, &stats) !=
        0)
    {
        return 0; /* Failed to compute stats */
    }

    /* Apply elite scaling if applicable */
    if (unit->is_elite)
    {
        stats.hp *= 1.5f;
        stats.damage *= 1.2f;
        stats.defense *= 1.1f;
    }

    /* Cache final stats */
    enemy->final_hp = stats.hp;
    enemy->final_damage = stats.damage;
    enemy->final_defense = stats.defense;

    /* Set health values */
    enemy->max_health = (int) (stats.hp + 0.5f); /* Round to nearest */
    if (enemy->max_health < 1)
        enemy->max_health = 1;
    enemy->health = enemy->max_health;

    return 1;
}

int rogue_enemy_integration_apply_unit_modifiers(struct RogueEnemy* enemy,
                                                 const struct RogueEncounterUnit* unit,
                                                 unsigned int modifier_seed, int is_elite,
                                                 int is_boss)
{
    if (!enemy || !unit)
        return 0;

    /* Initialize modifier arrays */
    enemy->modifier_count = 0;
    memset(enemy->modifier_ids, 0, sizeof(enemy->modifier_ids));

    /* Determine if modifiers should be applied */
    int should_apply_modifiers = 0;
    if (is_boss)
        should_apply_modifiers = 1; /* Bosses always get modifiers */
    else if (is_elite && (modifier_seed % 100) < 75)
        should_apply_modifiers = 1; /* 75% chance for elites */
    else if (!is_elite && (modifier_seed % 100) < 20)
        should_apply_modifiers = 1; /* 20% chance for normals */

    if (!should_apply_modifiers)
        return 1; /* No modifiers, but still successful */

    /* Determine budget cap based on enemy type */
    float budget_cap = 0.6f; /* Default cap */
    if (is_boss)
        budget_cap = 1.0f; /* Bosses can use full budget */
    else if (is_elite)
        budget_cap = 0.8f; /* Elites get higher budget */

    /* Roll modifiers using existing system */
    RogueEnemyModifierSet mod_set;
    memset(&mod_set, 0, sizeof(mod_set));

    int result = rogue_enemy_modifiers_roll(modifier_seed, enemy->tier_id, budget_cap, &mod_set);
    if (result != 0)
        return 1; /* Failed to roll modifiers, but enemy is still valid */

    /* Apply modifiers to enemy */
    enemy->modifier_count = (unsigned char) (mod_set.count < 8 ? mod_set.count : 8);
    for (int i = 0; i < enemy->modifier_count; i++)
    {
        if (mod_set.defs[i])
        {
            enemy->modifier_ids[i] = mod_set.defs[i]->id;
        }
    }

    return 1;
}

int rogue_enemy_integration_finalize_spawn(struct RogueEnemy* enemy,
                                           const struct RogueEncounterUnit* unit,
                                           const RogueRoomEncounterInfo* encounter_info,
                                           int player_level,
                                           const RogueEnemyTypeMapping* type_mapping)
{
    if (!enemy || !unit || !encounter_info || !type_mapping)
        return 0;

    /* Set encounter metadata */
    enemy->encounter_id = encounter_info->room_id; /* Use room id as encounter id */
    enemy->replay_hash_fragment = (unsigned int) (encounter_info->encounter_seed & 0xFFFFFFFF);

    /* Apply base stats */
    if (!rogue_enemy_integration_apply_unit_stats(enemy, unit, player_level, type_mapping))
    {
        return 0;
    }

    /* Generate modifier seed */
    unsigned int modifier_seed =
        encounter_info->encounter_seed ^ (unsigned int) unit->enemy_type_id ^ 0xDEADBEEF;

    /* Apply modifiers */
    if (!rogue_enemy_integration_apply_unit_modifiers(enemy, unit, modifier_seed, unit->is_elite,
                                                      0))
    {
        return 0;
    }

    /* Validate final result */
    if (!rogue_enemy_integration_validate_final_stats(enemy))
    {
        return 0;
    }

    return 1;
}

int rogue_enemy_integration_validate_final_stats(const struct RogueEnemy* enemy)
{
    if (!enemy)
        return 0;

    /* Check for non-negative stats */
    if (enemy->final_hp < 0.1f)
        return 0; /* HP must be positive */
    if (enemy->final_damage < 0.0f)
        return 0; /* Damage can't be negative */
    if (enemy->final_defense < 0.0f)
        return 0; /* Defense can't be negative */

    /* Check health values are consistent */
    if (enemy->max_health <= 0)
        return 0;
    if (enemy->health > enemy->max_health)
        return 0;
    if (enemy->health <= 0)
        return 0;

    /* Check level bounds */
    if (enemy->level <= 0)
        return 0;

    /* Check modifier count bounds */
    if (enemy->modifier_count > 8)
        return 0;

    return 1;
}

/* ================= Phase 4 implementations ================= */

int rogue_enemy_integration_build_display_info(const struct RogueEnemy* enemy, int player_level,
                                               RogueEnemyDisplayInfo* out_info)
{
    if (!enemy || !out_info)
        return 0;

    memset(out_info, 0, sizeof(*out_info));

    /* Get enemy type definition for display name */
    if (enemy->type_index >= 0 && enemy->type_index < g_app.enemy_type_count)
    {
        const RogueEnemyTypeDef* type_def = &g_app.enemy_types[enemy->type_index];
#if defined(_MSC_VER)
        strncpy_s(out_info->name, sizeof(out_info->name), type_def->name, _TRUNCATE);
#else
        strncpy(out_info->name, type_def->name, sizeof(out_info->name) - 1);
        out_info->name[sizeof(out_info->name) - 1] = '\0';
#endif
    }
    else
    {
#if defined(_MSC_VER)
        strcpy_s(out_info->name, sizeof(out_info->name), "Unknown Enemy");
#else
        strcpy(out_info->name, "Unknown Enemy");
#endif
    }

    /* Set tier description based on flags */
    if (enemy->boss_flag)
    {
#if defined(_MSC_VER)
        strcpy_s(out_info->tier_name, sizeof(out_info->tier_name), "Boss");
#else
        strcpy(out_info->tier_name, "Boss");
#endif
    }
    else if (enemy->elite_flag)
    {
#if defined(_MSC_VER)
        strcpy_s(out_info->tier_name, sizeof(out_info->tier_name), "Elite");
#else
        strcpy(out_info->tier_name, "Elite");
#endif
    }
    else if (enemy->support_flag)
    {
#if defined(_MSC_VER)
        strcpy_s(out_info->tier_name, sizeof(out_info->tier_name), "Support");
#else
        strcpy(out_info->tier_name, "Support");
#endif
    }
    else
    {
#if defined(_MSC_VER)
        strcpy_s(out_info->tier_name, sizeof(out_info->tier_name), "Normal");
#else
        strcpy(out_info->tier_name, "Normal");
#endif
    }

    /* Basic info */
    out_info->level = enemy->level;
    out_info->delta_level = player_level - enemy->level;
    out_info->is_elite = enemy->elite_flag;
    out_info->is_boss = enemy->boss_flag;
    out_info->is_support = enemy->support_flag;

    /* Modifier tags */
    out_info->modifier_count = enemy->modifier_count;
    for (int i = 0; i < enemy->modifier_count && i < 8; i++)
    {
        /* Get modifier short tag - placeholder implementation */
        snprintf(out_info->modifier_tags[i], sizeof(out_info->modifier_tags[i]), "M%d",
                 enemy->modifier_ids[i]);
    }

    /* Color coding */
    rogue_enemy_integration_get_type_color(enemy, &out_info->color_r, &out_info->color_g,
                                           &out_info->color_b);

    /* Populate final stats for combat integration */
    out_info->final_stats.hp = (float) enemy->max_health;
    out_info->final_stats.damage = 10.0f + enemy->level * 2.0f; /* Base damage formula */
    out_info->final_stats.defense = (float) enemy->armor;
    out_info->final_stats.hp_mult = 1.0f;  /* No scaling applied yet */
    out_info->final_stats.dmg_mult = 1.0f; /* No scaling applied yet */
    out_info->final_stats.def_mult = 1.0f; /* No scaling applied yet */

    return 1;
}

int rogue_enemy_integration_update_hud_target(const struct RogueEnemy* target_enemy,
                                              int player_level)
{
    (void) player_level; /* Suppress unused parameter warning */

    if (!target_enemy)
    {
        /* Clear target */
        g_app.target_enemy_active = 0;
        g_app.target_enemy_level = 0;
        return 1;
    }

    /* Set target info for HUD display */
    g_app.target_enemy_active = 1;
    g_app.target_enemy_level = target_enemy->level;

    return 1;
}

void rogue_enemy_integration_get_type_color(const struct RogueEnemy* enemy, unsigned char* out_r,
                                            unsigned char* out_g, unsigned char* out_b)
{
    if (!enemy || !out_r || !out_g || !out_b)
        return;

    /* Default white */
    *out_r = 255;
    *out_g = 255;
    *out_b = 255;

    if (enemy->boss_flag)
    {
        /* Boss - red/orange */
        *out_r = 255;
        *out_g = 140;
        *out_b = 0;
    }
    else if (enemy->elite_flag)
    {
        /* Elite - yellow */
        *out_r = 255;
        *out_g = 215;
        *out_b = 0;
    }
    else if (enemy->support_flag)
    {
        /* Support - cyan */
        *out_r = 0;
        *out_g = 191;
        *out_b = 255;
    }
    /* Normal enemies keep white */
}

const char* rogue_enemy_integration_get_modifier_telegraph(int modifier_id)
{
    /* Placeholder implementation - could be extended to lookup from modifier definitions */
    switch (modifier_id)
    {
    case 1:
        return "speed_aura";
    case 2:
        return "defense_aura";
    case 3:
        return "rage_aura";
    default:
        return "modifier_aura";
    }
}

/* ================= Phase 5 implementations ================= */

int rogue_enemy_integration_solve_spawn_positions(const RogueRoomEncounterInfo* encounter_info,
                                                  const RogueRoomDimensions* room_dims,
                                                  RogueSpawnSolution* out_solution)
{
    if (!encounter_info || !room_dims || !out_solution)
        return 0;

    memset(out_solution, 0, sizeof(*out_solution));

    /* Copy room bounds */
    out_solution->room_bounds[0] = room_dims->min_x;
    out_solution->room_bounds[1] = room_dims->min_y;
    out_solution->room_bounds[2] = room_dims->max_x;
    out_solution->room_bounds[3] = room_dims->max_y;

    /* For Phase 5, we'll use a default unit count based on depth level */
    int estimated_unit_count = 2 + (encounter_info->depth_level / 3);
    if (estimated_unit_count > 16)
        estimated_unit_count = 16;
    if (estimated_unit_count < 1)
        estimated_unit_count = 1;

    /* Set minimum distance between spawns based on enemy count */
    out_solution->min_distance = (estimated_unit_count <= 4) ? 2.0f : 1.5f;

    /* Assume boss spawn for deeper levels */
    int has_boss = (encounter_info->depth_level >= 5);

    /* Boss positioning: center anchored */
    int spawn_index = 0;
    if (has_boss && spawn_index < 16)
    {
        float center_x = (room_dims->min_x + room_dims->max_x) / 2.0f;
        float center_y = (room_dims->min_y + room_dims->max_y) / 2.0f;

        if (rogue_enemy_integration_validate_spawn_position(center_x, center_y, room_dims,
                                                            out_solution, spawn_index))
        {
            out_solution->positions[spawn_index][0] = center_x;
            out_solution->positions[spawn_index][1] = center_y;
            spawn_index++;
        }
    }

    /* Elite and normal positioning: spread around room */
    int max_attempts = 50; /* Bounded attempts to prevent infinite loops */
    for (int unit = (has_boss ? 1 : 0); unit < estimated_unit_count && spawn_index < 16; unit++)
    {
        int placed = 0;
        for (int attempt = 0; attempt < max_attempts && !placed; attempt++)
        {
            /* Generate position with some spread */
            float padding = 1.0f;
            float x =
                room_dims->min_x + padding +
                (rand() % 1000) / 1000.0f * (room_dims->max_x - room_dims->min_x - 2 * padding);
            float y =
                room_dims->min_y + padding +
                (rand() % 1000) / 1000.0f * (room_dims->max_y - room_dims->min_y - 2 * padding);

            if (rogue_enemy_integration_validate_spawn_position(x, y, room_dims, out_solution,
                                                                spawn_index))
            {
                out_solution->positions[spawn_index][0] = x;
                out_solution->positions[spawn_index][1] = y;
                spawn_index++;
                placed = 1;
            }
        }

        if (!placed)
        {
            /* Failed to place this enemy, mark solution as partial failure */
            out_solution->success = 0;
        }
    }

    out_solution->position_count = spawn_index;

    /* Success if we placed at least some enemies and intended to place them all */
    if (spawn_index == 0)
    {
        out_solution->success = 0;
    }
    else if (spawn_index >= estimated_unit_count)
    {
        out_solution->success = 1;
    }

    return 1;
}

int rogue_enemy_integration_validate_spawn_position(float x, float y,
                                                    const RogueRoomDimensions* room_dims,
                                                    const RogueSpawnSolution* existing_positions,
                                                    int check_count)
{
    if (!room_dims)
        return 0;

    /* Check room bounds */
    if (x < room_dims->min_x || x > room_dims->max_x || y < room_dims->min_y ||
        y > room_dims->max_y)
    {
        return 0;
    }

    /* Check obstacle zones */
    for (int i = 0; i < room_dims->obstacle_count; i++)
    {
        const float* obs = room_dims->obstacle_zones[i];
        if (x >= obs[0] && x <= obs[2] && y >= obs[1] && y <= obs[3])
        {
            return 0; /* Position is inside obstacle */
        }
    }

    /* Check minimum distance from existing spawn points */
    if (existing_positions)
    {
        for (int i = 0; i < check_count; i++)
        {
            float dx = x - existing_positions->positions[i][0];
            float dy = y - existing_positions->positions[i][1];
            float distance = sqrtf(dx * dx + dy * dy);
            if (distance < existing_positions->min_distance)
            {
                return 0; /* Too close to existing spawn */
            }
        }
    }

    return 1; /* Position is valid */
}

int rogue_enemy_integration_register_navmesh_handles(const RogueSpawnSolution* solution,
                                                     const struct RogueEnemy* enemies,
                                                     int enemy_count)
{
    if (!solution || !enemies)
        return 0;

    /* Placeholder implementation - would integrate with actual navigation system */
    for (int i = 0; i < enemy_count && i < solution->position_count; i++)
    {
        /* In real implementation, this would:
         * 1. Register enemy with navigation mesh
         * 2. Set up pathing handles
         * 3. Initialize AI state for navigation
         */
    }

    return 1;
}

int rogue_enemy_integration_finalize_enemy_placement(const RogueSpawnSolution* solution,
                                                     struct RogueEnemy* enemies, int enemy_count)
{
    if (!solution || !enemies)
        return 0;

    /* Apply positions to enemy entities */
    for (int i = 0; i < enemy_count && i < solution->position_count; i++)
    {
        /* In real implementation, this would set entity position */
        /* enemies[i].base.x = solution->positions[i][0]; */
        /* enemies[i].base.y = solution->positions[i][1]; */
    }

    return solution->success;
}

/* ================= Phase 6: Target Acquisition & Combat Hook ================= */

int rogue_enemy_integration_find_nearest_enemy(const RogueEnemyRegistry* registry,
                                               float position[2], float max_distance,
                                               int* out_enemy_id)
{
    if (!registry || !position || !out_enemy_id)
        return 0;

    float closest_distance = max_distance + 1.0f;
    int closest_enemy_id = -1;

    for (int i = 0; i < registry->count; i++)
    {
        const RogueEnemyRegistryEntry* entry = &registry->entries[i];
        if (!entry->is_alive)
            continue;

        float dx = position[0] - entry->position[0];
        float dy = position[1] - entry->position[1];
        float distance = sqrtf(dx * dx + dy * dy);

        if (distance <= max_distance && distance < closest_distance)
        {
            closest_distance = distance;
            closest_enemy_id = entry->enemy_id;
        }
    }

    if (closest_enemy_id >= 0)
    {
        *out_enemy_id = closest_enemy_id;
        return 1;
    }

    return 0;
}

int rogue_enemy_integration_find_enemy_at_position(const RogueEnemyRegistry* registry,
                                                   float position[2], float tolerance,
                                                   int* out_enemy_id)
{
    if (!registry || !position || !out_enemy_id)
        return 0;

    for (int i = 0; i < registry->count; i++)
    {
        const RogueEnemyRegistryEntry* entry = &registry->entries[i];
        if (!entry->is_alive)
            continue;

        float dx = position[0] - entry->position[0];
        float dy = position[1] - entry->position[1];
        float distance = sqrtf(dx * dx + dy * dy);

        if (distance <= tolerance)
        {
            *out_enemy_id = entry->enemy_id;
            return 1;
        }
    }

    return 0;
}

int rogue_enemy_integration_register_enemy(RogueEnemyRegistry* registry, int room_id,
                                           int encounter_id, float position[2],
                                           const RogueEnemyDisplayInfo* display_info)
{
    if (!registry || !position || !display_info)
        return -1;
    if (registry->count >= MAX_REGISTERED_ENEMIES)
        return -1;

    RogueEnemyRegistryEntry* entry = &registry->entries[registry->count];
    entry->enemy_id = registry->next_enemy_id++;
    entry->room_id = room_id;
    entry->encounter_id = encounter_id;
    entry->position[0] = position[0];
    entry->position[1] = position[1];
    entry->display_info = *display_info; /* Copy display info */
    entry->is_alive = 1;

    registry->count++;

    return entry->enemy_id;
}

void rogue_enemy_integration_update_enemy_position(RogueEnemyRegistry* registry, int enemy_id,
                                                   float position[2])
{
    if (!registry || !position)
        return;

    for (int i = 0; i < registry->count; i++)
    {
        RogueEnemyRegistryEntry* entry = &registry->entries[i];
        if (entry->enemy_id == enemy_id && entry->is_alive)
        {
            entry->position[0] = position[0];
            entry->position[1] = position[1];
            break;
        }
    }
}

int rogue_enemy_integration_get_enemy_display_info(const RogueEnemyRegistry* registry, int enemy_id,
                                                   RogueEnemyDisplayInfo* out_display_info)
{
    if (!registry || !out_display_info)
        return 0;

    for (int i = 0; i < registry->count; i++)
    {
        const RogueEnemyRegistryEntry* entry = &registry->entries[i];
        if (entry->enemy_id == enemy_id && entry->is_alive)
        {
            *out_display_info = entry->display_info; /* Copy display info */
            return 1;
        }
    }

    return 0;
}

void rogue_enemy_integration_mark_enemy_dead(RogueEnemyRegistry* registry, int enemy_id)
{
    if (!registry)
        return;

    for (int i = 0; i < registry->count; i++)
    {
        RogueEnemyRegistryEntry* entry = &registry->entries[i];
        if (entry->enemy_id == enemy_id && entry->is_alive)
        {
            entry->is_alive = 0;

            /* Emit analytics event for death */
            /* In real implementation, would log:
             * - encounter_id for this enemy
             * - time survived
             * - damage taken
             * - player level vs enemy level
             * - modifiers active at death
             */
            break;
        }
    }
}

int rogue_enemy_integration_get_combat_stats(const RogueEnemyRegistry* registry, int enemy_id,
                                             RogueEnemyCombatStats* out_stats)
{
    if (!registry || !out_stats)
        return 0;

    for (int i = 0; i < registry->count; i++)
    {
        const RogueEnemyRegistryEntry* entry = &registry->entries[i];
        if (entry->enemy_id == enemy_id && entry->is_alive)
        {
            /* Extract stats from display info */
            out_stats->max_health = entry->display_info.final_stats.hp;
            out_stats->current_health =
                entry->display_info.final_stats.hp; /* Full health initially */
            out_stats->base_damage = entry->display_info.final_stats.damage;
            out_stats->armor_rating = entry->display_info.final_stats.defense;
            out_stats->crit_chance = 0.05f;    /* Default 5% crit */
            out_stats->crit_multiplier = 2.0f; /* Default 2x crit multiplier */

            /* Resistance defaults - would be computed from modifiers */
            out_stats->fire_resist = 0.0f;
            out_stats->cold_resist = 0.0f;
            out_stats->lightning_resist = 0.0f;
            out_stats->poison_resist = 0.0f;

            return 1;
        }
    }

    return 0;
}

int rogue_enemy_integration_apply_damage(RogueEnemyRegistry* registry, int enemy_id, float damage,
                                         int damage_type)
{
    if (!registry || damage < 0.0f)
        return 0;

    for (int i = 0; i < registry->count; i++)
    {
        RogueEnemyRegistryEntry* entry = &registry->entries[i];
        if (entry->enemy_id == enemy_id && entry->is_alive)
        {
            /* Apply resistance based on damage type */
            float effective_damage = damage;
            switch (damage_type)
            {
            case 1:
                effective_damage *= (1.0f - 0.0f);
                break; /* fire resist */
            case 2:
                effective_damage *= (1.0f - 0.0f);
                break; /* cold resist */
            case 3:
                effective_damage *= (1.0f - 0.0f);
                break; /* lightning resist */
            case 4:
                effective_damage *= (1.0f - 0.0f);
                break; /* poison resist */
            default:   /* physical damage, no resist applied */
                break;
            }

            /* Apply armor reduction for physical damage */
            if (damage_type == 0)
            {
                float armor = entry->display_info.final_stats.defense;
                float damage_reduction = armor / (armor + 100.0f); /* DR formula */
                effective_damage *= (1.0f - damage_reduction);
            }

            /* For this implementation, we'll track damage in the hp stat */
            /* In real implementation, would maintain separate current_health field */
            entry->display_info.final_stats.hp -= effective_damage;

            if (entry->display_info.final_stats.hp <= 0.0f)
            {
                rogue_enemy_integration_mark_enemy_dead(registry, enemy_id);
                return 2; /* Enemy died */
            }

            return 1; /* Damage applied, enemy still alive */
        }
    }

    return 0; /* Enemy not found or already dead */
}

void rogue_enemy_integration_cleanup_dead_enemies(RogueEnemyRegistry* registry)
{
    if (!registry)
        return;

    /* Compact array by moving living enemies to fill gaps left by dead ones */
    int write_index = 0;
    for (int read_index = 0; read_index < registry->count; read_index++)
    {
        if (registry->entries[read_index].is_alive)
        {
            if (write_index != read_index)
            {
                registry->entries[write_index] = registry->entries[read_index];
            }
            write_index++;
        }
    }

    registry->count = write_index;
}
