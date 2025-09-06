#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "skills_validate.h"
#include "../../graphics/effect_spec.h"
#include "../../util/path_utils.h"
#include "skills_coeffs.h"
#include "skills_internal.h"
#include "skills_procs.h"
#include <stdio.h>
#include <string.h>

static void set_err(char* err, int cap, const char* msg)
{
    if (err && cap > 0)
    {
#ifdef _MSC_VER
        strncpy_s(err, (size_t) cap, msg, _TRUNCATE);
#else
        strncpy(err, msg, (size_t) cap - 1);
        err[cap - 1] = '\0';
#endif
    }
}

/* Heuristic: consider a skill "offensive" if it references an EffectSpec or has non-zero AP/cast.
 */
static int is_skill_offensive(const RogueSkillDef* d)
{
    if (!d)
        return 0;
    /* Treat effect_spec_id==0 as unset (CSV default), only >0 considered authored. */
    if (d->effect_spec_id > 0)
        return 1;
    if (d->action_point_cost > 0 || d->resource_cost_mana > 0)
        return 1;
    if (d->cast_time_ms > 0.0f)
        return 1;
    return 0;
}

int rogue_skills_validate_all(char* err, int err_cap)
{
    /* 1) Invalid references in skills */
    for (int i = 0; i < g_skill_count_internal; ++i)
    {
        const RogueSkillDef* d = &g_skill_defs_internal[i];
        /* Treat effect_spec_id==0 as "unset" for backward compat with CSV defaults. */
        if (d->effect_spec_id > 0)
        {
            if (rogue_effect_get(d->effect_spec_id) == NULL)
            {
                char buf[256];
                snprintf(buf, sizeof buf, "invalid skill.effect_spec_id id=%d idx=%d",
                         d->effect_spec_id, i);
                set_err(err, err_cap, buf);
                return -1;
            }
        }
    }

    /* 2) Invalid references in procs + build simple event->effect graph; effects to events mapping
          is approximated by a few known links (CHANNEL_TICK -> on hit like effects). */
    int pc = rogue_skills_proc_count();
    for (int i = 0; i < pc; ++i)
    {
        RogueProcDef def;
        if (!rogue_skills_proc_get_def(i, &def))
            continue;
        if (def.effect_spec_id >= 0 && !rogue_effect_get(def.effect_spec_id))
        {
            char buf[256];
            snprintf(buf, sizeof buf, "invalid proc.effect_spec_id id=%d idx=%d",
                     def.effect_spec_id, i);
            set_err(err, err_cap, buf);
            return -1;
        }
    }

    /* 3) Cycle detection: if any two procs apply the same effect on the same event, we can't
          prove a cycle without concrete effect->event edges, but we can detect self-proc cycles
          when an event triggers an effect whose application emits the same event immediately. In
          our engine, applying an effect doesn't synchronously emit SKILL_* events, so restrict to
          a conservative check: duplicate (event_type,effect_spec_id) pairs repeated more than N
          times can lead to bursty recursion when re-published; flag duplicates as warnings. */
    for (int i = 0; i < pc; ++i)
    {
        RogueProcDef a;
        if (!rogue_skills_proc_get_def(i, &a))
            continue;
        for (int j = i + 1; j < pc; ++j)
        {
            RogueProcDef b;
            if (!rogue_skills_proc_get_def(j, &b))
                continue;
            if (a.event_type == b.event_type && a.effect_spec_id >= 0 &&
                a.effect_spec_id == b.effect_spec_id)
            {
                char buf[256];
                snprintf(buf, sizeof buf, "duplicate proc pair may cause cycles: evt=%u eff=%d",
                         (unsigned) a.event_type, a.effect_spec_id);
                set_err(err, err_cap, buf);
                return -1;
            }
        }
    }

    /* 4) Missing coefficients: flag offensive skills that lack coeff entries. */
    for (int i = 0; i < g_skill_count_internal; ++i)
    {
        const RogueSkillDef* d = &g_skill_defs_internal[i];
        if (is_skill_offensive(d) && !rogue_skill_coeff_exists(i))
        {
            char buf[256];
            snprintf(buf, sizeof buf,
                     "skill %d '%s' appears offensive but has no coefficient entry", i,
                     d->name ? d->name : "<noname>");
            set_err(err, err_cap, buf);
            return -1;
        }
    }

    /* 5) Basic visuals/audio/AoE/projectile sanity checks (non-fatal but fail validation). */
    for (int i = 0; i < g_skill_count_internal; ++i)
    {
        const RogueSkillDef* d = &g_skill_defs_internal[i];
        /* Effect composition node validation */
        if (d->effect_node_count > 3)
        {
            set_err(err, err_cap, "effect_node_count out of range");
            return -1;
        }
        for (int ni = 0; ni < (int) d->effect_node_count; ++ni)
        {
            const int eid = d->effect_nodes[ni].effect_spec_id;
            if (eid > 0 && !rogue_effect_get(eid))
            {
                char buf2[256];
                snprintf(buf2, sizeof buf2, "skill %d '%s' effect%d_spec_id invalid: %d", i,
                         d->name ? d->name : "<noname>", ni + 2, eid);
                set_err(err, err_cap, buf2);
                return -1;
            }
            if (d->effect_nodes[ni].duration_ms < 0.0f)
            {
                char buf3a[256];
                snprintf(buf3a, sizeof buf3a, "skill %d '%s' effect%d_duration_ms must be >= 0", i,
                         d->name ? d->name : "<noname>", ni + 2);
                set_err(err, err_cap, buf3a);
                return -1;
            }
            if (d->effect_nodes[ni].repeat_count < 0 || d->effect_nodes[ni].repeat_count > 32)
            {
                char buf3[256];
                snprintf(buf3, sizeof buf3, "skill %d '%s' effect%d_repeat_count out of range", i,
                         d->name ? d->name : "<noname>", ni + 2);
                set_err(err, err_cap, buf3);
                return -1;
            }
            if (d->effect_nodes[ni].repeat_count == 0 && d->effect_nodes[ni].duration_ms > 0.0f &&
                d->effect_nodes[ni].repeat_interval_ms <= 0.0f)
            {
                char buf3b[256];
                snprintf(buf3b, sizeof buf3b,
                         "skill %d '%s' effect%d_duration_ms provided but repeat_interval_ms <= 0",
                         i, d->name ? d->name : "<noname>", ni + 2);
                set_err(err, err_cap, buf3b);
                return -1;
            }
            if (d->effect_nodes[ni].require_player_health_below_pct > 100)
            {
                char buf4[256];
                snprintf(buf4, sizeof buf4,
                         "skill %d '%s' effect%d_require_player_health_below_pct > 100", i,
                         d->name ? d->name : "<noname>", ni + 2);
                set_err(err, err_cap, buf4);
                return -1;
            }
            /* Advanced validation heuristics (Phase 1.2 "Advanced Effect Validation"): */
            if (d->effect_nodes[ni].delay_ms < 0.0f)
            {
                char bufDelay[256];
                snprintf(bufDelay, sizeof bufDelay, "skill %d '%s' effect%d_delay_ms must be >= 0",
                         i, d->name ? d->name : "<noname>", ni + 2);
                set_err(err, err_cap, bufDelay);
                return -1;
            }
            if (d->effect_nodes[ni].repeat_count > 0 &&
                d->effect_nodes[ni].repeat_interval_ms <= 0.0f)
            {
                char bufRCInt[256];
                snprintf(bufRCInt, sizeof bufRCInt,
                         "skill %d '%s' effect%d_repeat_interval_ms must be >0 when repeat_count>0",
                         i, d->name ? d->name : "<noname>", ni + 2);
                set_err(err, err_cap, bufRCInt);
                return -1;
            }
            if (d->effect_nodes[ni].repeat_count > 0 && d->effect_nodes[ni].duration_ms > 0.0f)
            {
                float window_needed = (float) d->effect_nodes[ni].repeat_count *
                                      d->effect_nodes[ni].repeat_interval_ms;
                if (d->effect_nodes[ni].duration_ms + 1e-3f < window_needed)
                {
                    char bufSpan[256];
                    snprintf(bufSpan, sizeof bufSpan,
                             "skill %d '%s' effect%d_duration_ms < repeat_count*repeat_interval_ms",
                             i, d->name ? d->name : "<noname>", ni + 2);
                    set_err(err, err_cap, bufSpan);
                    return -1;
                }
            }
        }
        /* Effect tree (if present) supersedes flat effect_nodes at runtime; validate separately */
        if (d->effect_tree_node_count > 0)
        {
            if (d->effect_tree_node_count > 8)
            {
                set_err(err, err_cap, "effect_tree_node_count out of range");
                return -1;
            }
            /* Basic field checks */
            for (int ti = 0; ti < (int) d->effect_tree_node_count; ++ti)
            {
                const int eid = d->effect_tree_nodes[ti].effect_spec_id;
                if (eid > 0 && !rogue_effect_get(eid))
                {
                    char bufT[256];
                    snprintf(bufT, sizeof bufT,
                             "skill %d '%s' tree.node[%d] invalid effect_spec_id=%d", i,
                             d->name ? d->name : "<noname>", ti, eid);
                    set_err(err, err_cap, bufT);
                    return -1;
                }
                /* Authoring-time SPAWN_ENTITY validation (pre-runtime clamp) */
                if (eid > 0)
                {
                    const RogueEffectSpec* es_spawn = rogue_effect_get(eid);
                    if (es_spawn && es_spawn->kind == ROGUE_EFFECT_SPAWN_ENTITY)
                    {
                        if (es_spawn->spawn_entity_count <= 0 || es_spawn->spawn_entity_count > 8)
                        {
                            char bufSE[256];
                            snprintf(bufSE, sizeof bufSE,
                                     "skill %d '%s' tree.node[%d] spawn_entity_count out of range "
                                     "(1..8)",
                                     i, d->name ? d->name : "<noname>", ti);
                            set_err(err, err_cap, bufSE);
                            return -1;
                        }
                        if (es_spawn->spawn_entity_life_ms <= 0.0f)
                        {
                            char bufSE2[256];
                            snprintf(bufSE2, sizeof bufSE2,
                                     "skill %d '%s' tree.node[%d] spawn_entity_life_ms must be > 0",
                                     i, d->name ? d->name : "<noname>", ti);
                            set_err(err, err_cap, bufSE2);
                            return -1;
                        }
                    }
                }
                if (d->effect_tree_nodes[ti].delay_ms < 0.0f)
                {
                    char bufTD0[256];
                    snprintf(bufTD0, sizeof bufTD0,
                             "skill %d '%s' tree.node[%d] delay_ms must be >=0", i,
                             d->name ? d->name : "<noname>", ti);
                    set_err(err, err_cap, bufTD0);
                    return -1;
                }
                if (d->effect_tree_nodes[ti].duration_ms < 0.0f)
                {
                    char bufT2[256];
                    snprintf(bufT2, sizeof bufT2,
                             "skill %d '%s' tree.node[%d] duration_ms must be >=0", i,
                             d->name ? d->name : "<noname>", ti);
                    set_err(err, err_cap, bufT2);
                    return -1;
                }
                if (d->effect_tree_nodes[ti].repeat_count < 0 ||
                    d->effect_tree_nodes[ti].repeat_count > 32)
                {
                    char bufT3[256];
                    snprintf(bufT3, sizeof bufT3,
                             "skill %d '%s' tree.node[%d] repeat_count out of range", i,
                             d->name ? d->name : "<noname>", ti);
                    set_err(err, err_cap, bufT3);
                    return -1;
                }
                if (d->effect_tree_nodes[ti].repeat_count == 0 &&
                    d->effect_tree_nodes[ti].duration_ms > 0.0f &&
                    d->effect_tree_nodes[ti].repeat_interval_ms <= 0.0f)
                {
                    char bufT4[256];
                    snprintf(bufT4, sizeof bufT4,
                             "skill %d '%s' tree.node[%d] duration_ms>0 but repeat_interval_ms<=0",
                             i, d->name ? d->name : "<noname>", ti);
                    set_err(err, err_cap, bufT4);
                    return -1;
                }
                if (d->effect_tree_nodes[ti].repeat_count > 0 &&
                    d->effect_tree_nodes[ti].repeat_interval_ms <= 0.0f)
                {
                    char bufTD6[256];
                    snprintf(bufTD6, sizeof bufTD6,
                             "skill %d '%s' tree.node[%d] repeat_interval_ms must be >0 when "
                             "repeat_count>0",
                             i, d->name ? d->name : "<noname>", ti);
                    set_err(err, err_cap, bufTD6);
                    return -1;
                }
                if (d->effect_tree_nodes[ti].repeat_count > 0 &&
                    d->effect_tree_nodes[ti].duration_ms > 0.0f)
                {
                    float need = (float) d->effect_tree_nodes[ti].repeat_count *
                                 d->effect_tree_nodes[ti].repeat_interval_ms;
                    if (d->effect_tree_nodes[ti].duration_ms + 1e-3f < need)
                    {
                        char bufTD7[256];
                        snprintf(bufTD7, sizeof bufTD7,
                                 "skill %d '%s' tree.node[%d] duration_ms < "
                                 "repeat_count*repeat_interval_ms",
                                 i, d->name ? d->name : "<noname>", ti);
                        set_err(err, err_cap, bufTD7);
                        return -1;
                    }
                }
                if (d->effect_tree_nodes[ti].require_player_health_below_pct > 100)
                {
                    char bufT5[256];
                    snprintf(bufT5, sizeof bufT5, "skill %d '%s' tree.node[%d] hp_gate > 100", i,
                             d->name ? d->name : "<noname>", ti);
                    set_err(err, err_cap, bufT5);
                    return -1;
                }
                int pid = d->effect_tree_nodes[ti].parent_index;
                if (pid >= (int) d->effect_tree_node_count)
                {
                    char bufT6[256];
                    snprintf(bufT6, sizeof bufT6,
                             "skill %d '%s' tree.node[%d] parent_index out of range", i,
                             d->name ? d->name : "<noname>", ti);
                    set_err(err, err_cap, bufT6);
                    return -1;
                }
            }
            /* Cycle detection: depth-first with color marking */
            unsigned char color[8];
            for (int k = 0; k < (int) d->effect_tree_node_count; ++k)
                color[k] = 0; /* 0 unvisited,1 in-stack,2 done */
            for (int k = 0; k < (int) d->effect_tree_node_count; ++k)
            {
                int v = k;
                int guard = 0;
                while (v >= 0 && v < (int) d->effect_tree_node_count)
                {
                    if (color[v] == 1)
                    {
                        char bufC[256];
                        snprintf(bufC, sizeof bufC, "skill %d '%s' effect tree cycle detected", i,
                                 d->name ? d->name : "<noname>");
                        set_err(err, err_cap, bufC);
                        return -1;
                    }
                    if (color[v] == 2)
                        break;
                    color[v] = 1; /* in stack */
                    v = d->effect_tree_nodes[v].parent_index;
                    if (++guard > 16)
                        break; /* safety */
                }
                /* backtrack mark */
                v = k;
                guard = 0;
                while (v >= 0 && v < (int) d->effect_tree_node_count && color[v] == 1 &&
                       guard++ < 16)
                {
                    color[v] = 2;
                    v = d->effect_tree_nodes[v].parent_index;
                }
            }
            /* Span heuristic: limit total scheduled span to 60s to catch runaway authoring.
               Single-fire nodes (no repeats, zero duration) are treated as instantaneous events
               with a minimal intrinsic span of 1ms so that long delay chains still compute a
               non-zero end time. */
            const float MAX_TREE_SPAN_MS = 60000.0f;
            const float MIN_INSTANT_SPAN_MS = 1.0f;
            for (int ti = 0; ti < (int) d->effect_tree_node_count; ++ti)
            {
                /* compute start time by summing delays up chain */
                float start = 0.0f;
                int cur = ti;
                int guard = 0;
                while (cur >= 0 && cur < (int) d->effect_tree_node_count && guard++ < 16)
                {
                    start += d->effect_tree_nodes[cur].delay_ms;
                    cur = d->effect_tree_nodes[cur].parent_index;
                }
                float span = 0.0f;
                if (d->effect_tree_nodes[ti].repeat_count > 0)
                {
                    span = (float) d->effect_tree_nodes[ti].repeat_count *
                           d->effect_tree_nodes[ti].repeat_interval_ms;
                }
                else if (d->effect_tree_nodes[ti].duration_ms > 0.0f)
                {
                    span = d->effect_tree_nodes[ti].duration_ms;
                }
                else
                {
                    span = MIN_INSTANT_SPAN_MS; /* instantaneous event */
                }
                float end = start + span;
                if (end > MAX_TREE_SPAN_MS)
                {
                    char bufSpan[256];
                    snprintf(bufSpan, sizeof bufSpan,
                             "skill %d '%s' effect tree scheduled span exceeds %.0fms", i,
                             d->name ? d->name : "<noname>", MAX_TREE_SPAN_MS);
                    set_err(err, err_cap, bufSpan);
                    return -1;
                }
            }
        }
        /* Authoring-time SPAWN_ENTITY validation for primary + flat nodes (after tree to reuse
         * pattern) */
        if (d->effect_spec_id > 0)
        {
            const RogueEffectSpec* es_se = rogue_effect_get(d->effect_spec_id);
            if (es_se && es_se->kind == ROGUE_EFFECT_SPAWN_ENTITY)
            {
                if (es_se->spawn_entity_count <= 0 || es_se->spawn_entity_count > 8)
                {
                    char bufSE3[256];
                    snprintf(bufSE3, sizeof bufSE3,
                             "skill %d '%s' primary spawn_entity_count out of range (1..8)", i,
                             d->name ? d->name : "<noname>");
                    set_err(err, err_cap, bufSE3);
                    return -1;
                }
                if (es_se->spawn_entity_life_ms <= 0.0f)
                {
                    char bufSE4[256];
                    snprintf(bufSE4, sizeof bufSE4,
                             "skill %d '%s' primary spawn_entity_life_ms must be > 0", i,
                             d->name ? d->name : "<noname>");
                    set_err(err, err_cap, bufSE4);
                    return -1;
                }
            }
        }
        for (int ni = 0; ni < (int) d->effect_node_count; ++ni)
        {
            int eid2 = d->effect_nodes[ni].effect_spec_id;
            if (eid2 > 0)
            {
                const RogueEffectSpec* es_se2 = rogue_effect_get(eid2);
                if (es_se2 && es_se2->kind == ROGUE_EFFECT_SPAWN_ENTITY)
                {
                    if (es_se2->spawn_entity_count <= 0 || es_se2->spawn_entity_count > 8)
                    {
                        char bufSE5[256];
                        snprintf(bufSE5, sizeof bufSE5,
                                 "skill %d '%s' effect%d spawn_entity_count out of range (1..8)", i,
                                 d->name ? d->name : "<noname>", ni + 2);
                        set_err(err, err_cap, bufSE5);
                        return -1;
                    }
                    if (es_se2->spawn_entity_life_ms <= 0.0f)
                    {
                        char bufSE6[256];
                        snprintf(bufSE6, sizeof bufSE6,
                                 "skill %d '%s' effect%d spawn_entity_life_ms must be > 0", i,
                                 d->name ? d->name : "<noname>", ni + 2);
                        set_err(err, err_cap, bufSE6);
                        return -1;
                    }
                }
            }
        }
        /* Clamp and consistency: if skill_type is out of range, treat as UNKNOWN; if PASSIVE type
         * but not flagged passive, warn-fail for now. */
        if (d->skill_type > ROGUE_SKTYPE_ULTIMATE)
        {
            char buf[256];
            snprintf(buf, sizeof buf, "skill %d '%s' invalid skill_type: %u", i,
                     d->name ? d->name : "<noname>", (unsigned) d->skill_type);
            set_err(err, err_cap, buf);
            return -1;
        }
        if (d->skill_type == ROGUE_SKTYPE_PASSIVE && !d->is_passive)
        {
            char buf[256];
            snprintf(buf, sizeof buf, "skill %d '%s' type=PASSIVE but is_passive=0 (mismatch)", i,
                     d->name ? d->name : "<noname>");
            set_err(err, err_cap, buf);
            return -1;
        }
        char apath[512];
        if (d->cast_sprite_sheet && d->cast_sprite_sheet[0])
        {
            if (!rogue_find_asset_path(d->cast_sprite_sheet, apath, (int) sizeof apath))
            {
                char buf[256];
                snprintf(buf, sizeof buf, "skill %d '%s' cast_sprite_sheet missing: %s", i,
                         d->name ? d->name : "<noname>", d->cast_sprite_sheet);
                set_err(err, err_cap, buf);
                return -1;
            }
        }
        if (d->projectile_sprite && d->projectile_sprite[0])
        {
            if (!rogue_find_asset_path(d->projectile_sprite, apath, (int) sizeof apath))
            {
                char buf[256];
                snprintf(buf, sizeof buf, "skill %d '%s' projectile_sprite missing: %s", i,
                         d->name ? d->name : "<noname>", d->projectile_sprite);
                set_err(err, err_cap, buf);
                return -1;
            }
        }
        if (d->impact_sprite && d->impact_sprite[0])
        {
            if (!rogue_find_asset_path(d->impact_sprite, apath, (int) sizeof apath))
            {
                char buf[256];
                snprintf(buf, sizeof buf, "skill %d '%s' impact_sprite missing: %s", i,
                         d->name ? d->name : "<noname>", d->impact_sprite);
                set_err(err, err_cap, buf);
                return -1;
            }
        }
        if (d->aoe_sprite && d->aoe_sprite[0])
        {
            if (!rogue_find_asset_path(d->aoe_sprite, apath, (int) sizeof apath))
            {
                char buf[256];
                snprintf(buf, sizeof buf, "skill %d '%s' aoe_sprite missing: %s", i,
                         d->name ? d->name : "<noname>", d->aoe_sprite);
                set_err(err, err_cap, buf);
                return -1;
            }
        }
        if (d->frame_count < 0 || d->frame_duration_ms < 0.0f)
        {
            char buf[256];
            snprintf(buf, sizeof buf, "skill %d '%s' invalid animation params (frames=%d, dt=%.2f)",
                     i, d->name ? d->name : "<noname>", d->frame_count, d->frame_duration_ms);
            set_err(err, err_cap, buf);
            return -1;
        }
        if (d->sound_volume > 100)
        {
            char buf[256];
            snprintf(buf, sizeof buf, "skill %d '%s' sound_volume out of range: %u", i,
                     d->name ? d->name : "<noname>", (unsigned) d->sound_volume);
            set_err(err, err_cap, buf);
            return -1;
        }
        if (d->aoe_shape > 0)
        {
            if (d->aoe_radius < 0.0f)
            {
                char buf[256];
                snprintf(buf, sizeof buf, "skill %d '%s' invalid aoe_radius: %.2f", i,
                         d->name ? d->name : "<noname>", d->aoe_radius);
                set_err(err, err_cap, buf);
                return -1;
            }
        }
        if (d->projectile_velocity < 0.0f)
        {
            char buf[256];
            snprintf(buf, sizeof buf, "skill %d '%s' invalid projectile_velocity: %.2f", i,
                     d->name ? d->name : "<noname>", d->projectile_velocity);
            set_err(err, err_cap, buf);
            return -1;
        }
    }

    return 0;
}
