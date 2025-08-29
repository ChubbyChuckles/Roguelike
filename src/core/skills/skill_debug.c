#include "skill_debug.h"
#include "../../content/json_io.h"
#include "../app/app_state.h"
#include "skills.h"
#include "skills_coeffs.h"
#include "skills_internal.h"
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
                     "\"cd_red_ms_per_rank\":%.3f,\"cast_time_ms\":%.3f",
                     (w > 1 ? "," : ""), i, name_sanitized, d->base_cooldown_ms,
                     d->cooldown_reduction_ms_per_rank, d->cast_time_ms);
        if (n < 0 || w + n >= out_cap)
            return -1;
        w += n;
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
            ++applied;
            // Debug trace for unit tests
            printf("skill_overrides: applied id=%d base=%.3f red=%.3f cast=%.3f coeff=%d\n",
                   skill_id, base_cd, cd_red, cast_ms, have_cp);
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
