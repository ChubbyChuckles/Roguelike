#include "effect_spec_load.h"
#include "../game/buffs.h"
#include "../game/combat_attacks.h"
#include "../util/kv_parser.h"
#include "effect_spec.h"
#include "effect_spec_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Tiny JSON helpers (array of flat objects) */
static const char* js_skip_ws(const char* s)
{
    while (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t')
        ++s;
    return s;
}
static const char* js_parse_string(const char* s, char* out, int cap)
{
    s = js_skip_ws(s);
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
static const char* js_parse_number(const char* s, double* out)
{
    s = js_skip_ws(s);
    char* end = NULL;
    double v = strtod(s, &end);
    if (end == s)
        return NULL;
    *out = v;
    return end;
}

/* String enums mapping copied from effect_spec_parser.c */
static int map_stack_rule(const char* s)
{
    if (!s)
        return -1;
    if (strcmp(s, "UNIQUE") == 0)
        return ROGUE_BUFF_STACK_UNIQUE;
    if (strcmp(s, "REFRESH") == 0)
        return ROGUE_BUFF_STACK_REFRESH;
    if (strcmp(s, "EXTEND") == 0)
        return ROGUE_BUFF_STACK_EXTEND;
    if (strcmp(s, "ADD") == 0)
        return ROGUE_BUFF_STACK_ADD;
    if (strcmp(s, "MULTIPLY") == 0)
        return ROGUE_BUFF_STACK_MULTIPLY;
    if (strcmp(s, "REPLACE_IF_STRONGER") == 0)
        return ROGUE_BUFF_STACK_REPLACE_IF_STRONGER;
    return -1;
}
static int map_kind(const char* s)
{
    if (!s)
        return -1;
    if (strcmp(s, "STAT_BUFF") == 0)
        return ROGUE_EFFECT_STAT_BUFF;
    if (strcmp(s, "DOT") == 0)
        return ROGUE_EFFECT_DOT;
    if (strcmp(s, "AURA") == 0)
        return ROGUE_EFFECT_AURA;
    return -1;
}
static int map_damage_type(const char* s)
{
    if (!s)
        return -1;
    if (strcmp(s, "PHYSICAL") == 0)
        return ROGUE_DMG_PHYSICAL;
    if (strcmp(s, "FIRE") == 0)
        return ROGUE_DMG_FIRE;
    if (strcmp(s, "FROST") == 0)
        return ROGUE_DMG_FROST;
    if (strcmp(s, "ARCANE") == 0)
        return ROGUE_DMG_ARCANE;
    if (strcmp(s, "POISON") == 0)
        return ROGUE_DMG_POISON;
    if (strcmp(s, "BLEED") == 0)
        return ROGUE_DMG_BLEED;
    if (strcmp(s, "TRUE") == 0)
        return ROGUE_DMG_TRUE;
    return -1;
}
static int map_buff_type(const char* s)
{
    if (!s)
        return -1;
    if (strcmp(s, "POWER_STRIKE") == 0)
        return ROGUE_BUFF_POWER_STRIKE;
    if (strcmp(s, "STAT_STRENGTH") == 0 || strcmp(s, "STRENGTH") == 0)
        return ROGUE_BUFF_STAT_STRENGTH;
    return -1;
}

static int parse_effects_array(const char* buf, int* out_ids, int max_ids)
{
    const char* s = js_skip_ws(buf);
    if (*s != '[')
        return -1;
    ++s;
    int count = 0;
    while (1)
    {
        s = js_skip_ws(s);
        if (*s == ']')
        {
            ++s;
            break;
        }
        if (*s != '{')
            return count; /* tolerate trailing junk */
        ++s;
        RogueEffectSpec spec;
        memset(&spec, 0, sizeof spec);
        int has_stack_rule = 0;
        /* Track whether certain keys were present and whether mapping failed. */
        int present_kind = 0, invalid_kind = 0;
        int present_buff_type = 0, invalid_buff_type = 0;
        int present_stack_rule = 0, invalid_stack_rule = 0;
        int present_damage_type = 0, invalid_damage_type = 0;
        int present_scale_by = 0, invalid_scale_by = 0;
        int present_require_buff = 0, invalid_require_buff = 0;
        while (1)
        {
            s = js_skip_ws(s);
            if (*s == '}')
            {
                ++s;
                break;
            }
            char key[64];
            const char* ns = js_parse_string(s, key, sizeof key);
            if (!ns)
                return count;
            s = js_skip_ws(ns);
            if (*s != ':')
                return count;
            ++s;
            s = js_skip_ws(s);
            if (*s == '"')
            {
                char val[64];
                const char* vs = js_parse_string(s, val, sizeof val);
                if (!vs)
                    return count;
                s = js_skip_ws(vs);
                if (strcmp(key, "kind") == 0)
                {
                    present_kind = 1;
                    int k = map_kind(val);
                    if (k >= 0)
                        spec.kind = (unsigned char) k;
                    else
                        invalid_kind = 1;
                }
                else if (strcmp(key, "stack_rule") == 0)
                {
                    present_stack_rule = 1;
                    int r = map_stack_rule(val);
                    if (r >= 0)
                    {
                        spec.stack_rule = (unsigned char) r;
                        has_stack_rule = 1;
                    }
                    else
                    {
                        invalid_stack_rule = 1;
                    }
                }
                else if (strcmp(key, "buff_type") == 0)
                {
                    present_buff_type = 1;
                    int bt = map_buff_type(val);
                    if (bt >= 0)
                        spec.buff_type = (unsigned short) bt;
                    else
                        invalid_buff_type = 1;
                }
                else if (strcmp(key, "scale_by_buff_type") == 0)
                {
                    present_scale_by = 1;
                    int bt = map_buff_type(val);
                    if (bt >= 0)
                        spec.scale_by_buff_type = (unsigned short) bt;
                    else
                        invalid_scale_by = 1;
                }
                else if (strcmp(key, "require_buff_type") == 0)
                {
                    present_require_buff = 1;
                    int bt = map_buff_type(val);
                    if (bt >= 0)
                        spec.require_buff_type = (unsigned short) bt;
                    else
                        invalid_require_buff = 1;
                }
                else if (strcmp(key, "damage_type") == 0)
                {
                    present_damage_type = 1;
                    int dt = map_damage_type(val);
                    if (dt >= 0)
                        spec.damage_type = (unsigned char) dt;
                    else
                        invalid_damage_type = 1;
                }
            }
            else
            {
                double num;
                const char* vs = js_parse_number(s, &num);
                if (!vs)
                    return count;
                s = js_skip_ws(vs);
                if (strcmp(key, "debuff") == 0)
                    spec.debuff = (unsigned char) num;
                else if (strcmp(key, "magnitude") == 0)
                    spec.magnitude = (int) num;
                else if (strcmp(key, "duration_ms") == 0)
                    spec.duration_ms = (float) num;
                else if (strcmp(key, "snapshot") == 0)
                    spec.snapshot = (unsigned char) num;
                else if (strcmp(key, "scale_pct_per_point") == 0)
                    spec.scale_pct_per_point = (int) num;
                else if (strcmp(key, "snapshot_scale") == 0)
                    spec.snapshot_scale = (unsigned char) num;
                else if (strcmp(key, "require_buff_min") == 0)
                    spec.require_buff_min = (int) num;
                else if (strcmp(key, "pulse_period_ms") == 0)
                    spec.pulse_period_ms = (float) num;
                else if (strcmp(key, "crit_mode") == 0)
                    spec.crit_mode = (unsigned char) num;
                else if (strcmp(key, "crit_chance_pct") == 0)
                    spec.crit_chance_pct = (unsigned char) num;
                else if (strcmp(key, "aura_radius") == 0)
                    spec.aura_radius = (float) num;
                else if (strcmp(key, "aura_group_mask") == 0)
                    spec.aura_group_mask = (unsigned int) num;
            }
            s = js_skip_ws(s);
            if (*s == ',')
            {
                ++s;
                continue;
            }
        }
        if (spec.kind == 0)
            spec.kind = ROGUE_EFFECT_STAT_BUFF;
        if (!has_stack_rule)
            spec.stack_rule = ROGUE_BUFF_STACK_ADD;
        if (spec.require_buff_type == 0)
            spec.require_buff_type = (unsigned short) 0xFFFFu;
        if (spec.scale_by_buff_type == 0)
            spec.scale_by_buff_type = (unsigned short) 0xFFFFu;

        /* Invalid reference rejection (Phase 10.5):
           - If key was present but mapping failed, reject this object.
           - For STAT_BUFF, require a valid buff_type to be provided.
         */
        int reject = 0;
        if (invalid_stack_rule || invalid_damage_type || invalid_scale_by || invalid_require_buff)
            reject = 1;
        /* Only treat kind invalid if key was present with unknown value. */
        if (invalid_kind)
            reject = 1;
        if (spec.kind == ROGUE_EFFECT_STAT_BUFF)
        {
            if (!present_buff_type || invalid_buff_type)
                reject = 1;
        }
        /* For DOT, require valid damage_type if provided (invalid already triggers reject). */
        if (spec.kind == ROGUE_EFFECT_DOT)
        {
            /* Accept default damage_type if not present; but reject if present and invalid.
               Already handled via invalid_damage_type flag. */
            (void) 0;
        }
        if (!reject)
        {
            int id = rogue_effect_register(&spec);
            if (id >= 0)
            {
                if (out_ids && count < max_ids)
                    out_ids[count] = id;
                ++count;
            }
        }
        s = js_skip_ws(s);
        if (*s == ',')
        {
            ++s;
            continue;
        }
    }
    return count;
}

int rogue_effects_load_from_json_text(const char* json_text, int* out_effect_ids, int max_ids)
{
    if (!json_text)
        return -1;
    return parse_effects_array(json_text, out_effect_ids, max_ids);
}

int rogue_effects_load_from_file(const char* path, int* out_effect_ids, int max_ids, char* err,
                                 int errcap)
{
    if (err && errcap)
        err[0] = '\0';
    if (!path)
        return -1;
    FILE* f = NULL;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0)
        f = NULL;
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        if (err && errcap)
            snprintf(err, errcap, "failed to open %s", path);
        return -1;
    }
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
    int n = parse_effects_array(buf, out_effect_ids, max_ids);
    free(buf);
    return n;
}
