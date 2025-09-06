#include "skill_debug.h"
#include "../../content/json_io.h"
#include "../app/app_state.h"
#include "skills.h"
#include "skills_coeffs.h"
#include "skills_internal.h"
#include "skills_validate.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>

int rogue_skill_debug_count(void) { return g_app.skill_count; }

const char* rogue_skill_debug_name(int id)
{
    const RogueSkillDef* d = rogue_skill_get_def(id);
    return d && d->name ? d->name : "<noname>";
}

int rogue_skill_debug_get_coeff(int id, RogueSkillCoeffParams* out)
{
    if (!out)
        return -1;
    return rogue_skill_coeff_get_params(id, out);
}

int rogue_skill_debug_set_coeff(int id, const RogueSkillCoeffParams* in)
{
    if (!in)
        return -1;
    return rogue_skill_coeff_register(id, in);
}

int rogue_skill_debug_get_timing(int id, float* base_cooldown_ms, float* cd_red_ms_per_rank,
                                 float* cast_time_ms)
{
    const RogueSkillDef* d = rogue_skill_get_def(id);
    if (!d)
        return -1;
    if (base_cooldown_ms)
        *base_cooldown_ms = d->base_cooldown_ms;
    if (cd_red_ms_per_rank)
        *cd_red_ms_per_rank = d->cooldown_reduction_ms_per_rank;
    if (cast_time_ms)
        *cast_time_ms = d->cast_time_ms;
    return 0;
}

int rogue_skill_debug_set_timing(int id, float base_cooldown_ms, float cd_red_ms_per_rank,
                                 float cast_time_ms)
{
    RogueSkillDef* d = NULL;
    if (id < 0 || id >= g_app.skill_count)
        return -1;
    d = &g_app.skill_defs[id];
    d->base_cooldown_ms = base_cooldown_ms;
    d->cooldown_reduction_ms_per_rank = cd_red_ms_per_rank;
    d->cast_time_ms = cast_time_ms;
    return 0;
}

int rogue_skill_debug_simulate(const char* profile_json, char* out_buf, int out_cap)
{
    return skill_simulate_rotation(profile_json, out_buf, out_cap);
}

int rogue_skill_debug_get_visuals(int id, RogueSkillVisualParams* out)
{
    if (!out)
        return -1;
    const RogueSkillDef* d = rogue_skill_get_def(id);
    if (!d)
        return -1;
    /* Paths */
    out->cast_sprite_sheet[0] = '\0';
    out->projectile_sprite[0] = '\0';
    out->impact_sprite[0] = '\0';
    out->aoe_sprite[0] = '\0';
#if defined(_MSC_VER)
    if (d->cast_sprite_sheet)
        strncpy_s(out->cast_sprite_sheet, sizeof out->cast_sprite_sheet, d->cast_sprite_sheet,
                  _TRUNCATE);
    if (d->projectile_sprite)
        strncpy_s(out->projectile_sprite, sizeof out->projectile_sprite, d->projectile_sprite,
                  _TRUNCATE);
    if (d->impact_sprite)
        strncpy_s(out->impact_sprite, sizeof out->impact_sprite, d->impact_sprite, _TRUNCATE);
    if (d->aoe_sprite)
        strncpy_s(out->aoe_sprite, sizeof out->aoe_sprite, d->aoe_sprite, _TRUNCATE);
#else
    if (d->cast_sprite_sheet)
        strncpy(out->cast_sprite_sheet, d->cast_sprite_sheet, sizeof out->cast_sprite_sheet - 1);
    if (d->projectile_sprite)
        strncpy(out->projectile_sprite, d->projectile_sprite, sizeof out->projectile_sprite - 1);
    if (d->impact_sprite)
        strncpy(out->impact_sprite, d->impact_sprite, sizeof out->impact_sprite - 1);
    if (d->aoe_sprite)
        strncpy(out->aoe_sprite, d->aoe_sprite, sizeof out->aoe_sprite - 1);
    out->cast_sprite_sheet[sizeof out->cast_sprite_sheet - 1] = '\0';
    out->projectile_sprite[sizeof out->projectile_sprite - 1] = '\0';
    out->impact_sprite[sizeof out->impact_sprite - 1] = '\0';
    out->aoe_sprite[sizeof out->aoe_sprite - 1] = '\0';
#endif
    /* Animation */
    out->frame_count = d->frame_count;
    out->frame_duration_ms = d->frame_duration_ms;
    out->animation_loops = d->animation_loops;
    out->grid_width = d->grid_width;
    out->grid_height = d->grid_height;
    /* Audio */
    out->cast_sound_id[0] = '\0';
    out->impact_sound_id[0] = '\0';
    out->loop_sound_id[0] = '\0';
#if defined(_MSC_VER)
    if (d->cast_sound_id)
        strncpy_s(out->cast_sound_id, sizeof out->cast_sound_id, d->cast_sound_id, _TRUNCATE);
    if (d->impact_sound_id)
        strncpy_s(out->impact_sound_id, sizeof out->impact_sound_id, d->impact_sound_id, _TRUNCATE);
    if (d->loop_sound_id)
        strncpy_s(out->loop_sound_id, sizeof out->loop_sound_id, d->loop_sound_id, _TRUNCATE);
#else
    if (d->cast_sound_id)
        strncpy(out->cast_sound_id, d->cast_sound_id, sizeof out->cast_sound_id - 1);
    if (d->impact_sound_id)
        strncpy(out->impact_sound_id, d->impact_sound_id, sizeof out->impact_sound_id - 1);
    if (d->loop_sound_id)
        strncpy(out->loop_sound_id, d->loop_sound_id, sizeof out->loop_sound_id - 1);
    out->cast_sound_id[sizeof out->cast_sound_id - 1] = '\0';
    out->impact_sound_id[sizeof out->impact_sound_id - 1] = '\0';
    out->loop_sound_id[sizeof out->loop_sound_id - 1] = '\0';
#endif
    out->sound_volume = d->sound_volume;
    out->sound_pitch_variance = d->sound_pitch_variance;
    /* AoE */
    out->aoe_shape = d->aoe_shape;
    out->aoe_radius = d->aoe_radius;
    out->aoe_angle = d->aoe_angle;
    /* Projectile */
    out->projectile_velocity = d->projectile_velocity;
    out->trajectory_type = d->trajectory_type;
    out->pierce_count = d->pierce_count;
    out->homing_strength = d->homing_strength;
    return 0;
}

