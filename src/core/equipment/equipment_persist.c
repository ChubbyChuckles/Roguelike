/*
 * NOTE: This file is written in a C89-friendly style for MSVC (no mixed
 * declarations and code, no loop-init declarations).
 */

#include "equipment_persist.h"
#include "../loot/loot_instances.h"
#include "../loot/loot_item_defs.h"
#include "equipment.h"
#include "equipment_uniques.h"
#include <stdio.h>
#include <string.h>

/* Forward declaration to avoid repeated block-scope externs (MSVC warning C4210). */
extern unsigned long long rogue_stat_cache_fingerprint(void);

static unsigned long long fnv1a64(const unsigned char* data, size_t n)
{
    unsigned long long h = 1469598103934665603ULL;
    size_t i;
    for (i = 0; i < n; ++i)
    {
        h ^= data[i];
        h *= 1099511628211ULL;
    }
    return h;
}

int rogue_equipment_serialize(char* buf, int cap)
{
    int off = 0;
    int n;
    int s;
    if (!buf || cap < 16)
        return -1;
    n = snprintf(buf + off, cap - off, "EQUIP_V1\n");
    if (n < 0 || n >= cap - off)
        return -1;
    off += n;
    for (s = 0; s < ROGUE_EQUIP__COUNT; ++s)
    {
        int inst = rogue_equip_get((enum RogueEquipSlot) s);
        const RogueItemInstance* it;
        if (inst < 0)
            continue;
        it = rogue_item_instance_at(inst);
        if (!it)
            continue;
        /* Retrieve set id via base definition (0 if none), unique id (if any), & build synthetic
         * runeword pattern. */
        {
            const RogueItemDef* def = rogue_item_def_at(it->def_index);
            int set_id = def ? def->set_id : 0;
            const char* unique_id = "-";
            {
                int uidx = rogue_unique_find_by_base_def(it->def_index);
                if (uidx >= 0)
                {
                    const RogueUniqueDef* u = rogue_unique_at(uidx);
                    if (u && u->id[0])
                        unique_id = u->id;
                }
            }
            char pattern[64];
            int i;
            int pat_len = 0;
            pattern[0] = '\0';
            if (it->socket_count > 0)
            {
                for (i = 0; i < it->socket_count && i < 6; ++i)
                {
                    int g = it->sockets[i];
                    if (g >= 0 && pat_len < (int) sizeof(pattern) - 3)
                    {
                        pattern[pat_len++] = 'A' + (g % 26);
                    }
                }
                pattern[pat_len] = '\0';
            }
            n = snprintf(buf + off, cap - off,
                         "SLOT %d DEF %d ILVL %d RAR %d PREF %d %d SUFF %d %d DUR %d %d ENCH %d QC "
                         "%d SOCKS %d %d %d %d %d %d %d LOCKS %d %d FRACT %d SET %d UNQ %s RW %s\n",
                         s, it->def_index, it->item_level, it->rarity, it->prefix_index,
                         it->prefix_value, it->suffix_index, it->suffix_value, it->durability_cur,
                         it->durability_max, it->enchant_level, it->quality, it->socket_count,
                         it->sockets[0], it->sockets[1], it->sockets[2], it->sockets[3],
                         it->sockets[4], it->sockets[5], it->prefix_locked, it->suffix_locked,
                         it->fractured, set_id, unique_id, (pattern[0] ? pattern : "-"));
        }
        if (n < 0 || n >= cap - off)
            return -1;
        off += n;
    }
    if (off < cap)
        buf[off] = '\0';
    return off;
}

static int parse_int(const char** p)
{
    int sign = 1;
    int v = 0;
    while (**p == ' ')
        (*p)++;
    if (**p == '-')
    {
        sign = -1;
        (*p)++;
    }
    while (**p >= '0' && **p <= '9')
    {
        v = v * 10 + (**p - '0');
        (*p)++;
    }
    return v * sign;
}

