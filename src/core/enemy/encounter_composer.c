#include "core/enemy/encounter_composer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file encounter_composer.c
 * @brief Encounter template loader and composition utilities.
 *
 * This module provides parsing of simple encounter template files into
 * in-memory templates and a lightweight composer that, given a template id
 * and a seed, produces an encounter composition (list of units, elites,
 * supports and boss presence). The parsing supports a line-oriented
 * key=value format; blank lines separate templates.
 */

/** Storage for parsed encounter templates. */
static RogueEncounterTemplate g_templates[ROGUE_MAX_ENCOUNTER_TEMPLATES];

/** Number of templates currently loaded into @c g_templates. */
static int g_template_count = 0;

/**
 * @brief Advance RNG state using xorshift32.
 *
 * This is a tiny local PRNG used for composing encounters deterministically
 * from a provided seed. The function updates the provided state in-place
 * and returns the produced value.
 */
static unsigned int rng_next(unsigned int* s)
{
    unsigned int x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

/**
 * @brief Return a random integer in [0, hi) using the provided state.
 *
 * If hi <= 0 the function returns 0.
 */
static int rng_range(unsigned int* s, int hi)
{
    if (hi <= 0)
        return 0;
    return (int) (rng_next(s) % (unsigned) hi);
}

/** @brief Return a pseudo-random float in [0.0, 1.0). */
static float rng_float(unsigned int* s) { return (rng_next(s) & 0xFFFFFF) / (float) 0x1000000; }

/**
 * @brief Parse a textual type token into the encounter type enum.
 *
 * Recognized tokens: "swarm", "mixed", "champion_pack", "boss_room".
 * Unknown tokens default to ROGUE_ENCOUNTER_SWARM.
 */
static int parse_type(const char* v)
{
    if (strcmp(v, "swarm") == 0)
        return ROGUE_ENCOUNTER_SWARM;
    if (strcmp(v, "mixed") == 0)
        return ROGUE_ENCOUNTER_MIXED;
    if (strcmp(v, "champion_pack") == 0)
        return ROGUE_ENCOUNTER_CHAMPION_PACK;
    if (strcmp(v, "boss_room") == 0)
        return ROGUE_ENCOUNTER_BOSS_ROOM;
    return ROGUE_ENCOUNTER_SWARM;
}

/**
 * @brief Load encounter templates from a key=value formatted file.
 *
 * The loader resets the internal template list and then parses the file.
 * Each blank line separates templates. Supported keys include id, name,
 * type, min, max, boss, support_min, support_max, elite_spacing and
 * elite_chance. Default values for elite spacing (3) and chance (0.15)
 * are applied when a new template is started.
 *
 * @param path Path to the template file.
 * @return Number of templates loaded, or negative on error.
 */
int rogue_encounters_load_file(const char* path)
{
    FILE* f = NULL;
    g_template_count = 0;
#if defined(_MSC_VER)
    if (fopen_s(&f, path, "rb") != 0)
        return -1;
#else
    f = fopen(path, "rb");
    if (!f)
        return -1;
#endif
    char line[256];
    RogueEncounterTemplate cur;
    memset(&cur, 0, sizeof cur);
    cur.elite_spacing = 3;
    cur.elite_chance = 0.15f;
    while (fgets(line, sizeof line, f))
    {
        int blank = 1;
        for (char* c = line; *c; ++c)
        {
            if (!isspace((unsigned char) *c))
            {
                blank = 0;
                break;
            }
        }
        if (blank)
        {
            if (cur.name[0])
            {
                if (g_template_count < ROGUE_MAX_ENCOUNTER_TEMPLATES)
                    g_templates[g_template_count++] = cur;
                memset(&cur, 0, sizeof cur);
                cur.elite_spacing = 3;
                cur.elite_chance = 0.15f;
            }
            continue;
        }
        char* eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        char* key = line;
        char* val = eq + 1;
        while (*val && isspace((unsigned char) *val))
            val++;
        char* end = val + strlen(val);
        while (end > val && (end[-1] == '\n' || end[-1] == '\r'))
            *--end = '\0';
        if (strcmp(key, "id") == 0)
            cur.id = atoi(val);
        else if (strcmp(key, "name") == 0)
        {
#if defined(_MSC_VER)
            strncpy_s(cur.name, sizeof cur.name, val, _TRUNCATE);
#else
            strncpy(cur.name, val, sizeof cur.name - 1);
            cur.name[sizeof cur.name - 1] = '\0';
#endif
        }
        else if (strcmp(key, "type") == 0)
            cur.type = parse_type(val);
        else if (strcmp(key, "min") == 0)
            cur.min_count = atoi(val);
        else if (strcmp(key, "max") == 0)
            cur.max_count = atoi(val);
        else if (strcmp(key, "boss") == 0)
            cur.boss = atoi(val);
        else if (strcmp(key, "support_min") == 0)
            cur.support_min = atoi(val);
        else if (strcmp(key, "support_max") == 0)
            cur.support_max = atoi(val);
        else if (strcmp(key, "elite_spacing") == 0)
            cur.elite_spacing = atoi(val);
        else if (strcmp(key, "elite_chance") == 0)
            cur.elite_chance = (float) atof(val);
    }
    if (cur.name[0] && g_template_count < ROGUE_MAX_ENCOUNTER_TEMPLATES)
        g_templates[g_template_count++] = cur;
    fclose(f);
    return g_template_count;
}

/** @brief Return number of loaded encounter templates. */
int rogue_encounter_template_count(void) { return g_template_count; }

/**
 * @brief Get a pointer to a template by index.
 *
 * @param index Zero-based template index.
 * @return Pointer to template or NULL if out of range.
 */
const RogueEncounterTemplate* rogue_encounter_template_at(int index)
{
    if (index < 0 || index >= g_template_count)
        return NULL;
    return &g_templates[index];
}

/**
 * @brief Find a template by its id field.
 *
 * @param id Template id to look up.
 * @return Pointer to template or NULL if not found.
 */
const RogueEncounterTemplate* rogue_encounter_template_by_id(int id)
{
    for (int i = 0; i < g_template_count; i++)
        if (g_templates[i].id == id)
            return &g_templates[i];
    return NULL;
}

/**
 * @brief Compose an encounter from a template id and seed.
 *
 * The composition fills the provided @c out structure with units determined
 * from the template: baseline enemy placeholders, elite selection based on
 * elite spacing and chance, and optional support adds for boss templates.
 *
 * Parameters player_level and biome_id are currently unused but accepted
 * for future expansion; seed drives deterministic randomness.
 *
 * Error codes:
 *  - -1 : invalid output pointer
 *  - -2 : template id not found
 *
 * @param template_id Template id to use for composition.
 * @param player_level Player level (currently unused).
 * @param difficulty_rating Difficulty rating used as baseline unit level.
 * @param biome_id Biome id (currently unused).
 * @param seed Deterministic seed for RNG; if zero a small default is used.
 * @param out Output composition buffer to populate.
 * @return 0 on success or negative error code.
 */
int rogue_encounter_compose(int template_id, int player_level, int difficulty_rating, int biome_id,
                            unsigned int seed, RogueEncounterComposition* out)
{
    (void) player_level;
    (void) biome_id;
    if (!out)
        return -1;
    memset(out, 0, sizeof *out);
    const RogueEncounterTemplate* t = rogue_encounter_template_by_id(template_id);
    if (!t)
        return -2;
    out->template_id = template_id;
    unsigned int state = seed ? seed : 0xA53u;
    int span = (t->max_count > t->min_count) ? (t->max_count - t->min_count + 1) : 1;
    int count = t->min_count + rng_range(&state, span);
    if (count < t->min_count)
        count = t->min_count;
    if (count > t->max_count)
        count = t->max_count;
    int next_elite_slot = t->elite_spacing > 0 ? t->elite_spacing : 3;
    for (int i = 0; i < count && i < 64; i++)
    {
        RogueEncounterUnit* u = &out->units[out->unit_count];
        u->enemy_type_id = 0;         /* placeholder baseline enemy type */
        u->level = difficulty_rating; /* baseline: align level to difficulty */
        u->is_elite = 0;
        if (t->boss && i == 0)
        {
            u->is_elite = 1;
            out->boss_present = 1;
        }
        else
        {
            if (i == next_elite_slot)
            {
                if (rng_float(&state) < t->elite_chance)
                {
                    u->is_elite = 1;
                    out->elite_count++;
                    next_elite_slot = i + t->elite_spacing;
                }
                else
                {
                    next_elite_slot = i + 1;
                }
            }
        }
        out->unit_count++;
    }
    /* Support adds */
    if (t->boss && t->support_max > 0)
    {
        int sup_span =
            (t->support_max > t->support_min) ? (t->support_max - t->support_min + 1) : 1;
        int sup = t->support_min + rng_range(&state, sup_span);
        for (int s = 0; s < sup && out->unit_count < 64; s++)
        {
            RogueEncounterUnit* u = &out->units[out->unit_count++];
            u->enemy_type_id = 0;
            u->level = difficulty_rating;
            u->is_elite = 0;
            out->support_count++;
        }
    }
    return 0;
}
