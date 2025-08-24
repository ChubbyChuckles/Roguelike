#include "material_registry.h"
#include "../loot/loot_item_defs.h"
#include "../path_utils.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file material_registry.c
 * @brief Material registry: mapping between material ids and item defs.
 *
 * This module maintains a small in-memory registry of materials used by the
 * crafting system. Each material record stores an id, an associated item
 * definition index, tier, category and base value. Loading supports both a
 * CSV-like legacy format and a compact JSON array format.
 */

/**
 * @brief Storage for material definitions.
 *
 * The array is sized by ROGUE_MATERIAL_REGISTRY_CAP to avoid dynamic
 * allocations; callers should use the provided helpers to query and load
 * entries.
 */
static RogueMaterialDef g_materials[ROGUE_MATERIAL_REGISTRY_CAP];

/** @brief Number of valid entries currently in @c g_materials. */
static int g_material_count = 0;

/**
 * @brief Reset the material registry to an empty state.
 *
 * Clears the in-memory count; existing array contents are not zeroed but are
 * considered invalid past the new count.
 */
void rogue_material_registry_reset(void) { g_material_count = 0; }

/**
 * @brief Return the number of loaded material definitions.
 *
 * @return Count of materials currently registered.
 */
int rogue_material_count(void) { return g_material_count; }

/**
 * @brief Get a pointer to a loaded material definition by index.
 *
 * @param idx Zero-based index into the registry.
 * @return Pointer to the definition or NULL if index is out of range.
 */
const RogueMaterialDef* rogue_material_get(int idx)
{
    if (idx < 0 || idx >= g_material_count)
        return NULL;
    return &g_materials[idx];
}

/**
 * @brief Map a category string to the internal category enum.
 *
 * Recognized strings: "ore", "plant", "essence", "component", "currency".
 * The function returns -1 for unknown or NULL input.
 *
 * @param s Category string.
 * @return Internal category constant or -1 on error.
 */
static int category_from_str(const char* s)
{
    if (!s)
        return -1;
    if (strcmp(s, "ore") == 0)
        return ROGUE_MAT_ORE;
    if (strcmp(s, "plant") == 0)
        return ROGUE_MAT_PLANT;
    if (strcmp(s, "essence") == 0)
        return ROGUE_MAT_ESSENCE;
    if (strcmp(s, "component") == 0)
        return ROGUE_MAT_COMPONENT;
    if (strcmp(s, "currency") == 0)
        return ROGUE_MAT_CURRENCY;
    return -1;
}

/**
 * @brief Find a material definition by its string id.
 *
 * @param id Material id string.
 * @return Pointer to the material definition or NULL if not found.
 */
const RogueMaterialDef* rogue_material_find(const char* id)
{
    if (!id)
        return NULL;
    for (int i = 0; i < g_material_count; i++)
    {
        if (strcmp(g_materials[i].id, id) == 0)
            return &g_materials[i];
    }
    return NULL;
}
/**
 * @brief Find the material associated with an item definition index.
 *
 * @param item_def_index Item definition index as returned by the item registry.
 * @return Pointer to the material definition or NULL if not found.
 */
const RogueMaterialDef* rogue_material_find_by_item(int item_def_index)
{
    if (item_def_index < 0)
        return NULL;
    for (int i = 0; i < g_material_count; i++)
    {
        if (g_materials[i].item_def_index == item_def_index)
            return &g_materials[i];
    }
    return NULL;
}
/**
 * @brief Search for material ids that start with the provided prefix.
 *
 * Writes up to max indices into out_indices and returns the number written.
 *
 * @param prefix Prefix string to match.
 * @param out_indices Caller-provided array to receive matching indices.
 * @param max Maximum number of indices to write.
 * @return Number of matches written into out_indices.
 */
int rogue_material_prefix_search(const char* prefix, int* out_indices, int max)
{
    if (!prefix || !out_indices || max <= 0)
        return 0;
    int n = 0;
    size_t plen = strlen(prefix);
    for (int i = 0; i < g_material_count && n < max; i++)
    {
        if (strncmp(g_materials[i].id, prefix, plen) == 0)
            out_indices[n++] = i;
    }
    return n;
}

/**
 * @brief Find a material index by category and tier.
 *
 * @param category Material category constant.
 * @param tier Material tier.
 * @return Index of a matching material, or -1 if none found or invalid args.
 */
int rogue_material_find_by_category_and_tier(int category, int tier)
{
    if (tier < 0 || category < 0)
        return -1;
    for (int i = 0; i < g_material_count; i++)
    {
        if (g_materials[i].category == category && g_materials[i].tier == tier)
            return i;
    }
    return -1;
}

/**
 * @brief Return the registry index of the next tier for the same category.
 *
 * If the input material has tier N, this finds a material with tier N+1 in
 * the same category and returns its index, or -1 if not present.
 *
 * @param material_index Index of the base material.
 * @return Index of the next-tier material or -1 on error/not found.
 */
