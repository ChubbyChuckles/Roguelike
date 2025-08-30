/**
 * @file effect_spec_parser.h
 * @brief Key-value based effect specification parser for game effects,
 *        buffs, and damage-over-time mechanics with child effect support.
 */

#include "effect_spec_parser.h"
#include "../game/buffs.h"
#include "../game/combat_attacks.h" /* RogueDamageType enum & ROGUE_DMG_* */
#include "../util/kv_parser.h"
#include "effect_spec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Parses a string to a buff stack rule enum value.
 *
 * @param s The string to parse.
 * @return The corresponding ROGUE_BUFF_STACK_* value, or -1 if not found.
 */
static int parse_stack_rule(const char* s)
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

/**
 * @brief Parses a string to an effect kind enum value.
 *
 * @param s The string to parse.
 * @return The corresponding ROGUE_EFFECT_* value, or -1 if not found.
 */
static int parse_kind(const char* s)
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

/**
 * @brief Parses a string to a damage type enum value.
 *
 * @param s The string to parse.
 * @return The corresponding ROGUE_DMG_* value, or -1 if not found.
 */
static int parse_damage_type(const char* s)
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

/**
 * @brief Parses a string to a buff type enum value.
 *
 * @param s The string to parse.
 * @return The corresponding ROGUE_BUFF_* value, or -1 if not found.
 */
static int parse_buff_type(const char* s)
{
    if (!s)
        return -1;
    if (strcmp(s, "POWER_STRIKE") == 0)
        return ROGUE_BUFF_POWER_STRIKE;
    if (strcmp(s, "STAT_STRENGTH") == 0)
        return ROGUE_BUFF_STAT_STRENGTH;
    if (strcmp(s, "STRENGTH") == 0)
        return ROGUE_BUFF_STAT_STRENGTH;
    return -1;
}

/**
 * @brief Accumulator structure for tracking effect specification parsing state.
 */
typedef struct Accum
{
    int present;          /**< Whether this effect index has been encountered */
    RogueEffectSpec spec; /**< The effect specification being built */
    int has_stack_rule;   /**< Whether a stack rule has been explicitly set */
} Accum;

/**
 * @brief Parses effect specifications from key-value text format.
 *
 * Processes text in key-value format where keys follow the pattern "effect.X.field"
 * where X is the effect index and field is the property name. Supports child effects
 * with delays and various effect properties.
 *
 * @param text The key-value text to parse.
 * @param out_ids Array to store registered effect IDs (can be NULL).
 * @param max_ids Maximum number of effect IDs to store.
 * @param err Buffer to store error messages (can be NULL).
 * @param errcap Capacity of the error buffer.
 * @return Number of effects successfully parsed and registered, or -1 on error.
 */