int rogue_skill_debug_set_visuals(int id, const RogueSkillVisualParams* in)
{
    if (!in)
        return -1;
    if (id < 0 || id >= g_app.skill_count)
        return -1;
    RogueSkillDef* d = &g_app.skill_defs[id];
    /* Replace strings: free existing then duplicate new when non-empty */
    const char* fields_str_src[7] = {
        in->cast_sprite_sheet, in->projectile_sprite, in->impact_sprite, in->aoe_sprite,
        in->cast_sound_id,     in->impact_sound_id,   in->loop_sound_id};
    const char** fields_dst[7] = {&d->cast_sprite_sheet, &d->projectile_sprite, &d->impact_sprite,
                                  &d->aoe_sprite,        &d->cast_sound_id,     &d->impact_sound_id,
                                  &d->loop_sound_id};
    for (int i = 0; i < 7; ++i)
    {
        if (*fields_dst[i])
        {
            free((char*) *fields_dst[i]);
            *fields_dst[i] = NULL;
        }
        const char* src = fields_str_src[i];
        if (src && src[0])
        {
            size_t len = strlen(src);
            char* dup = (char*) malloc(len + 1);
            if (!dup)
                return -2;
            memcpy(dup, src, len + 1);
            *fields_dst[i] = dup;
        }
    }
    /* Scalars */
    d->frame_count = in->frame_count;
    d->frame_duration_ms = in->frame_duration_ms;
    d->animation_loops = (unsigned char) (in->animation_loops ? 1 : 0);
    d->grid_width = (unsigned short) (in->grid_width > 0 ? in->grid_width : 0);
    d->grid_height = (unsigned short) (in->grid_height > 0 ? in->grid_height : 0);
    d->sound_volume = (unsigned char) ((in->sound_volume < 0)
                                           ? 0
                                           : (in->sound_volume > 100 ? 100 : in->sound_volume));
    d->sound_pitch_variance = in->sound_pitch_variance;
    d->aoe_shape = (unsigned char) ((in->aoe_shape < 0) ? 0 : in->aoe_shape);
    d->aoe_radius = in->aoe_radius;
    d->aoe_angle = in->aoe_angle;
    d->projectile_velocity = in->projectile_velocity;
    d->trajectory_type = (unsigned char) ((in->trajectory_type < 0) ? 0 : in->trajectory_type);
    d->pierce_count = (unsigned char) ((in->pierce_count < 0) ? 0 : in->pierce_count);
    d->homing_strength = in->homing_strength;
    return 0;
}

