#include "core/progression/progression_stats.h"
#include <string.h>

/* Static registry ordered by ID asc. Keep contiguous for binary search potential (linear ok small N). */
#define RS(id, code, name, cat, res) { id, code, name, cat, res }
static const RogueStatDef g_stats[] = {
    /* Primary Attributes (0-99) */
    RS(0,  "STR",   "Strength",   ROGUE_STAT_PRIMARY, 0),
    RS(1,  "DEX",   "Dexterity",  ROGUE_STAT_PRIMARY, 0),
    RS(2,  "VIT",   "Vitality",   ROGUE_STAT_PRIMARY, 0),
    RS(3,  "INT",   "Intelligence",ROGUE_STAT_PRIMARY, 0),
    /* Derived (100-199) */
    RS(100,"CRITC", "Crit Chance %", ROGUE_STAT_DERIVED, 0),
    RS(101,"CRITD", "Crit Damage %", ROGUE_STAT_DERIVED, 0),
    RS(102,"DPS_EST","DPS Estimate", ROGUE_STAT_DERIVED, 0),
    RS(103,"EHP_EST","EHP Estimate", ROGUE_STAT_DERIVED, 0),
    RS(104,"TOUGH",  "Toughness Index", ROGUE_STAT_DERIVED, 0),
    RS(105,"MOBI",   "Mobility Index", ROGUE_STAT_DERIVED, 0),
    RS(106,"SUST",   "Sustain Index", ROGUE_STAT_DERIVED, 1), /* placeholder not yet computed */
    /* Resistances (derived) */
    RS(120,"RES_PHY","Physical Resist %", ROGUE_STAT_DERIVED, 0),
    RS(121,"RES_FIR","Fire Resist %",     ROGUE_STAT_DERIVED, 0),
    RS(122,"RES_COL","Cold Resist %",     ROGUE_STAT_DERIVED, 0),
    RS(123,"RES_LIT","Lightning Resist %",ROGUE_STAT_DERIVED, 0),
    RS(124,"RES_POI","Poison Resist %",   ROGUE_STAT_DERIVED, 0),
    RS(125,"RES_STA","Status Resist %",   ROGUE_STAT_DERIVED, 0),
    /* Ratings (200-299) future Phase 3 placeholders */
    RS(200,"CRIT_R", "Crit Rating", ROGUE_STAT_RATING, 1),
    RS(201,"HASTE_R","Haste Rating",ROGUE_STAT_RATING, 1),
    RS(202,"AVOID_R","Avoidance Rating",ROGUE_STAT_RATING,1),
    /* Modifiers (300-399) future use */
    RS(300,"DMG_MOD","Damage % Modifier",ROGUE_STAT_MODIFIER,1),
    RS(301,"SPD_MOD","Speed % Modifier", ROGUE_STAT_MODIFIER,1),
};
#undef RS

static unsigned long long hash_u64(unsigned long long h, unsigned long long v){
    /* Simple mix (xorshift64* inspired) */
    v ^= v >> 33; v *= 0xff51afd7ed558ccdULL; v ^= v >> 33; v *= 0xc4ceb9fe1a85ec53ULL; v ^= v >> 33;
    h ^= v + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2);
    return h;
}

const RogueStatDef* rogue_stat_def_all(size_t* count){ if(count) *count = sizeof(g_stats)/sizeof(g_stats[0]); return g_stats; }

const RogueStatDef* rogue_stat_def_by_id(int id){
    for(size_t i=0;i<sizeof(g_stats)/sizeof(g_stats[0]);++i){ if(g_stats[i].id==id) return &g_stats[i]; }
    return 0;
}

unsigned long long rogue_stat_registry_fingerprint(void){
    unsigned long long h=0xcbf29ce484222325ULL; /* seed */
    for(size_t i=0;i<sizeof(g_stats)/sizeof(g_stats[0]);++i){
        const RogueStatDef* d=&g_stats[i];
        h = hash_u64(h,(unsigned long long)(unsigned int)d->id);
        h = hash_u64(h,(unsigned long long)(unsigned int)d->category);
        h = hash_u64(h,(unsigned long long)(unsigned int)d->reserved);
        /* incorporate code/name chars */
        for(const char* c=d->code; *c; ++c) h = hash_u64(h,(unsigned long long)(unsigned char)*c);
        for(const char* c=d->name; *c; ++c) h = hash_u64(h,(unsigned long long)(unsigned char)*c);
    }
    return h;
}
