/* Extended mastery implementation (Phases 6.1–6.5) */
#include "core/progression/progression_mastery.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct MasteryEntry {
    unsigned long long xp;          /* total xp */
    unsigned int last_activity_ms;  /* last timestamp xp added */
    unsigned short rank_cache;      /* cached rank */
    unsigned char dirty;            /* rank needs recompute */
} MasteryEntry;

static MasteryEntry* g_entries = NULL;
static int g_cap = 0;
static int g_decay_enabled = 0;
static unsigned int g_now = 0; /* internal monotonic clock */
static int g_ring_points_cache = -1; /* recomputed lazily */

/* Tunables */
enum { MASTER_INITIAL_CAP = 128 };
static const double RANK_BASE_THRESH = 100.0; /* T(0) */
static const double RANK_GROWTH = 1.5;        /* geometric */
static const unsigned short RING_UNLOCK_RANK = 5; /* reaching this rank contributes ring point */
static const unsigned int DECAY_GRACE_MS = 60000; /* 60s inactivity grace */
static const unsigned int DECAY_INTERVAL_MS = 15000; /* every 15s after grace */
static const double DECAY_FRACTION = 0.10; /* remove 10% of surplus over current rank floor */

static double threshold_for_rank_int(int rank){
    double t = RANK_BASE_THRESH;
    while(rank-- > 0) t *= RANK_GROWTH;
    return t;
}

static int ensure_cap(int skill_id){
    if(skill_id < g_cap) return 0;
    int newcap = g_cap ? g_cap : MASTER_INITIAL_CAP;
    while(newcap <= skill_id) newcap *= 2;
    MasteryEntry* tmp = (MasteryEntry*)realloc(g_entries, (size_t)newcap * sizeof(MasteryEntry));
    if(!tmp) return -1;
    if(newcap > g_cap) memset(tmp + g_cap, 0, (size_t)(newcap - g_cap) * sizeof(MasteryEntry));
    g_entries = tmp; g_cap = newcap; return 0;
}

static unsigned short recompute_rank(MasteryEntry* e){
    if(!e) return 0;
    unsigned long long xp = e->xp;
    unsigned short rank = 0;
    double thresh = RANK_BASE_THRESH;
    while(xp >= (unsigned long long)thresh){ xp -= (unsigned long long)thresh; rank++; thresh *= RANK_GROWTH; if(rank > 2000) break; }
    e->rank_cache = rank; e->dirty = 0; return rank;
}

int rogue_mastery_init(int max_skills, int enable_decay){
    if(g_entries) return 0;
    g_cap = max_skills > 0 ? max_skills : MASTER_INITIAL_CAP;
    g_entries = (MasteryEntry*)calloc((size_t)g_cap, sizeof(MasteryEntry));
    if(!g_entries){ g_cap = 0; return -1; }
    g_decay_enabled = enable_decay ? 1 : 0; g_now = 0; g_ring_points_cache = -1; return 0;
}

void rogue_mastery_shutdown(void){
    free(g_entries); g_entries=NULL; g_cap=0; g_ring_points_cache=-1; g_now=0; }

static void mark_ring_points_dirty(void){ g_ring_points_cache = -1; }

int rogue_mastery_add_xp(int skill_id, unsigned int xp, unsigned int timestamp_ms){
    if(skill_id < 0) return -1;
    if(!g_entries){ if(rogue_mastery_init(skill_id+1, g_decay_enabled)<0) return -1; }
    if(ensure_cap(skill_id)<0) return -1;
    MasteryEntry* e = &g_entries[skill_id];
    if(timestamp_ms > g_now) g_now = timestamp_ms; /* advance clock */
    /* Scale raw xp by mastery tier bonus? For acquisition we keep linear; tiers affect usage bonus not accrual */
    e->xp += xp;
    e->last_activity_ms = g_now;
    e->dirty = 1;
    mark_ring_points_dirty();
    return 0;
}