int rogue_skill_debug_get_type(int id, int* out_skill_type)
{
    if (!out_skill_type)
        return -1;
    const RogueSkillDef* d = rogue_skill_get_def(id);
    if (!d)
        return -1;
    *out_skill_type = (int) d->skill_type;
    return 0;
}

int rogue_skill_debug_set_type(int id, int skill_type)
{
    if (id < 0 || id >= g_app.skill_count)
        return -1;
    if (skill_type < 0)
        skill_type = 0;
    if (skill_type > 9)
        skill_type = 9;
    g_app.skill_defs[id].skill_type = (unsigned char) skill_type;
    return 0;
}

int rogue_skill_debug_get_effects(int id, int* out_primary_effect_id,
                                  struct RogueSkillEffectNode* nodes, int* inout_node_count)
{
    const RogueSkillDef* d = rogue_skill_get_def(id);
    if (!d)
        return -1;
    if (out_primary_effect_id)
        *out_primary_effect_id = d->effect_spec_id;
    if (nodes && inout_node_count && *inout_node_count > 0)
    {
        int n = d->effect_node_count;
        if (n > 3)
            n = 3;
        int cap = *inout_node_count;
        if (n > cap)
            n = cap;
        for (int i = 0; i < n; ++i)
            nodes[i] = d->effect_nodes[i];
        *inout_node_count = n;
    }
    return 0;
}

int rogue_skill_debug_set_effects(int id, int primary_effect_id,
                                  const struct RogueSkillEffectNode* nodes, int node_count)
{
    if (id < 0 || id >= g_app.skill_count)
        return -1;
    RogueSkillDef* d = &g_app.skill_defs[id];
    d->effect_spec_id = primary_effect_id;
    if (node_count < 0)
        node_count = 0;
    if (node_count > 3)
        node_count = 3;
    d->effect_node_count = (unsigned char) node_count;
    for (int i = 0; i < node_count; ++i)
    {
        d->effect_nodes[i] = nodes[i];
    }
    for (int i = node_count; i < 3; ++i)
    {
        d->effect_nodes[i].effect_spec_id = -1;
        d->effect_nodes[i].delay_ms = 0.0f;
        d->effect_nodes[i].duration_ms = 0.0f;
        d->effect_nodes[i].repeat_count = 0;
        d->effect_nodes[i].repeat_interval_ms = 0.0f;
        d->effect_nodes[i].require_player_health_below_pct = 0;
    }
    return 0;
}

/* --- EffectNode Tree Debug Accessors (experimental) --------------------------------------- */
int rogue_skill_debug_get_effect_tree(int id, struct RogueSkillEffectTreeNodeDebug* nodes,
                                      int* inout_node_count)
{
    if (id < 0 || id >= g_app.skill_count)
        return -1;
    if (!nodes || !inout_node_count || *inout_node_count <= 0)
        return -1;
    RogueSkillDef* d = &g_app.skill_defs[id];
    int n = d->effect_tree_node_count;
    if (n > 8)
        n = 8;
    if (n > *inout_node_count)
        n = *inout_node_count;
    for (int i = 0; i < n; ++i)
    {
        nodes[i].effect_spec_id = d->effect_tree_nodes[i].effect_spec_id;
        nodes[i].delay_ms = d->effect_tree_nodes[i].delay_ms;
        nodes[i].duration_ms = d->effect_tree_nodes[i].duration_ms;
        nodes[i].repeat_count = d->effect_tree_nodes[i].repeat_count;
        nodes[i].repeat_interval_ms = d->effect_tree_nodes[i].repeat_interval_ms;
        nodes[i].require_player_health_below_pct =
            d->effect_tree_nodes[i].require_player_health_below_pct;
        nodes[i].parent_index = d->effect_tree_nodes[i].parent_index;
    }
    *inout_node_count = n;
    return 0;
}

