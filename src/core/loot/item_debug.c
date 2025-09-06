/**
 * @file item_debug.c
 * @author Christian "ChubbyChuckles" Rickert
 * @date 06/09/2025
 * @version 1.0
 *
 * This file provides debug utilities for manipulating and inspecting item definitions in the
 * rogue-like game. It includes functions for counting items, accessing definitions, modifying
 * fields like level requirements, damage, armor, stats, and resistances, as well as saving and
 * loading the item registry to/from JSON files, and creating new item definitions programmatically.
 */

#include "item_debug.h"
#include "../../content/json_io.h"
#include "../vendor/vendor.h"
#include "loot_item_defs.h"
#include <stdlib.h>
#include <string.h>

/**
 * @brief Returns the total number of item definitions in the registry.
 *
 * This function simply forwards to the core item definitions count for debugging purposes.
 *
 * @return The number of item definitions registered.
 */
int rogue_item_debug_count(void) { return rogue_item_defs_count(); }

/**
 * @brief Retrieves an item definition by index for inspection.
 *
 * Provides direct access to an item definition at the specified index in the registry.
 *
 * @param index The index of the item definition to retrieve.
 * @return Pointer to the RogueItemDef at the given index, or NULL if invalid.
 */
const RogueItemDef* rogue_item_debug_get(int index) { return rogue_item_def_at(index); }

/**
 * @brief Internal helper to set integer fields on an item definition.
 *
 * Sets various numeric fields on the item definition, such as level requirements, stack max, base
 * values, damage ranges, armor, rarity, flags, sockets, and implicit stats/resistances. Ensures
 * valid ranges (e.g., socket_max >= socket_min, clamped to 6).
 *
 * @param d Pointer to the mutable RogueItemDef to modify.
 * @param field Null-terminated string name of the field to set (e.g., "level_req",
 * "base_damage_min").
 * @param value The integer value to assign to the field.
 * @return 0 on success, -1 if field invalid or parameters null.
 */
static int set_int_field(RogueItemDef* d, const char* field, int value)
{
    if (!field || !d)
        return -1;
    /* Basic numeric fields commonly edited in overlay */
    if (strcmp(field, "level_req") == 0)
        d->level_req = value;
    else if (strcmp(field, "stack_max") == 0)
        d->stack_max = value > 0 ? value : 1;
    else if (strcmp(field, "base_value") == 0)
        d->base_value = value;
    else if (strcmp(field, "base_damage_min") == 0)
        d->base_damage_min = value;
    else if (strcmp(field, "base_damage_max") == 0)
        d->base_damage_max = value;
    else if (strcmp(field, "base_armor") == 0)
        d->base_armor = value;
    else if (strcmp(field, "rarity") == 0)
        d->rarity = value < 0 ? 0 : value;
    else if (strcmp(field, "flags") == 0)
        d->flags = value;
    else if (strcmp(field, "socket_min") == 0)
        d->socket_min = value < 0 ? 0 : value;
    else if (strcmp(field, "socket_max") == 0)
        d->socket_max = value < 0 ? 0 : value;
    else if (strcmp(field, "implicit_strength") == 0)
        d->implicit_strength = value;
    else if (strcmp(field, "implicit_dexterity") == 0)
        d->implicit_dexterity = value;
    else if (strcmp(field, "implicit_vitality") == 0)
        d->implicit_vitality = value;
    else if (strcmp(field, "implicit_intelligence") == 0)
        d->implicit_intelligence = value;
    else if (strcmp(field, "implicit_armor_flat") == 0)
        d->implicit_armor_flat = value;
    else if (strcmp(field, "implicit_resist_physical") == 0)
        d->implicit_resist_physical = value;
    else if (strcmp(field, "implicit_resist_fire") == 0)
        d->implicit_resist_fire = value;
    else if (strcmp(field, "implicit_resist_cold") == 0)
        d->implicit_resist_cold = value;
    else if (strcmp(field, "implicit_resist_lightning") == 0)
        d->implicit_resist_lightning = value;
    else if (strcmp(field, "implicit_resist_poison") == 0)
        d->implicit_resist_poison = value;
    else if (strcmp(field, "implicit_resist_status") == 0)
        d->implicit_resist_status = value;
    else
        return -1;
    if (d->socket_max < d->socket_min)
        d->socket_max = d->socket_min;
    if (d->socket_max > 6)
        d->socket_max = 6;
    return 0;
}

/**
 * @brief Sets an integer field on the item definition at the given index.
 *
 * Modifies the specified field on the item definition and notifies the vendor system to reprice
 * affected items if successful. Relies on internal mutability of the registry.
 *
 * @param index The index of the item definition to modify.
 * @param field Null-terminated string name of the field to set.
 * @param value The integer value to assign.
 * @return 0 on success, -1 on failure (invalid index, field, or parameters).
 */
int rogue_item_debug_set_int(int index, const char* field, int value)
{
    const RogueItemDef* c = rogue_item_def_at(index);
    if (!c)
        return -1;
    RogueItemDef* mut = (RogueItemDef*) c; /* internal registry is mutable */
    int rc = set_int_field(mut, field, value);
    if (rc == 0)
    {
        /* Notify vendor system to reprice affected items */
        (void) rogue_vendor_on_item_def_changed(index);
    }
    return rc;
}

/**
 * @brief Sets the name field on the item definition at the given index.
 *
 * Updates the name string on the item definition, truncating if necessary based on platform.
 * Notifies vendor system for consistency, though name changes don't affect pricing.
 *
 * @param index The index of the item definition to modify.
 * @param name Null-terminated string for the new name.
 * @return 0 on success, -1 if invalid index or name null.
 */
