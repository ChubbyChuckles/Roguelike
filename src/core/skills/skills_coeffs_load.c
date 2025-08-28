#include "skills_coeffs_load.h"
#include "../../util/log.h"
#include "skills.h"
#include "skills_coeffs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal JSON helpers copied from skills_registry.c style */
static const char* sc_skip_ws(const char* s)
{
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')
        ++s;
    return s;
}
static const char* sc_parse_string(const char* s, char* out, int out_sz)
{
    s = sc_skip_ws(s);
    if (*s != '"')
        return NULL;
    ++s;
    int i = 0;
    while (*s && *s != '"')
    {
        if (*s == '\\' && s[1])
            ++s;
        if (i + 1 < out_sz)
            out[i++] = *s;
        ++s;
    }
    if (*s != '"')
        return NULL;
    out[i] = '\0';
    return s + 1;
}
static const char* sc_parse_number(const char* s, double* out)
{
    s = sc_skip_ws(s);
    char* end = NULL;
    double v = strtod(s, &end);
    if (end == s)
        return NULL;
    *out = v;
    return end;
}

static int parse_json_array(const char* buf)
{
    const char* s = sc_skip_ws(buf);
    if (*s != '[')
        return -1;
    ++s;
    int count = 0;
    char key[64];
    char sval[64];
    while (1)
    {
        s = sc_skip_ws(s);
        if (*s == ']')
        {
            ++s;
            break;
        }
        if (*s != '{')
            return count;
        ++s;
        int skill_id = -1;
        RogueSkillCoeffParams p;
        memset(&p, 0, sizeof p);
        p.base_scalar = 1.0f;
        int seen_skill_id = 0;
        int done_obj = 0;
        while (!done_obj)
        {
            s = sc_skip_ws(s);
            if (*s == '}')
            {
                ++s;
                break;
            }
            const char* ns = sc_parse_string(s, key, sizeof key);
            if (!ns)
                return count;
            s = sc_skip_ws(ns);
            if (*s != ':')
                return count;
            ++s;
            s = sc_skip_ws(s);
            if (*s == '"')
            {
                const char* vs = sc_parse_string(s, sval, sizeof sval);
                if (!vs)
                    return count;
                s = sc_skip_ws(vs);
                /* no string fields */
            }
            else
            {
                double num;
                const char* vs = sc_parse_number(s, &num);
                if (!vs)
                    return count;
                s = sc_skip_ws(vs);
                if (strcmp(key, "skill_id") == 0)
                {
                    skill_id = (int) num;
                    seen_skill_id = 1;
                }
                else if (strcmp(key, "base_scalar") == 0)
                    p.base_scalar = (float) num;
                else if (strcmp(key, "per_rank_scalar") == 0)
                    p.per_rank_scalar = (float) num;
                else if (strcmp(key, "str_pct_per10") == 0)
                    p.str_pct_per10 = (float) num;
                else if (strcmp(key, "int_pct_per10") == 0)
                    p.int_pct_per10 = (float) num;
                else if (strcmp(key, "dex_pct_per10") == 0)
                    p.dex_pct_per10 = (float) num;
                else if (strcmp(key, "stat_cap_pct") == 0)
                    p.stat_cap_pct = (float) num;
                else if (strcmp(key, "stat_softness") == 0)
                    p.stat_softness = (float) num;
            }
            s = sc_skip_ws(s);
            if (*s == ',')
            {
                ++s;
                continue;
            }
        }
        /* Reject entries missing skill_id or negative id. */
        if (seen_skill_id && skill_id >= 0)
        {
            if (rogue_skill_coeff_register(skill_id, &p) == 0)
                ++count;
        }
        else
        {
            /* Invalid object -> treat as hard error for Phase 10.5 */
            return -1;
        }
        s = sc_skip_ws(s);
        if (*s == ',')
        {
            ++s;
            continue;
        }
    }
    return count;
}

int rogue_skill_coeffs_parse_json_text(const char* json_text)
{
    if (!json_text)
        return -1;
    return parse_json_array(json_text);
}

static int load_csv(FILE* f)
{
    char line[512];
    int count = 0;
    while (fgets(line, sizeof line, f))
    {
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r')
            continue;
        /* token order: skill_id,base,per_rank,str,int,dex,cap,softness */
        char* tok;
        char* ctx = NULL;
        int col = 0;
        RogueSkillCoeffParams p;
        memset(&p, 0, sizeof p);
        p.base_scalar = 1.0f;
        int skill_id = -1;
        for (tok = strtok_s(line, ",", &ctx); tok; tok = strtok_s(NULL, ",", &ctx))
        {
            /* Trim */
            while (*tok == ' ')
                ++tok;
            if (col == 0)
                skill_id = atoi(tok);
            else if (col == 1)
                p.base_scalar = (float) atof(tok);
            else if (col == 2)
                p.per_rank_scalar = (float) atof(tok);
            else if (col == 3)
                p.str_pct_per10 = (float) atof(tok);
            else if (col == 4)
                p.int_pct_per10 = (float) atof(tok);
            else if (col == 5)
                p.dex_pct_per10 = (float) atof(tok);
            else if (col == 6)
                p.stat_cap_pct = (float) atof(tok);
            else if (col == 7)
                p.stat_softness = (float) atof(tok);
            ++col;
        }
        if (skill_id >= 0)
        {
            if (rogue_skill_coeff_register(skill_id, &p) == 0)
                ++count;
        }
    }
    return count;
}

int rogue_skill_coeffs_load_from_cfg(const char* path)
{
    if (!path)
        return -1;
    ROGUE_LOG_INFO("Loading skill coeffs: %s", path);
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0)
        f = NULL;
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        ROGUE_LOG_ERROR("Failed to open coeffs file: %s", path);
        return -1;
    }
    int result = -1;
    if (strstr(path, ".json") != NULL)
    {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        if (sz < 0)
        {
            fclose(f);
            return -1;
        }
        fseek(f, 0, SEEK_SET);
        char* buf = (char*) malloc((size_t) sz + 1);
        if (!buf)
        {
            fclose(f);
            return -1;
        }
        size_t rd = fread(buf, 1, (size_t) sz, f);
        buf[rd] = '\0';
        fclose(f);
        result = parse_json_array(buf);
        free(buf);
    }
    else /* assume CSV */
    {
        result = load_csv(f);
        fclose(f);
    }
    return result;
}