int rogue_skill_debug_set_effect_tree(int id, const struct RogueSkillEffectTreeNodeDebug* nodes,
                                      int node_count)
{
    if (id < 0 || id >= g_app.skill_count)
        return -1;
    RogueSkillDef* d = &g_app.skill_defs[id];
    if (node_count < 0)
        node_count = 0;
    if (node_count > 8)
        node_count = 8;
    d->effect_tree_node_count = (unsigned char) node_count;
    for (int i = 0; i < node_count; ++i)
    {
        d->effect_tree_nodes[i].effect_spec_id = nodes[i].effect_spec_id;
        d->effect_tree_nodes[i].delay_ms = nodes[i].delay_ms;
        d->effect_tree_nodes[i].duration_ms = nodes[i].duration_ms;
        d->effect_tree_nodes[i].repeat_count = nodes[i].repeat_count;
        d->effect_tree_nodes[i].repeat_interval_ms = nodes[i].repeat_interval_ms;
        d->effect_tree_nodes[i].require_player_health_below_pct =
            nodes[i].require_player_health_below_pct;
        d->effect_tree_nodes[i].parent_index = nodes[i].parent_index;
    }
    for (int i = node_count; i < 8; ++i)
    {
        d->effect_tree_nodes[i].effect_spec_id = -1;
        d->effect_tree_nodes[i].delay_ms = 0.0f;
        d->effect_tree_nodes[i].duration_ms = 0.0f;
        d->effect_tree_nodes[i].repeat_count = 0;
        d->effect_tree_nodes[i].repeat_interval_ms = 0.0f;
        d->effect_tree_nodes[i].require_player_health_below_pct = 0;
        d->effect_tree_nodes[i].parent_index = -1;
    }
    return 0;
}

/* --- Overrides JSON export/import ---------------------------------------------------------- */

int rogue_skill_debug_export_overrides_json(char* out_buf, int out_cap)
{
    if (!out_buf || out_cap <= 0)
        return -1;
    int w = 0;
    int n = snprintf(out_buf, out_cap, "[");
    if (n < 0 || n >= out_cap)
        return -1;
    w += n;
    for (int i = 0; i < g_app.skill_count; ++i)
    {
        const RogueSkillDef* d = rogue_skill_get_def(i);
        if (!d)
            continue;
        RogueSkillCoeffParams cp = {0};
        int has_coeff = rogue_skill_coeff_get_params(i, &cp) == 0;
        /* Write object with present fields */
        char name_sanitized[128];
        const char* nm = (d->name ? d->name : "");
        int ni = 0;
        for (const char* p = nm; *p && ni + 1 < (int) sizeof name_sanitized; ++p)
        {
            char c = *p;
            if (c == '"' || c == '\\')
            {
                if (ni + 2 >= (int) sizeof name_sanitized)
                    break;
                name_sanitized[ni++] = '\\';
                name_sanitized[ni++] = c;
            }
            else
                name_sanitized[ni++] = c;
        }
        name_sanitized[ni] = '\0';
        n = snprintf(out_buf + w, out_cap - w,
                     "%s{\"skill_id\":%d,\"name\":\"%s\",\"base_cooldown_ms\":%.3f,"
                     "\"cd_red_ms_per_rank\":%.3f,\"cast_time_ms\":%.3f,\"skill_type\":%u",
                     (w > 1 ? "," : ""), i, name_sanitized, d->base_cooldown_ms,
                     d->cooldown_reduction_ms_per_rank, d->cast_time_ms, (unsigned) d->skill_type);
        if (n < 0 || w + n >= out_cap)
            return -1;
        w += n;
        /* Append primary effect + nodes if present for ease of editing */
        if (d->effect_spec_id >= 0)
        {
            n = snprintf(out_buf + w, out_cap - w, ",\"effect_spec_id\":%d", d->effect_spec_id);
            if (n < 0 || w + n >= out_cap)
                return -1;
            w += n;
        }
        if (d->effect_node_count > 0)
        {
            n = snprintf(out_buf + w, out_cap - w, ",\"effect_nodes\":[");
            if (n < 0 || w + n >= out_cap)
                return -1;
            w += n;
            for (int ei = 0; ei < d->effect_node_count; ++ei)
            {
                const struct RogueSkillEffectNode* en = &d->effect_nodes[ei];
                n = snprintf(out_buf + w, out_cap - w,
                             "%s{\"effect_spec_id\":%d,\"delay_ms\":%.3f,\"duration_ms\":%.3f,"
                             "\"repeat_count\":%d,\"repeat_interval_ms\":%.3f,\"hp_below_pct\":%u}",
                             (ei ? "," : ""), en->effect_spec_id, en->delay_ms, en->duration_ms,
                             en->repeat_count, en->repeat_interval_ms,
                             (unsigned) en->require_player_health_below_pct);
                if (n < 0 || w + n >= out_cap)
                    return -1;
                w += n;
            }
            if (w + 1 >= out_cap)
                return -1;
            out_buf[w++] = ']';
        }
        if (has_coeff)
        {
            n = snprintf(out_buf + w, out_cap - w,
                         ",\"coeff\":{\"base\":%.3f,\"per_rank\":%.3f,\"str\":%.3f,"
                         "\"int\":%.3f,\"dex\":%.3f,\"cap\":%.3f,\"soft\":%.3f}}",
                         cp.base_scalar, cp.per_rank_scalar, cp.str_pct_per10, cp.int_pct_per10,
                         cp.dex_pct_per10, cp.stat_cap_pct, cp.stat_softness);
        }
        else
        {
            n = snprintf(out_buf + w, out_cap - w, "}");
        }
        if (n < 0 || w + n >= out_cap)
            return -1;
        w += n;
    }
    if (w + 1 >= out_cap)
        return -1;
    out_buf[w++] = ']';
    out_buf[w] = '\0';
    // Debug: dump to a file in CWD for investigation during tests
    {
        FILE* f = NULL;
        errno_t fe = fopen_s(&f, "skill_overrides_dbg.json", "w");
        if (fe == 0 && f)
        {
            fwrite(out_buf, 1, (size_t) w, f);
            fclose(f);
        }
    }
    return w;
}