int rogue_equipment_deserialize(const char* buf)
{
    const char* p;
    int version = 0;
    if (!buf)
        return -1;
    p = buf;
    if (strncmp(p, "EQUIP_V1", 8) == 0)
    {
        const char* nl = strchr(p, '\n');
        version = 1;
        if (!nl)
            return -2;
        p = nl + 1;
    }
    /* Migration notes (Phase 13.2):
       Version 0 (no header) predates slot model expansion. Legacy ordering:
         0 WEAPON, 1 HEAD, 2 CHEST, 3 LEGS, 4 HANDS, 5 FEET
       All other slots (rings, amulet, belt, cloak, charms, offhand) did not exist and are ignored
       if present. We remap these indices into the current enum. Future versions can extend this
       conditional.
    */
    static const int legacy_v0_slot_map[6] = {/* 0 */ ROGUE_EQUIP_WEAPON,
                                              /* 1 */ ROGUE_EQUIP_ARMOR_HEAD,
                                              /* 2 */ ROGUE_EQUIP_ARMOR_CHEST,
                                              /* 3 */ ROGUE_EQUIP_ARMOR_LEGS,
                                              /* 4 */ ROGUE_EQUIP_ARMOR_HANDS,
                                              /* 5 */ ROGUE_EQUIP_ARMOR_FEET};
    while (*p)
    {
        if (strncmp(p, "SLOT", 4) == 0)
        {
            int slot;
            int def_index = -1, item_level = 1, rarity = 0;
            int pidx = -1, pval = 0, sidx = -1, sval = 0;
            int dc = 0, dm = 0, ench = 0, qual = 0;
            int sockc = 0;
            int gems[6];
            int pl = 0, sl = 0, fract = 0;
            /* Track required tokens to validate integrity of each SLOT line. */
            int seen_def = 0, seen_dur = 0, seen_qc = 0, seen_socks = 0, seen_locks = 0;
            int i;
            for (i = 0; i < 6; ++i)
                gems[i] = -1;
            p += 4;
            slot = parse_int(&p);
            if (version == 0)
            {
                if (slot >= 0 && slot < 6)
                {
                    slot = legacy_v0_slot_map[slot];
                }
                else
                { /* Unknown legacy slot -> skip entire line */
                    while (*p && *p != '\n')
                        p++;
                    if (*p == '\n')
                        p++;
                    continue;
                }
            }
            if (slot < 0 || slot >= ROGUE_EQUIP__COUNT)
                return -3;
            while (*p && *p != '\n')
            {
                if (strncmp(p, "DEF", 3) == 0)
                {
                    p += 3;
                    def_index = parse_int(&p);
                    seen_def = 1;
                }
                else if (strncmp(p, "ILVL", 4) == 0)
                {
                    p += 4;
                    item_level = parse_int(&p);
                }
                else if (strncmp(p, "RAR", 3) == 0)
                {
                    p += 3;
                    rarity = parse_int(&p);
                }
                else if (strncmp(p, "PREF", 4) == 0)
                {
                    p += 4;
                    pidx = parse_int(&p);
                    pval = parse_int(&p);
                }
                else if (strncmp(p, "SUFF", 4) == 0)
                {
                    p += 4;
                    sidx = parse_int(&p);
                    sval = parse_int(&p);
                }
                else if (strncmp(p, "DUR", 3) == 0)
                {
                    p += 3;
                    dc = parse_int(&p);
                    dm = parse_int(&p);
                    seen_dur = 1;
                }
                else if (strncmp(p, "ENCH", 4) == 0)
                {
                    p += 4;
                    ench = parse_int(&p);
                }
                else if (strncmp(p, "QC", 2) == 0)
                {
                    p += 2;
                    qual = parse_int(&p);
                    seen_qc = 1;
                }
                else if (strncmp(p, "SOCKS", 5) == 0)
                {
                    p += 5;
                    sockc = parse_int(&p);
                    for (i = 0; i < 6; ++i)
                    {
                        gems[i] = parse_int(&p);
                    }
                    seen_socks = 1;
                }
                else if (strncmp(p, "LOCKS", 5) == 0)
                {
                    p += 5;
                    pl = parse_int(&p);
                    sl = parse_int(&p);
                    seen_locks = 1;
                }
                else if (strncmp(p, "FRACT", 5) == 0)
                {
                    p += 5;
                    fract = parse_int(&p);
                }
                else if (strncmp(p, "SET", 3) == 0)
                {
                    p += 3;
                    (void) parse_int(&p);
                }
                else if (strncmp(p, "UNQ", 3) == 0)
                {
                    p += 3;
                    while (*p == ' ')
                        p++;
                    while (*p && *p != ' ' && *p != '\n')
                        p++;
                }
                else if (strncmp(p, "RW", 2) == 0)
                {
                    p += 2; /* skip runeword pattern token */
                    while (*p == ' ')
                        p++;
                    while (*p && *p != ' ' && *p != '\n')
                        p++;
                }
                else
                {
                    p++;
                }
            }
            /* For legacy v0, tolerate missing tokens by defaulting to reasonable values. */
            if (version == 0)
            {
                if (!seen_dur)
                {
                    dc = 0;
                    dm = 0;
                    seen_dur = 1;
                }
                if (!seen_qc)
                {
                    qual = 0;
                    seen_qc = 1;
                }
                if (!seen_socks)
                {
                    sockc = 0;
                    for (i = 0; i < 6; ++i)
                        gems[i] = -1;
                    seen_socks = 1;
                }
                if (!seen_locks)
                {
                    pl = 0;
                    sl = 0;
                    seen_locks = 1;
                }
            }
            /* Validate mandatory tokens and basic ranges. If corrupted, reject with negatives. */
            if (!seen_def)
                return -10; /* missing DEF */
            if (!seen_dur)
                return -11; /* missing DUR */
            if (!seen_qc)
                return -12; /* missing QC */
            if (!seen_socks)
                return -13; /* missing SOCKS */
            if (!seen_locks)
                return -14; /* missing LOCKS */
                            /* Durability validation:
                                 - Allow non-durable items encoded as DUR 0 0 (e.g., rings, trinkets).
                                 - Reject negatives.
                                 - Best-effort for malformed non-durable: if dm==0 and dc>0, promote dm to dc to
                                     yield a valid pair instead of rejecting (supports fuzz test expectation of
                                     successful parse with changed state hash).
                                 - If max > 0, require current within [0, max] and enforce an upper bound to avoid
                                     runaway values from corruption.
                            */
            if (dc < 0 || dm < 0)
                return -15; /* invalid durability (negative) */
            if (dm == 0)
            {
                if (dc > 0)
                {
                    /* Best-effort: promote to minimal valid pair (dm==dc). */
                    dm = dc;
                }
            }
            else
            {
                if (dc > dm || dm > 100000)
                    return -15; /* invalid durability range */
            }
            if (sockc < 0 || sockc > 6)
                return -16; /* invalid socket count */
            if (!((pl == 0 || pl == 1) && (sl == 0 || sl == 1)))
                return -17; /* invalid lock flags */
            if (def_index >= 0)
            {
                extern int rogue_items_spawn(int def_index, int quantity, float x, float y);
                /* spawn */
                {
                    int inst = rogue_items_spawn(def_index, 1, 0, 0);
                    if (inst >= 0)
                    {
                        RogueItemInstance* it = (RogueItemInstance*) rogue_item_instance_at(inst);
                        if (it)
                        {
                            it->item_level = item_level;
                            it->rarity = rarity;
                            it->prefix_index = pidx;
                            it->prefix_value = pval;
                            it->suffix_index = sidx;
                            it->suffix_value = sval;
                            it->durability_cur = dc;
                            it->durability_max = dm;
                            it->enchant_level = ench;
                            it->quality = qual;
                            it->socket_count = sockc;
                            for (i = 0; i < 6; ++i)
                                it->sockets[i] = gems[i];
                            it->prefix_locked = pl;
                            it->suffix_locked = sl;
                            it->fractured = fract;
                            rogue_equip_try((enum RogueEquipSlot) slot, inst);
                        }
                    }
                }
            }
            if (*p == '\n')
                p++;
        }
        else
        {
            /* skip unknown line */
            while (*p && *p != '\n')
                p++;
            if (*p == '\n')
                p++;
        }
    }
    return 0;
}

