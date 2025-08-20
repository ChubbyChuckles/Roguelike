#include "core/material_refine.h"
#include "core/material_registry.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef ROGUE_MATERIAL_REGISTRY_CAP
#define ROGUE_MATERIAL_REGISTRY_CAP 128
#endif

/* Quality ledger: per material (registry index) we store counts for qualities 0..100 inclusive. */
static int g_quality[ROGUE_MATERIAL_REGISTRY_CAP][ROGUE_MATERIAL_QUALITY_MAX+1];

void rogue_material_quality_reset(void){ memset(g_quality,0,sizeof g_quality); }
static int valid_material(int m){ return m>=0 && m<ROGUE_MATERIAL_REGISTRY_CAP; }

int rogue_material_quality_add(int material_def, int quality, int count){ if(!valid_material(material_def)|| count<0) return -1; if(quality<0) quality=0; if(quality>ROGUE_MATERIAL_QUALITY_MAX) quality=ROGUE_MATERIAL_QUALITY_MAX; g_quality[material_def][quality]+=count; return 0; }
int rogue_material_quality_consume(int material_def, int quality, int count){ if(!valid_material(material_def)|| count<=0) return -1; if(quality<0||quality>ROGUE_MATERIAL_QUALITY_MAX) return -1; int have=g_quality[material_def][quality]; if(have<count) return -2; g_quality[material_def][quality]-=count; return count; }
int rogue_material_quality_count(int material_def, int quality){ if(!valid_material(material_def)|| quality<0||quality>ROGUE_MATERIAL_QUALITY_MAX) return -1; return g_quality[material_def][quality]; }
int rogue_material_quality_total(int material_def){ if(!valid_material(material_def)) return -1; int sum=0; for(int q=0;q<=ROGUE_MATERIAL_QUALITY_MAX;q++) sum+=g_quality[material_def][q]; return sum; }
int rogue_material_quality_average(int material_def){ if(!valid_material(material_def)) return -1; long long num=0; long long denom=0; for(int q=0;q<=ROGUE_MATERIAL_QUALITY_MAX;q++){ int c=g_quality[material_def][q]; if(c>0){ num += (long long)q * c; denom += c; } } if(denom==0) return -1; return (int)(num/denom); }
float rogue_material_quality_bias(int material_def){ int avg = rogue_material_quality_average(material_def); if(avg<0) return 0.0f; return (float)avg / (float)ROGUE_MATERIAL_QUALITY_MAX; }

/* Simple LCG step */
static unsigned int rng_next(unsigned int* s){ *s = (*s * 1664525u) + 1013904223u; return *s; }

int rogue_material_refine(int material_def, int from_quality, int to_quality, int consume_count,
                          unsigned int* rng_state, int* out_produced, int* out_crit){
    if(out_produced) *out_produced=0; if(out_crit) *out_crit=0; if(!valid_material(material_def) || !rng_state || !out_produced || !out_crit) return -1;
    if(from_quality<0||from_quality>ROGUE_MATERIAL_QUALITY_MAX||to_quality<0||to_quality>ROGUE_MATERIAL_QUALITY_MAX||to_quality<=from_quality||consume_count<=0) return -1;
    int have = rogue_material_quality_count(material_def, from_quality); if(have < consume_count) return -2;
    /* Consume source up front */
    g_quality[material_def][from_quality] -= consume_count;
    /* Base production 70% */
    int produced = (int)((long long)consume_count * 70ll / 100ll);
    unsigned int r = rng_next(rng_state); unsigned int roll = r % 100u; /* 0..99 */
    if(roll < 10u){ /* failure 10% */
        produced = (int)((long long)produced * 25ll / 100ll); /* 25% of base */
        if(produced<=0){ /* all lost */ if(out_produced) *out_produced=0; return -3; }
    } else if(roll >= 10u && roll < 15u){ /* critical 5% (10-14) */
        produced = (int)(produced + (produced+1)/2); /* +50% rounded */
        int overflow = (int)((long long)produced * 20ll / 100ll); /* 20% escalates */
        if(overflow>0 && to_quality < ROGUE_MATERIAL_QUALITY_MAX){ g_quality[material_def][to_quality+1] += overflow; produced -= overflow; if(out_crit) *out_crit=1; }
        else if(out_crit) *out_crit=1;
    }
    if(produced>0) g_quality[material_def][to_quality] += produced; else return -3; if(out_produced) *out_produced=produced; return 0;
}