static const char* sd_ws(const char* s)
{
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')
        ++s;
    return s;
}
static const char* sd_str(const char* s, char* out, int cap)
{
    s = sd_ws(s);
    if (*s != '"')
        return NULL;
    ++s;
    int i = 0;
    while (*s && *s != '"')
    {
        if (*s == '\\' && s[1])
            ++s;
        if (i + 1 < cap)
            out[i++] = *s;
        ++s;
    }
    if (*s != '"')
        return NULL;
    out[i] = '\0';
    return s + 1;
}
static const char* sd_num(const char* s, double* out)
{
    s = sd_ws(s);
    char* end = NULL;
    double v = strtod(s, &end);
    if (end == s)
        return NULL;
    *out = v;
    return end;
}

int rogue_skill_debug_load_overrides_text(const char* json_text)
{
    if (!json_text)
        return -1;
    const char* s = sd_ws(json_text);
    if (*s != '[')
        return -1;
    ++s;
    int applied = 0;
    char key[64];
    while (1)
    {
        s = sd_ws(s);
        // Be tolerant of extraneous commas between objects
        while (*s == ',')
        {
            ++s;
            s = sd_ws(s);
        }
        if (*s == ']')
        {
            ++s;
            break;
        }
        if (*s != '{')
            return applied;
        ++s;
        int skill_id = -1;
        float base_cd = 0.f, cd_red = 0.f, cast_ms = 0.f;
        int have_base = 0, have_red = 0, have_cast = 0, have_id = 0;
        int skill_type = 0;
        int have_type = 0;
        /* Effect composition locals: parse order-independent */
        int primary_effect_id = -1;
        int have_primary = 0;
        struct RogueSkillEffectNode parsed_nodes[3];
        int parsed_node_count = 0;
        int have_nodes = 0;
        RogueSkillCoeffParams cp;
        int have_cp = 0;
        memset(&cp, 0, sizeof cp);
        cp.base_scalar = 1.0f;
        int done_obj = 0;
        while (!done_obj)
        {
            s = sd_ws(s);
            if (*s == '}')
            {
                ++s;
                break;
            }
            const char* ns = sd_str(s, key, (int) sizeof key);
            if (!ns)
                return applied;
            s = sd_ws(ns);
            if (*s != ':')
                return applied;
            ++s;
            s = sd_ws(s);
            if (strcmp(key, "skill_id") == 0)
            {
                double idv;
                const char* vs = sd_num(s, &idv);
                if (!vs)
                    return applied;
                s = sd_ws(vs);
                skill_id = (int) idv;
                have_id = 1;
                printf("skill_overrides: id=%d\n", skill_id);
            }
            else if (strcmp(key, "base_cooldown_ms") == 0)
            {
                double v;
                const char* vs = sd_num(s, &v);
                if (!vs)
                    return applied;
                s = sd_ws(vs);
                base_cd = (float) v;
                have_base = 1;
                printf("skill_overrides: base=%.3f\n", base_cd);
            }
            else if (strcmp(key, "cd_red_ms_per_rank") == 0)
            {
                double v;
                const char* vs = sd_num(s, &v);
                if (!vs)
                    return applied;
                s = sd_ws(vs);
                cd_red = (float) v;
                have_red = 1;
                printf("skill_overrides: red=%.3f\n", cd_red);
            }
            else if (strcmp(key, "cast_time_ms") == 0)
            {
                double v;
                const char* vs = sd_num(s, &v);
                if (!vs)
                    return applied;
                s = sd_ws(vs);
                cast_ms = (float) v;
                have_cast = 1;
                printf("skill_overrides: cast=%.3f\n", cast_ms);
            }
            else if (strcmp(key, "skill_type") == 0)
            {
                double v;
                const char* vs = sd_num(s, &v);
                if (!vs)
                    return applied;
                s = sd_ws(vs);
                skill_type = (int) v;
                have_type = 1;
                printf("skill_overrides: type=%d\n", skill_type);
            }
            else if (strcmp(key, "effect_spec_id") == 0)
            {
                double v;
                const char* vs = sd_num(s, &v);
                if (!vs)
                    return applied;
                s = sd_ws(vs);
                primary_effect_id = (int) v;
                have_primary = 1;
                printf("skill_overrides: primary_effect=%d\n", primary_effect_id);
            }
            else if (strcmp(key, "effect_nodes") == 0)
            {
                s = sd_ws(s);
                if (*s != '[')
                    return applied;
                ++s;
                /* Parse up to 3 nodes */
                parsed_node_count = 0;
                while (1)
                {
                    s = sd_ws(s);
                    if (*s == ']')
                    {
                        ++s;
                        break;
                    }
                    if (*s != '{')
                        return applied;
                    ++s;
                    struct RogueSkillEffectNode en;
                    memset(&en, 0, sizeof en);
                    en.effect_spec_id = -1;
                    while (1)
                    {
                        s = sd_ws(s);
                        if (*s == '}')
                        {
                            ++s;
                            break;
                        }
                        char k2[32];
                        const char* ns2 = sd_str(s, k2, (int) sizeof k2);
                        if (!ns2)
                            return applied;
                        s = sd_ws(ns2);
                        if (*s != ':')
                            return applied;
                        ++s;
                        double num;
                        const char* vs2 = sd_num(s, &num);
                        if (!vs2)
                            return applied;
                        s = sd_ws(vs2);
                        if (strcmp(k2, "effect_spec_id") == 0)
                            en.effect_spec_id = (int) num;
                        else if (strcmp(k2, "delay_ms") == 0)
                            en.delay_ms = (float) num;
                        else if (strcmp(k2, "duration_ms") == 0)
                            en.duration_ms = (float) num;
                        else if (strcmp(k2, "repeat_count") == 0)
                            en.repeat_count = (int) num;
                        else if (strcmp(k2, "repeat_interval_ms") == 0)
                            en.repeat_interval_ms = (float) num;
                        else if (strcmp(k2, "hp_below_pct") == 0)
                            en.require_player_health_below_pct = (unsigned char) num;
                        s = sd_ws(s);
                        if (*s == ',')
                        {
                            ++s;
                            continue;
                        }
                    }
                    if (parsed_node_count < 3)
                        parsed_nodes[parsed_node_count++] = en;
                    s = sd_ws(s);
                    if (*s == ',')
                    {
                        ++s;
                        continue;
                    }
                }
                have_nodes = 1;
            }
            else if (strcmp(key, "coeff") == 0)
            {
                s = sd_ws(s);
                if (*s != '{')
                    return applied;
                ++s;
                char k2[32];
                int done_c = 0;
                while (!done_c)
                {
                    s = sd_ws(s);
                    if (*s == '}')
                    {
                        ++s;
                        break;
                    }
                    const char* ns2 = sd_str(s, k2, (int) sizeof k2);
                    if (!ns2)
                        return applied;
                    s = sd_ws(ns2);
                    if (*s != ':')
                        return applied;
                    ++s;
                    double num;
                    const char* vs2 = sd_num(s, &num);
                    if (!vs2)
                        return applied;
                    s = sd_ws(vs2);
                    if (strcmp(k2, "base") == 0)
                        cp.base_scalar = (float) num;
                    else if (strcmp(k2, "per_rank") == 0)
                        cp.per_rank_scalar = (float) num;
                    else if (strcmp(k2, "str") == 0)
                        cp.str_pct_per10 = (float) num;
                    else if (strcmp(k2, "int") == 0)
                        cp.int_pct_per10 = (float) num;
                    else if (strcmp(k2, "dex") == 0)
                        cp.dex_pct_per10 = (float) num;
                    else if (strcmp(k2, "cap") == 0)
                        cp.stat_cap_pct = (float) num;
                    else if (strcmp(k2, "soft") == 0)
                        cp.stat_softness = (float) num;
                    // consume comma between coeff fields, if present
                    s = sd_ws(s);
                    if (*s == ',')
                    {
                        ++s;
                        continue;
                    }
                }
                have_cp = 1;
            }
            else if (strcmp(key, "name") == 0)
            {
                // Skip string value for name
                char tmpname[128];
                const char* vs = sd_str(s, tmpname, (int) sizeof tmpname);
                if (!vs)
                    return applied;
                s = sd_ws(vs);
            }
            else
            {
                // Unknown key: try to skip a string or a number value conservatively
                const char* ss = sd_ws(s);
                if (*ss == '"')
                {
                    char tmp[128];
                    const char* vs = sd_str(ss, tmp, (int) sizeof tmp);
                    if (!vs)
                        return applied;
                    s = sd_ws(vs);
                }
                else
                {
                    double dummy;
                    const char* vs = sd_num(ss, &dummy);
                    if (!vs)
                        return applied;
                    s = sd_ws(vs);
                }
            }
            s = sd_ws(s);
            if (*s == ',')
            {
                ++s;
                continue;
            }
        }
        if (have_id && skill_id >= 0 && skill_id < g_app.skill_count)
        {
            if (have_base || have_red || have_cast)
                rogue_skill_debug_set_timing(
                    skill_id, have_base ? base_cd : g_app.skill_defs[skill_id].base_cooldown_ms,
                    have_red ? cd_red : g_app.skill_defs[skill_id].cooldown_reduction_ms_per_rank,
                    have_cast ? cast_ms : g_app.skill_defs[skill_id].cast_time_ms);
            if (have_cp)
                (void) rogue_skill_debug_set_coeff(skill_id, &cp);
            if (have_type)
                (void) rogue_skill_debug_set_type(skill_id, skill_type);
            if (have_primary || have_nodes)
            {
                int prim =
                    have_primary ? primary_effect_id : g_app.skill_defs[skill_id].effect_spec_id;
                const struct RogueSkillEffectNode* n_ptr = NULL;
                int n_cnt = 0;
                if (have_nodes)
                {
                    n_ptr = parsed_nodes;
                    n_cnt = parsed_node_count;
                }
                else
                {
                    n_ptr = g_app.skill_defs[skill_id].effect_nodes;
                    n_cnt = g_app.skill_defs[skill_id].effect_node_count;
                }
                (void) rogue_skill_debug_set_effects(skill_id, prim, n_ptr, n_cnt);
            }
            ++applied;
            // Debug trace for unit tests
            printf("skill_overrides: applied id=%d base=%.3f red=%.3f cast=%.3f coeff=%d type=%d "
                   "prim=%d nodes=%d\n",
                   skill_id, base_cd, cd_red, cast_ms, have_cp, have_type ? skill_type : -1,
                   have_primary ? primary_effect_id : -1, have_nodes ? parsed_node_count : -1);
        }
        s = sd_ws(s);
        if (*s == ',')
        {
            ++s;
            continue;
        }
    }
    return applied;
}