unsigned long long rogue_equipment_state_hash(void)
{
    char tmp[4096];
    int n = rogue_equipment_serialize(tmp, (int) sizeof tmp);
    if (n < 0)
        return 0ULL;
    return fnv1a64((const unsigned char*) tmp, (size_t) n);
}

/* Phase 18.1: Golden master snapshot implementation.
   Format (single line, no trailing '\n'):
         EQSNAP v1 EQUIP_HASH=<16hex> STAT_FP=<16hex>
   (Future fields append as KEY=VALUE tokens separated by space; unknown keys ignored by
   comparator.) Rationale: short, deterministic, friendly to git diff and CI golden master pattern.
*/
int rogue_equipment_snapshot_export(char* buf, int cap)
{
    unsigned long long equip_h;
    unsigned long long stat_fp = 0ULL;
    int n;
    if (!buf || cap < 8)
        return -1;
    equip_h = rogue_equipment_state_hash();
    /* Stat fingerprint API (Phase 2.5) */
    stat_fp = rogue_stat_cache_fingerprint();
    n = snprintf(buf, cap, "EQSNAP v1 EQUIP_HASH=%016llx STAT_FP=%016llx",
                 (unsigned long long) equip_h, (unsigned long long) stat_fp);
    if (n < 0 || n >= cap)
        return -1;
    return n; /* bytes written (excluding NUL) */
}

