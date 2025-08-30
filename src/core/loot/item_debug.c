#include "item_debug.h"
#include "../../content/json_io.h"
#include "../vendor/vendor.h"
#include "loot_item_defs.h"
#include <stdlib.h>
#include <string.h>

int rogue_item_debug_count(void) { return rogue_item_defs_count(); }

const RogueItemDef* rogue_item_debug_get(int index) { return rogue_item_def_at(index); }

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