int rogue_material_next_tier_index(int material_index)
{
    if (material_index < 0 || material_index >= g_material_count)
        return -1;
    const RogueMaterialDef* m = &g_materials[material_index];
    int target_tier = m->tier + 1;
    for (int i = 0; i < g_material_count; i++)
    {
        if (g_materials[i].category == m->category && g_materials[i].tier == target_tier)
            return i;
    }
    return -1;
}

/**
 * @brief Trim whitespace from both ends of a C string in-place.
 *
 * Removes trailing CR/LF/space/tab characters and leading space/tab
 * characters. The operation modifies the buffer in place.
 */
static void trim(char* s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ' || s[n - 1] == '\t'))
        s[--n] = '\0';
    char* p = s;
    while (*p == ' ' || *p == '\t')
        p++;
    if (p != s)
        memmove(s, p, strlen(p) + 1);
}

/**
 * @brief Load material definitions from a file path.
 *
 * The loader supports two formats:
 *  - JSON: a compact array of objects with keys {id, item, tier, category, base_value}
 *  - CSV-like legacy: lines of comma-separated values: id,item_def_id,tier,category,base_value
 *
 * The function attempts to resolve item ids via the item definition registry
 * and skips unknown or duplicate entries. On success it appends entries to
 * the internal registry until capacity is reached.
 *
 * @param path Filesystem path to the materials file.
 * @return Number of entries added, or negative on error.
 */
int rogue_material_registry_load_path(const char* path)
{
    FILE* f = NULL;
#if defined(_MSC_VER)
    fopen_s(&f, path, "rb");
#else
    f = fopen(path, "rb");
#endif
    if (!f)
    {
        fprintf(stderr, "material_registry: cannot open %s\n", path);
        return -1;
    }
    /* If JSON file, slurp and parse a compact array of objects. Expected schema:
       [ { "id": "iron_ore_mat", "item": "iron_ore", "tier": 0, "category": "ore", "base_value": 8
       }, ... ] */
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
        const char* s = buf;
        while (*s && *s != '[')
            s++;
        if (*s != '[')
        {
            free(buf);
            return -1;
        }
        s++;
        int added = 0;
        while (*s)
        {
            while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t' || *s == ','))
                s++;
            if (*s == ']')
                break;
            if (*s != '{')
                break;
            s++;
            char id[32] = {0};
            char item_id[64] = {0};
            int tier = 0, cat = -1, base_val = 0;
            while (*s && *s != '}')
            {
                while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t'))
                    s++;
                if (*s != '"')
                {
                    s++;
                    continue;
                }
                s++;
                char key[32];
                int ki = 0;
                while (*s && *s != '"' && ki + 1 < (int) sizeof key)
                    key[ki++] = *s++;
                key[ki] = '\0';
                if (*s == '"')
                    s++;
                while (*s && *s != ':')
                    s++;
                if (*s == ':')
                    s++;
                while (*s && (*s == ' ' || *s == '\n' || *s == '\r' || *s == '\t'))
                    s++;
                if (strcmp(key, "id") == 0 || strcmp(key, "name") == 0)
                {
                    if (*s == '"')
                    {
                        s++;
                        int i = 0;
                        while (*s && *s != '"' && i + 1 < (int) sizeof id)
                            id[i++] = *s++;
                        id[i] = '\0';
                        if (*s == '"')
                            s++;
                    }
                }
                else if (strcmp(key, "item") == 0 || strcmp(key, "item_id") == 0)
                {
                    if (*s == '"')
                    {
                        s++;
                        int i = 0;
                        while (*s && *s != '"' && i + 1 < (int) sizeof item_id)
                            item_id[i++] = *s++;
                        item_id[i] = '\0';
                        if (*s == '"')
                            s++;
                    }
                }
                else if (strcmp(key, "tier") == 0)
                {
                    tier = (int) strtol(s, (char**) &s, 10);
                }
                else if (strcmp(key, "category") == 0)
                {
                    char catbuf[32] = {0};
                    if (*s == '"')
                    {
                        s++;
                        int i = 0;
                        while (*s && *s != '"' && i + 1 < (int) sizeof catbuf)
                            catbuf[i++] = *s++;
                        catbuf[i] = '\0';
                        if (*s == '"')
                            s++;
                    }
                    cat = category_from_str(catbuf);
                }
                else if (strcmp(key, "base_value") == 0)
                {
                    base_val = (int) strtol(s, (char**) &s, 10);
                }
                while (*s && *s != ',' && *s != '}')
                    s++;
                if (*s == ',')
                    s++;
            }
            while (*s && *s != '}')
                s++;
            if (*s == '}')
                s++;
            if (id[0] && item_id[0] && cat >= 0)
            {
                int item_def = rogue_item_def_index(item_id);
                if (item_def >= 0 && !rogue_material_find(id) &&
                    g_material_count < ROGUE_MATERIAL_REGISTRY_CAP)
                {
                    RogueMaterialDef* m = &g_materials[g_material_count++];
                    memset(m, 0, sizeof *m);
#if defined(_MSC_VER)
                    strncpy_s(m->id, sizeof(m->id), id, _TRUNCATE);
#else
                    strncpy(m->id, id, sizeof(m->id) - 1);
                    m->id[sizeof(m->id) - 1] = '\0';
#endif
                    m->item_def_index = item_def;
                    m->tier = tier < 0 ? 0 : (tier > 50 ? 50 : tier);
                    m->category = cat;
                    m->base_value = base_val < 0 ? 0 : base_val;
                    added++;
                }
            }
            while (*s && *s != ',' && *s != ']')
                s++;
            if (*s == ',')
                s++;
        }
        free(buf);
        return added;
    }
    char line[256];
    int added = 0;
    while (fgets(line, sizeof line, f))
    {
        trim(line);
        if (line[0] == '#' || line[0] == '\0')
            continue;
        if (g_material_count >= ROGUE_MATERIAL_REGISTRY_CAP)
        {
            fprintf(stderr, "material_registry: capacity reached\n");
            break;
        }
        /* id,item_def_id,tier,category,base_value */
        char* tok_ctx = NULL;
        char buf[256];
        {
            size_t ln = strlen(line);
            if (ln >= sizeof buf)
                ln = sizeof buf - 1;
            memcpy(buf, line, ln);
            buf[ln] = '\0';
        }
#if defined(_MSC_VER)
        char* tok = strtok_s(buf, ",", &tok_ctx);
#else
        char* tok = strtok(buf, ",");
#endif
        if (!tok)
            continue;
        char id[32];
        memset(id, 0, sizeof id);
        {
            size_t ilen = strlen(tok);
            if (ilen >= sizeof id)
                ilen = sizeof id - 1;
            memcpy(id, tok, ilen);
            id[ilen] = '\0';
        }
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &tok_ctx);
#else
        tok = strtok(NULL, ",");