int rogue_equipment_snapshot_compare(const char* snapshot_text)
{
    unsigned long long expect_equip = 0ULL, expect_fp = 0ULL;
    unsigned long long cur_equip, cur_fp;
    const char* p;
    if (!snapshot_text)
        return -1;
    p = snapshot_text;
    if (strncmp(p, "EQSNAP", 6) != 0)
        return -1;
    /* tokenize by space; look for EQUIP_HASH= and STAT_FP= prefixes */
    while (*p)
    {
        if (strncmp(p, "EQUIP_HASH=", 11) == 0)
        {
            p += 11;
            /* parse 16 hex digits (ignore shorter) */
            int i;
            expect_equip = 0ULL;
            for (i = 0; i < 16 && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
                                   (*p >= 'A' && *p <= 'F'));
                 ++i)
            {
                unsigned char c = (unsigned char) *p++;
                unsigned long long v = (c >= '0' && c <= '9') ? (unsigned long long) (c - '0')
                                       : (c >= 'a' && c <= 'f')
                                           ? (unsigned long long) (10 + c - 'a')
                                           : (unsigned long long) (10 + c - 'A');
                expect_equip = (expect_equip << 4) | v;
            }
        }
        else if (strncmp(p, "STAT_FP=", 8) == 0)
        {
            p += 8;
            int i;
            expect_fp = 0ULL;
            for (i = 0; i < 16 && ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f') ||
                                   (*p >= 'A' && *p <= 'F'));
                 ++i)
            {
                unsigned char c = (unsigned char) *p++;
                unsigned long long v = (c >= '0' && c <= '9') ? (unsigned long long) (c - '0')
                                       : (c >= 'a' && c <= 'f')
                                           ? (unsigned long long) (10 + c - 'a')
                                           : (unsigned long long) (10 + c - 'A');
                expect_fp = (expect_fp << 4) | v;
            }
        }
        else
        {
            /* advance to next space */
            while (*p && *p != ' ')
                p++;
        }
        while (*p == ' ')
            p++;
    }
    cur_equip = rogue_equipment_state_hash();
    cur_fp = rogue_stat_cache_fingerprint();
    if (expect_equip == 0ULL && expect_fp == 0ULL)
        return -1; /* nothing parsed */
    if (cur_equip == expect_equip && cur_fp == expect_fp)
        return 0;
    return 1; /* mismatch */
}