int rogue_effects_parse_text(const char* text, int* out_ids, int max_ids, char* err, int errcap)
{
    if (errcap > 0 && err)
        err[0] = '\0';
    if (!text)
    {
        if (err && errcap)
            snprintf(err, errcap, "null text");
        return -1;
    }
    RogueKVFile kv = {0};
    kv.data = text;
    kv.length = (int) strlen(text);
    Accum acc[64];
    memset(acc, 0, sizeof acc);
    int cursor = 0;
    RogueKVEntry e;
    RogueKVError ke;
    int count = 0;
    while (rogue_kv_next(&kv, &cursor, &e, &ke))
    {
        if (!e.key)
            continue;
        if (strncmp(e.key, "effect.", 7) != 0)
            continue;
        const char* rest = e.key + 7; /* <index>.field */
        char* dot = strchr(rest, '.');
        if (!dot)
            continue;
        int idx = atoi(rest);
        if (idx < 0 || idx >= (int) (sizeof acc / sizeof acc[0]))
            continue;
        const char* field = dot + 1;
        Accum* a = &acc[idx];
        a->present = 1;
        RogueEffectSpec* s = &a->spec;
        if (strcmp(field, "kind") == 0)
        {
            int k = parse_kind(e.value);
            if (k >= 0)
                s->kind = (unsigned char) k;
        }
        else if (strcmp(field, "debuff") == 0)
        {
            s->debuff = (unsigned char) atoi(e.value);
        }
        else if (strcmp(field, "buff_type") == 0)
        {
            int bt = parse_buff_type(e.value);
            if (bt >= 0)
                s->buff_type = (unsigned short) bt;
        }
        else if (strcmp(field, "magnitude") == 0)
        {
            s->magnitude = atoi(e.value);
        }
        else if (strcmp(field, "duration_ms") == 0)
        {
            s->duration_ms = (float) atof(e.value);
        }
        else if (strcmp(field, "damage_type") == 0)
        {
            int dt = parse_damage_type(e.value);
            if (dt >= 0)
                s->damage_type = (unsigned char) dt;
        }
        else if (strcmp(field, "crit_mode") == 0)
        {
            s->crit_mode = (unsigned char) atoi(e.value);
        }
        else if (strcmp(field, "crit_chance_pct") == 0)
        {
            s->crit_chance_pct = (unsigned char) atoi(e.value);
        }
        else if (strcmp(field, "stack_rule") == 0)
        {
            int r = parse_stack_rule(e.value);
            if (r >= 0)
            {
                s->stack_rule = (unsigned char) r;
                a->has_stack_rule = 1;
            }
        }
        else if (strcmp(field, "snapshot") == 0)
        {
            s->snapshot = (unsigned char) atoi(e.value);
        }
        else if (strcmp(field, "pulse_period_ms") == 0)
        {
            s->pulse_period_ms = (float) atof(e.value);
        }
        else if (strcmp(field, "aura_radius") == 0)
        {
            s->aura_radius = (float) atof(e.value);
        }
        else if (strcmp(field, "aura_group_mask") == 0)
        {
            s->aura_group_mask = (unsigned int) strtoul(e.value, NULL, 10);
        }
        else if (strcmp(field, "scale_by_buff_type") == 0)
        {
            int bt = parse_buff_type(e.value);
            if (bt >= 0)
                s->scale_by_buff_type = (unsigned short) bt;
        }
        else if (strcmp(field, "scale_pct_per_point") == 0)
        {
            s->scale_pct_per_point = atoi(e.value);
        }
        else if (strcmp(field, "snapshot_scale") == 0)
        {
            s->snapshot_scale = (unsigned char) atoi(e.value);
        }
        else if (strcmp(field, "require_buff_type") == 0)
        {
            int bt = parse_buff_type(e.value);
            if (bt >= 0)
                s->require_buff_type = (unsigned short) bt;
        }
        else if (strcmp(field, "require_buff_min") == 0)
        {
            s->require_buff_min = atoi(e.value);
        }
        else if (strncmp(field, "child", 5) == 0)
        {
            /* childN.id or childN.delay_ms */
            int n = atoi(field + 5);
            if (n >= 0 && n < 4)
            {
                const char* sub = strstr(field, ".id");
                if (sub)
                {
                    int id = atoi(e.value);
                    s->children[n].child_effect_id = id;
                    if (s->child_count < n + 1)
                        s->child_count = (unsigned char) (n + 1);
                }
                else if ((sub = strstr(field, ".delay_ms")) != NULL)
                {
                    s->children[n].delay_ms = (float) atof(e.value);
                    if (s->child_count < n + 1)
                        s->child_count = (unsigned char) (n + 1);
                }
            }
        }
        else
        {
            /* ignore unknown keys */
        }
    }
    /* Register in index order. Default stack rule to ADD if unspecified. */
    for (int i = 0; i < (int) (sizeof acc / sizeof acc[0]); ++i)
    {
        if (!acc[i].present)
            continue;
        RogueEffectSpec s = acc[i].spec;
        if (s.kind == 0)
            s.kind = ROGUE_EFFECT_STAT_BUFF;
        if (!acc[i].has_stack_rule)
            s.stack_rule = ROGUE_BUFF_STACK_ADD; /* preserve explicit UNIQUE=0 */
        if (s.require_buff_type == 0)
            s.require_buff_type = (unsigned short) 0xFFFFu; /* sentinel for none */
        if (s.scale_by_buff_type == 0)
            s.scale_by_buff_type = (unsigned short) 0xFFFFu; /* sentinel for none */
        int id = rogue_effect_register(&s);
        if (id >= 0 && out_ids && count < max_ids)
            out_ids[count] = id;
        if (id >= 0)
            count++;
    }
    return count;
}

/**
 * @brief Parses effect specifications from a key-value file.
 *
 * Loads and parses a file containing effect specifications in key-value format,
 * then registers the effects with the effect system.
 *
 * @param path Path to the key-value file to parse.
 * @param out_ids Array to store registered effect IDs (can be NULL).
 * @param max_ids Maximum number of effect IDs to store.
 * @param err Buffer to store error messages (can be NULL).
 * @param errcap Capacity of the error buffer.
 * @return Number of effects successfully parsed and registered, or -1 on error.
 */
int rogue_effects_parse_file(const char* path, int* out_ids, int max_ids, char* err, int errcap)
{
    if (errcap > 0 && err)
        err[0] = '\0';
    RogueKVFile kv;
    if (!rogue_kv_load_file(path, &kv))
    {
        if (err && errcap)
            snprintf(err, errcap, "failed to read %s", path ? path : "(null)");
        return -1;
    }
    int n = rogue_effects_parse_text(kv.data, out_ids, max_ids, err, errcap);
    rogue_kv_free(&kv);
    return n;
}
