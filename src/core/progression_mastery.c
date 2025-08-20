#include "core/progression_mastery.h"
#include <stdlib.h>
#include <string.h>

/* Simple fixed-size repository: support up to 1024 skill ids (sparse mapping by id index).
 * Use dynamic resize if larger ids used in future.
 */
static double* g_xp = NULL;
static int g_cap = 0;

int rogue_progression_mastery_init(void){
    if(g_xp) return 0;
    g_cap = 1024;
    g_xp = (double*)calloc((size_t)g_cap, sizeof(double));
    return g_xp?0:-1;
}

void rogue_progression_mastery_shutdown(void){
    free(g_xp); g_xp = NULL; g_cap = 0;
}

static int ensure_capacity(int skill_id){
    if(skill_id < g_cap) return 0;
    int newcap = g_cap;
    while(newcap <= skill_id) newcap *= 2;
    double* tmp = (double*)realloc(g_xp, (size_t)newcap * sizeof(double));
    if(!tmp) return -1;
    memset(tmp + g_cap, 0, (size_t)(newcap - g_cap) * sizeof(double));
    g_xp = tmp; g_cap = newcap; return 0;
}

double rogue_progression_mastery_add_xp(int skill_id, double xp){
    if(skill_id < 0) return 0.0;
    if(!g_xp){ if(rogue_progression_mastery_init()<0) return 0.0; }
    if(skill_id >= g_cap){ if(ensure_capacity(skill_id)<0) return 0.0; }
    g_xp[skill_id] += xp;
    return g_xp[skill_id];
}

double rogue_progression_mastery_get_xp(int skill_id){
    if(skill_id < 0) return 0.0;
    if(!g_xp) return 0.0;
    if(skill_id >= g_cap) return 0.0;
    return g_xp[skill_id];
}

/* Rank function: rank increases when XP crosses threshold T(rank) = 100 * (1.5^rank)
 * This gives diminishing returns in XP per rank.
 */
double rogue_progression_mastery_threshold_for_rank(int rank){
    if(rank < 0) return 100.0;
    double t = 100.0;
    while(rank-- > 0) t *= 1.5; /* simple loop -- rank small in practice */
    return t;
}

int rogue_progression_mastery_get_rank(int skill_id){
    double xp = rogue_progression_mastery_get_xp(skill_id);
    int rank = 0;
    double thresh = 100.0;
    while(xp >= thresh){ xp -= thresh; rank++; thresh *= 1.5; if(rank > 1000) break; }
    return rank;
}