void rogue_mastery_update(unsigned int elapsed_ms){
    if(!g_entries) return;
    g_now += elapsed_ms;
    if(!g_decay_enabled) return;
    for(int i=0;i<g_cap;i++){
        MasteryEntry* e = &g_entries[i]; if(!e->xp) continue;
        unsigned int inactive = (g_now >= e->last_activity_ms) ? (g_now - e->last_activity_ms) : 0;
        if(inactive < DECAY_GRACE_MS) continue;
        unsigned int decay_windows = (inactive - DECAY_GRACE_MS) / DECAY_INTERVAL_MS;
        if(decay_windows == 0) continue;
        /* Determine floor XP for current rank (sum thresholds up to rank). */
        if(e->dirty) recompute_rank(e);
        unsigned short rank = e->rank_cache;
        double floor = 0.0; for(int r=0;r<rank;r++){ floor += threshold_for_rank_int(r); }
        double surplus = (double)e->xp - floor; if(surplus <= 0.0) continue;
        double decay_total = surplus * (1.0 - pow(1.0-DECAY_FRACTION, decay_windows));
        unsigned long long remove = (unsigned long long)decay_total;
        if(remove > surplus) remove = (unsigned long long)surplus;
        e->xp -= remove; e->dirty = 1; mark_ring_points_dirty();
        /* Advance anchor so we don't repeatedly decay same window overly; reset last_activity to grace boundary start */
        e->last_activity_ms = g_now - (inactive % DECAY_INTERVAL_MS); 
    }
}

unsigned short rogue_mastery_rank(int skill_id){
    if(skill_id < 0 || !g_entries || skill_id >= g_cap) return 0;
    MasteryEntry* e = &g_entries[skill_id];
    if(e->dirty) return recompute_rank(e);
    return e->rank_cache;
}

unsigned long long rogue_mastery_xp(int skill_id){
    if(skill_id < 0 || !g_entries || skill_id >= g_cap) return 0ULL;
    return g_entries[skill_id].xp;
}

unsigned long long rogue_mastery_xp_to_next(int skill_id){
    unsigned short r = rogue_mastery_rank(skill_id);
    double thresh = threshold_for_rank_int(r);
    unsigned long long needed = (unsigned long long)thresh;
    unsigned long long have = rogue_mastery_xp(skill_id);
    /* subtract floors */
    if(r>0){ double floor=0.0; for(int i=0;i<r;i++) floor += threshold_for_rank_int(i); if(have > (unsigned long long)floor) have -= (unsigned long long)floor; else have=0; }
    if(have >= needed) return 0ULL;
    return needed - have;
}

float rogue_mastery_bonus_scalar(int skill_id){
    unsigned short r = rogue_mastery_rank(skill_id);
    /* Rank brackets -> scalar mapping */
    if(r >= 25) return 1.20f;
    if(r >= 15) return 1.16f;
    if(r >= 10) return 1.12f;
    if(r >= 7)  return 1.09f;
    if(r >= 5)  return 1.06f;
    if(r >= 3)  return 1.03f;
    if(r >= 1)  return 1.01f;
    return 1.0f;
}

int rogue_mastery_minor_ring_points(void){
    if(!g_entries) return 0;
    if(g_ring_points_cache >= 0) return g_ring_points_cache;
    int count=0; for(int i=0;i<g_cap;i++){ if(g_entries[i].xp){ if(rogue_mastery_rank(i) >= RING_UNLOCK_RANK) count++; } }
    g_ring_points_cache = count; return count;
}

void rogue_mastery_set_decay(int enabled){ g_decay_enabled = enabled ? 1 : 0; }

/* --- Back-compat wrappers (Phase 6.1 minimal API) --- */
int rogue_progression_mastery_init(void){ return rogue_mastery_init(MASTER_INITIAL_CAP, 0); }
void rogue_progression_mastery_shutdown(void){ rogue_mastery_shutdown(); }
double rogue_progression_mastery_add_xp(int skill_id, double xp){ if(xp <= 0.0) return (double)rogue_mastery_xp(skill_id); rogue_mastery_add_xp(skill_id,(unsigned int)(xp+0.5), g_now); return (double)rogue_mastery_xp(skill_id); }
double rogue_progression_mastery_get_xp(int skill_id){ return (double)rogue_mastery_xp(skill_id); }
int rogue_progression_mastery_get_rank(int skill_id){ return (int)rogue_mastery_rank(skill_id); }
double rogue_progression_mastery_threshold_for_rank(int rank){ return threshold_for_rank_int(rank); }