int rogue_skill_debug_create(const char* name, int max_rank, float base_cooldown_ms,
                             float cd_red_ms_per_rank, float cast_time_ms, int is_passive)
{
    if (!name || !name[0])
        return -1;
    if (max_rank <= 0)
        max_rank = 1;
    if (base_cooldown_ms < 0)
        base_cooldown_ms = 0;
    if (cd_red_ms_per_rank < -60000.0f)
        cd_red_ms_per_rank = -60000.0f;
    if (cast_time_ms < 0)
        cast_time_ms = 0;
    /* Check duplicate by name (linear scan; debug-only use) */
    for (int i = 0; i < g_app.skill_count; ++i)
    {
        const RogueSkillDef* d = rogue_skill_get_def(i);
        if (d && d->name && strcmp(d->name, name) == 0)
            return -2;
    }
    RogueSkillDef def;
    memset(&def, 0, sizeof def);
    def.name = name;
    def.icon = NULL;
    def.max_rank = max_rank;
    def.base_cooldown_ms = base_cooldown_ms;
    def.cooldown_reduction_ms_per_rank = cd_red_ms_per_rank;
    def.cast_time_ms = cast_time_ms;
    def.is_passive = is_passive ? 1 : 0;
    def.action_point_cost = 0;
    def.resource_cost_mana = 0;
    def.max_charges = 0;
    def.charge_recharge_ms = 0;
    def.input_buffer_ms = 0;
    def.min_weave_ms = 0;
    def.early_cancel_min_pct = 0;
    def.cast_type = (cast_time_ms > 0.0f) ? 1 : 0; /* 0 instant, 1 cast */
    def.combo_builder = 0;
    def.combo_spender = 0;
    def.effect_spec_id = -1;
    def.haste_mode_flags = 0;
    /* Costs/refunds default 0 */
    def.ap_cost_pct_max = 0;
    def.ap_cost_per_rank = 0;
    def.ap_cost_surcharge_amount = 0;
    def.ap_cost_surcharge_threshold = 0;
    def.mana_cost_pct_max = 0;
    def.mana_cost_per_rank = 0;
    def.mana_cost_surcharge_amount = 0;
    def.mana_cost_surcharge_threshold = 0;
    def.refund_on_miss_pct = 0;
    def.refund_on_resist_pct = 0;
    def.refund_on_cancel_pct = 0;
    int idx = rogue_skill_register(&def);
    return idx;
}

