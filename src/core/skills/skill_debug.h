#ifndef ROGUE_CORE_SKILL_DEBUG_H
#define ROGUE_CORE_SKILL_DEBUG_H

#include "skills.h"
#include "skills_coeffs.h"

#ifdef __cplusplus
extern "C"
{
#endif

    /* Lightweight debug/inspection APIs for skills; safe in headless unit tests. */

    /* Return number of registered skills. */
    int rogue_skill_debug_count(void);

    /* Get display name by id; returns non-NULL string or "<noname>". */
    const char* rogue_skill_debug_name(int id);

    /* Get read-write coeff params for a skill; returns 0 on success. */
    int rogue_skill_debug_get_coeff(int id, RogueSkillCoeffParams* out);

    /* Overwrite coeff params for a skill (live update). Returns 0 on success. */
    int rogue_skill_debug_set_coeff(int id, const RogueSkillCoeffParams* in);

    /* Get/edit core timing properties on the definition; returns 0 on success. */
    int rogue_skill_debug_get_timing(int id, float* base_cooldown_ms, float* cd_red_ms_per_rank,
                                     float* cast_time_ms);
    int rogue_skill_debug_set_timing(int id, float base_cooldown_ms, float cd_red_ms_per_rank,
                                     float cast_time_ms);

    /* Simple wrapper to run rotation simulation and write result JSON. */
    int rogue_skill_debug_simulate(const char* profile_json, char* out_buf, int out_cap);

    /* Export all skills' timing + coeff overrides to a compact JSON array into out_buf.
        Returns number of bytes written (>=0) or <0 on overflow/error. */
    int rogue_skill_debug_export_overrides_json(char* out_buf, int out_cap);

    /* Parse overrides JSON text (same schema as export) and apply live to registry.
        Returns number of entries applied or <0 on parse error. */
    int rogue_skill_debug_load_overrides_text(const char* json_text);

    /* Save overrides to file atomically using content/json_io. Returns 0 on success. */
    int rogue_skill_debug_save_overrides(const char* path);

    /* Load overrides from a file path (reads entire file then delegates to _load_overrides_text).
        Returns number of entries applied (>=0) or <0 on read/parse error. */
    int rogue_skill_debug_load_overrides_file(const char* path);

    /* Auto-reload helper: if the overrides file at `path` has a newer mtime than the last
        successful check, load it and apply changes. Returns number of entries applied (>0 on
        updates, 0 when no changes), or <0 on error. Passing NULL uses the default
        "build/skills_overrides.json" path. Overhead is a single stat/GetFileAttributes call
        per invocation. */
    int rogue_skill_debug_autoreload_tick(const char* path);

    /* Create a new skill at runtime for prototyping. Returns new skill index (>=0),
        -1 on invalid params, -2 on duplicate name, <-10 on internal errors. The
        skill is appended to the registry with sensible defaults. Pass is_passive!=0
        to create a passive (no activation/cooldown). For actives, cast_time_ms<=0
        creates an instant skill; >0 creates a cast skill. */
    int rogue_skill_debug_create(const char* name, int max_rank, float base_cooldown_ms,
                                 float cd_red_ms_per_rank, float cast_time_ms, int is_passive);

    /* Auto-reload the base skills definition file (JSON) when its mtime changes. If path is NULL,
        defaults to "assets/skills_uhf87f.json". Returns number loaded on change, 0 when unchanged,
        and <0 on error. This performs a full registry rebuild via rogue_skills_reload_from_cfg. */
    int rogue_skills_base_autoreload_tick(const char* path);

    /* Fetch meta properties for a skill definition. Returns 0 on success. */
    int rogue_skill_debug_get_meta(int id, int* out_max_rank, int* out_is_passive);

    /* Validate the entire skills/procs/effects registry. Returns 0 on success (valid),
        -1 on validation failure and writes a short human-readable message into `err`
        when provided. Headless-safe for unit tests. */
    int rogue_skill_debug_validate(char* err, int err_cap);

    /* Visuals/Audio/AoE/Projectile/Animation debug struct and accessors (Phase groundwork) */
    typedef struct RogueSkillVisualParams
    {
        /* Paths (relative to assets/ allowed) */
        char cast_sprite_sheet[256];
        char projectile_sprite[256];
        char impact_sprite[256];
        char aoe_sprite[256];
        /* Animation */
        int frame_count;
        float frame_duration_ms;
        int animation_loops; /* 0/1 */
        int grid_width;
        int grid_height;
        /* Audio */
        char cast_sound_id[128];
        char impact_sound_id[128];
        char loop_sound_id[128];
        int sound_volume; /* 0..100 */
        float sound_pitch_variance;
        /* AoE */
        int aoe_shape; /* 0 none,1 circle,2 cone,3 line */
        float aoe_radius;
        float aoe_angle;
        /* Projectile */
        float projectile_velocity;
        int trajectory_type; /* 0 linear,1 arc,2 homing,3 scatter */
        int pierce_count;
        float homing_strength;
    } RogueSkillVisualParams;

    /* Returns 0 on success */
    int rogue_skill_debug_get_visuals(int id, RogueSkillVisualParams* out);
    int rogue_skill_debug_set_visuals(int id, const RogueSkillVisualParams* in);

    /* Skill type (RogueSkillType enum) accessors. Returns 0 on success. */
    int rogue_skill_debug_get_type(int id, int* out_skill_type);
    int rogue_skill_debug_set_type(int id, int skill_type);

    /* Effect composition (primary EffectSpec id and up to 3 additional nodes).
        Returns 0 on success. `nodes` may be NULL when only reading/writing the primary id.
        Node count is clamped to [0,3]. */
    int rogue_skill_debug_get_effects(int id, int* out_primary_effect_id,
                                      struct RogueSkillEffectNode* nodes, int* inout_node_count);
    int rogue_skill_debug_set_effects(int id, int primary_effect_id,
                                      const struct RogueSkillEffectNode* nodes, int node_count);

    /* Experimental EffectNode tree debugging (Phase 1.2 extension). When a tree is defined for
       a skill (node_count>0), the legacy flat effect_nodes[] array is ignored at runtime. */
    struct RogueSkillEffectTreeNodeDebug
    {
        int effect_spec_id;
        float delay_ms;
        float duration_ms;
        int repeat_count;
        float repeat_interval_ms;
        unsigned char require_player_health_below_pct;
        signed char parent_index; /* -1 root */
    };
    /* Get tree nodes: writes up to *inout_node_count entries into nodes; returns 0 success. */
    int rogue_skill_debug_get_effect_tree(int id, struct RogueSkillEffectTreeNodeDebug* nodes,
                                          int* inout_node_count);
    /* Set/replace entire tree (node_count 0..8). Returns 0 on success. */
    int rogue_skill_debug_set_effect_tree(int id, const struct RogueSkillEffectTreeNodeDebug* nodes,
                                          int node_count);

#ifdef __cplusplus
}
#endif

#endif /* ROGUE_CORE_SKILL_DEBUG_H */