int rogue_item_debug_set_name(int index, const char* name)
{
    const RogueItemDef* c = rogue_item_def_at(index);
    if (!c || !name)
        return -1;
    RogueItemDef* mut = (RogueItemDef*) c;
#if defined(_MSC_VER)
    strncpy_s(mut->name, sizeof mut->name, name, _TRUNCATE);
#else
    strncpy(mut->name, name, sizeof mut->name - 1);
    mut->name[sizeof mut->name - 1] = '\0';
#endif
    /* Name doesn't affect price, but keep behavior consistent and notify in case UIs show names */
    (void) rogue_vendor_on_item_def_changed(index);
    return 0;
}

/**
 * @brief Saves the entire item definitions registry to a JSON file.
 *
 * Exports the registry to JSON using a growing buffer (starting at 64KB, max 8MB) to handle
 * varying sizes. Uses atomic write for safety. Intended for debugging and persistence.
 *
 * @param path Null-terminated string path to the output JSON file.
 * @return 0 on success, -1 on failure (invalid path, allocation error, or write failure).
 */
int rogue_item_debug_save_json(const char* path)
{
    if (!path)
        return -1;
    /* Export registry to JSON text, growing buffer until it fits */
    int cap = 64 * 1024;                 /* start at 64KB */
    const int cap_max = 8 * 1024 * 1024; /* hard cap at 8MB to avoid runaway */
    char* buf = NULL;
    int n = -1;
    for (;;)
    {
        if (buf)
            free(buf);
        buf = (char*) malloc((size_t) cap);
        if (!buf)
            return -1;
        n = rogue_item_defs_export_json(buf, cap);
        if (n >= 0)
            break; /* success */
        cap *= 2;
        if (cap > cap_max)
        {
            free(buf);
            return -1;
        }
    }
    char err[128];
    int rc = json_io_write_atomic(path, buf, (size_t) n, err, (int) sizeof err);
    free(buf);
    return rc == 0 ? 0 : -1;
}

/**
 * @brief Loads item definitions from a JSON file into the registry.
 *
 * Merges or adds definitions from the JSON file and triggers full vendor repricing if successful,
 * as multiple items may have changed.
 *
 * @param path Null-terminated string path to the input JSON file.
 * @return Number of items added on success, -1 on failure (invalid path or parse error).
 */
int rogue_item_debug_load_json(const char* path)
{
    if (!path)
        return -1;
    int added_or_err = rogue_item_defs_load_from_json(path);
    /* After load/merge, safe to reprice all vendor items since multiple defs could change */
    if (added_or_err >= 0)
        rogue_vendor_reprice_all();
    return added_or_err;
}

/**
 * @brief Creates and adds a new item definition to the registry.
 *
 * Initializes a new RogueItemDef with provided parameters, validates inputs (non-empty id/name,
 * valid category, no duplicate id), sets defaults/clamps values (e.g., stack_max >=1, damage_max >=
 * min, sockets <=6, sprite tiles >=1), adds to registry, and triggers full vendor repricing.
 *
 * @param id Unique null-terminated string identifier for the item.
 * @param name Display name null-terminated string.
 * @param category The item category (must be valid enum value).
 * @param level_req Required player level.
 * @param stack_max Maximum stack size (clamped >=1).
 * @param base_value Base monetary value.
 * @param base_dmg_min Minimum base damage.
 * @param base_dmg_max Maximum base damage (clamped >= min).
 * @param base_armor Base armor value.
 * @param rarity Item rarity level (clamped >=0).
 * @param socket_min Minimum sockets (clamped >=0).
 * @param socket_max Maximum sockets (clamped >= min, <=6).
 * @return Index of added item on success, -1 on invalid params, -2 on duplicate id.
 */
int rogue_item_debug_create(const char* id, const char* name, RogueItemCategory category,
                            int level_req, int stack_max, int base_value, int base_dmg_min,
                            int base_dmg_max, int base_armor, int rarity, int socket_min,
                            int socket_max)
{
    if (!id || !name)
        return -1;
    if (!id[0] || !name[0])
        return -1;
    if (category < 0 || category >= ROGUE_ITEM__COUNT)
        return -1;
    if (rogue_item_def_index_fast(id) >= 0)
        return -2; /* duplicate id */
    RogueItemDef d;
    memset(&d, 0, sizeof d);
#if defined(_MSC_VER)
    strncpy_s(d.id, sizeof d.id, id, _TRUNCATE);
    strncpy_s(d.name, sizeof d.name, name, _TRUNCATE);
#else
    strncpy(d.id, id, sizeof d.id - 1);
    strncpy(d.name, name, sizeof d.name - 1);
#endif
    d.category = category;
    d.level_req = level_req;
    d.stack_max = stack_max > 0 ? stack_max : 1;
    d.base_value = base_value;
    d.base_damage_min = base_dmg_min;
    d.base_damage_max = base_dmg_max < base_dmg_min ? base_dmg_min : base_dmg_max;
    d.base_armor = base_armor;
    d.rarity = rarity < 0 ? 0 : rarity;
    d.socket_min = socket_min < 0 ? 0 : socket_min;
    d.socket_max = socket_max < d.socket_min ? d.socket_min : socket_max;
    if (d.socket_max > 6)
        d.socket_max = 6;
    /* minimum sprite tile defaults to 1x1 to avoid zero */
    d.sprite_tw = d.sprite_tw <= 0 ? 1 : d.sprite_tw;
    d.sprite_th = d.sprite_th <= 0 ? 1 : d.sprite_th;
    int idx = rogue_item_defs_add(&d);
    if (idx >= 0)
    {
        /* Prices for existing vendor inventory using this id don't exist yet, but repricing all is
         * cheap and keeps behaviors consistent in case the registry index matches a current slot
         * after a reload/merge.
         */
        rogue_vendor_reprice_all();
    }
    return idx; /* may be -1 or -2 */
}