int rogue_skill_debug_save_overrides(const char* path)
{
    if (!path)
        return -1;
    char* buf = (char*) malloc(64 * 1024);
    if (!buf)
        return -1;
    int n = rogue_skill_debug_export_overrides_json(buf, 64 * 1024);
    if (n < 0)
    {
        free(buf);
        return -2;
    }
    char err[256];
    int rc = json_io_write_atomic(path, buf, (size_t) n, err, (int) sizeof err);
    free(buf);
    return rc;
}

int rogue_skill_debug_load_overrides_file(const char* path)
{
    if (!path)
        return -1;
    char* data = NULL;
    size_t len = 0;
    char err[256];
    if (json_io_read_file(path, &data, &len, err, (int) sizeof err) != 0)
        return -2;
    /* Ensure null-terminated */
    int applied = rogue_skill_debug_load_overrides_text(data);
    free(data);
    return applied;
}

/* --- Auto-reload support --------------------------------------------------------------- */

int rogue_skill_debug_autoreload_tick(const char* path)
{
    const char* use_path = path ? path : "build/skills_overrides.json";
    static unsigned long long s_last_mtime = 0ULL;
    unsigned long long cur = 0ULL;
    char err[128];
    if (json_io_get_mtime_ms(use_path, &cur, err, (int) sizeof err) != 0)
    {
        return 0; /* treat missing file as no-op */
    }
    if (cur == 0ULL || cur == s_last_mtime)
        return 0; /* unchanged */
    s_last_mtime = cur;
    int applied = rogue_skill_debug_load_overrides_file(use_path);
    return applied;
}

int rogue_skills_base_autoreload_tick(const char* path)
{
    const char* use_path = path ? path : "assets/skills_uhf87f.json";
    static unsigned long long s_last_mtime = 0ULL;
    unsigned long long cur = 0ULL;
    char err[128];
    if (json_io_get_mtime_ms(use_path, &cur, err, (int) sizeof err) != 0)
    {
        return 0; /* missing/unreadable: no-op */
    }
    if (cur == 0ULL || cur == s_last_mtime)
        return 0; /* unchanged */
    s_last_mtime = cur;
    /* Full registry reload */
    int loaded = rogue_skills_reload_from_cfg(use_path);
    return loaded;
}

int rogue_skill_debug_get_meta(int id, int* out_max_rank, int* out_is_passive)
{
    const RogueSkillDef* d = rogue_skill_get_def(id);
    if (!d)
        return -1;
    if (out_max_rank)
        *out_max_rank = d->max_rank > 0 ? d->max_rank : 1;
    if (out_is_passive)
        *out_is_passive = d->is_passive ? 1 : 0;
    return 0;
}

int rogue_skill_debug_validate(char* err, int err_cap)
{
    return rogue_skills_validate_all(err, err_cap);
}