#endif
        if (!tok)
            continue;
        int item_def = rogue_item_def_index(tok);
        if (item_def < 0)
        {
            fprintf(stderr, "material_registry: unknown item def %s\n", tok);
            continue;
        }
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &tok_ctx);
#else
        tok = strtok(NULL, ",");
#endif
        if (!tok)
            continue;
        int tier = atoi(tok);
        if (tier < 0)
            tier = 0;
        if (tier > 50)
            tier = 50; /* sanity */
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &tok_ctx);
#else
        tok = strtok(NULL, ",");
#endif
        if (!tok)
            continue;
        int cat = category_from_str(tok);
        if (cat < 0)
        {
            fprintf(stderr, "material_registry: bad category %s\n", tok);
            continue;
        }
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &tok_ctx);
#else
        tok = strtok(NULL, ",");
#endif
        if (!tok)
            continue;
        int base_val = atoi(tok);
        if (base_val < 0)
            base_val = 0;
        /* duplicate id check */
        if (rogue_material_find(id))
        {
            fprintf(stderr, "material_registry: duplicate %s skipped\n", id);
            continue;
        }
        RogueMaterialDef* m = &g_materials[g_material_count];
        memset(m, 0, sizeof *m);
        {
            size_t ilen = strlen(id);
            if (ilen >= sizeof m->id)
                ilen = sizeof m->id - 1;
            memcpy(m->id, id, ilen);
            m->id[ilen] = '\0';
        }
        m->item_def_index = item_def;
        m->tier = tier;
        m->category = cat;
        m->base_value = base_val;
        g_material_count++;
        added++;
    }
    fclose(f);
    return added;
}

/**
 * @brief Load the default materials file from the assets directory.
 *
 * Tries the modern path "materials/materials.cfg" and falls back to the
 * legacy "items/materials.cfg" for compatibility.
 *
 * @return Number of entries added, or negative on error.
 */
int rogue_material_registry_load_default(void)
{
    char path[256];
    /* Actual repo path uses assets/materials/materials.cfg; still support previous
     * items/materials.cfg if introduced later */
    if (!rogue_find_asset_path("materials/materials.cfg", path, sizeof path))
    {
        if (!rogue_find_asset_path("items/materials.cfg", path, sizeof path))
            return -1;
    }
    return rogue_material_registry_load_path(path);
}

unsigned int rogue_material_seed_mix(unsigned int world_seed, int material_index)
{ /* FNV-1a 32bit mix */
    unsigned int h = 2166136261u;
    h ^= world_seed;
    h *= 16777619u;
    h ^= (unsigned int) material_index;
    h *= 16777619u;
    return h;
}

/* Phase 6.3 helper: resolve material tier by associated item id (string) */
int rogue_material_tier_by_item(const char* item_id)
{
    if (!item_id)
        return -1;
    for (int i = 0; i < g_material_count; i++)
    {
        const RogueMaterialDef* m = &g_materials[i];
        if (m->item_def_index >= 0)
        {
            const RogueItemDef* d = rogue_item_def_at(m->item_def_index);
            if (d && strcmp(d->id, item_id) == 0)
                return m->tier;
        }
    }
    return -1;
}
