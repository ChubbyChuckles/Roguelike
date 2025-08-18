#include "core/loot_affixes.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static RogueAffixDef g_affixes[ROGUE_MAX_AFFIXES];
static int g_affix_count = 0;

int rogue_affixes_reset(void){ g_affix_count=0; return 0; }
int rogue_affix_count(void){ return g_affix_count; }
const RogueAffixDef* rogue_affix_at(int index){ if(index<0||index>=g_affix_count) return NULL; return &g_affixes[index]; }
int rogue_affix_index(const char* id){ if(!id) return -1; for(int i=0;i<g_affix_count;i++){ if(strcmp(g_affixes[i].id,id)==0) return i; } return -1; }

static char* next_field(char** cur){ if(!*cur) return NULL; char* s=*cur; char* p=*cur; while(*p && *p!=','){ p++; } if(*p==','){ *p='\0'; p++; } else if(*p=='\0'){} *cur = (*p)? p:NULL; return s; }
static RogueAffixType parse_type(const char* s){ if(strcmp(s,"PREFIX")==0) return ROGUE_AFFIX_PREFIX; return ROGUE_AFFIX_SUFFIX; }
static RogueAffixStat parse_stat(const char* s){
    if(strcmp(s,"damage_flat")==0) return ROGUE_AFFIX_STAT_DAMAGE_FLAT;
    if(strcmp(s,"agility_flat")==0) return ROGUE_AFFIX_STAT_AGILITY_FLAT; /* legacy */
    if(strcmp(s,"strength_flat")==0) return ROGUE_AFFIX_STAT_STRENGTH_FLAT;
    if(strcmp(s,"dexterity_flat")==0) return ROGUE_AFFIX_STAT_DEXTERITY_FLAT;
    if(strcmp(s,"vitality_flat")==0) return ROGUE_AFFIX_STAT_VITALITY_FLAT;
    if(strcmp(s,"intelligence_flat")==0) return ROGUE_AFFIX_STAT_INTELLIGENCE_FLAT;
    if(strcmp(s,"armor_flat")==0) return ROGUE_AFFIX_STAT_ARMOR_FLAT;
    if(strcmp(s,"resist_physical")==0) return ROGUE_AFFIX_STAT_RESIST_PHYSICAL;
    if(strcmp(s,"resist_fire")==0) return ROGUE_AFFIX_STAT_RESIST_FIRE;
    if(strcmp(s,"resist_cold")==0) return ROGUE_AFFIX_STAT_RESIST_COLD;
    if(strcmp(s,"resist_lightning")==0) return ROGUE_AFFIX_STAT_RESIST_LIGHTNING;
    if(strcmp(s,"resist_poison")==0) return ROGUE_AFFIX_STAT_RESIST_POISON;
    if(strcmp(s,"resist_status")==0) return ROGUE_AFFIX_STAT_RESIST_STATUS;
    return ROGUE_AFFIX_STAT_NONE;
}

static int parse_line(char* line){
    for(char* p=line; *p; ++p){ if(*p=='\r'||*p=='\n'){ *p='\0'; break; } }
    if(line[0]=='#' || line[0]=='\0') return 0;
    char* cursor=line;
    char* f_type = next_field(&cursor);
    char* f_id = next_field(&cursor);
    char* f_stat = next_field(&cursor);
    char* f_min = next_field(&cursor);
    char* f_max = next_field(&cursor);
    char* f_w0 = next_field(&cursor);
    char* f_w1 = next_field(&cursor);
    char* f_w2 = next_field(&cursor);
    char* f_w3 = next_field(&cursor);
    char* f_w4 = next_field(&cursor);
    if(!f_type||!f_id||!f_stat||!f_min||!f_max||!f_w0||!f_w1||!f_w2||!f_w3||!f_w4) return -1;
    if(g_affix_count >= ROGUE_MAX_AFFIXES) return -1; /* bounds guard */
    RogueAffixDef d; memset(&d,0,sizeof d);
    d.type = parse_type(f_type);
#if defined(_MSC_VER)
    strncpy_s(d.id,sizeof d.id,f_id,_TRUNCATE);
#else
    strncpy(d.id,f_id,sizeof d.id -1);
#endif
    d.stat = parse_stat(f_stat);
    d.min_value = (int)strtol(f_min,NULL,10);
    d.max_value = (int)strtol(f_max,NULL,10); if(d.max_value < d.min_value) d.max_value = d.min_value;
    d.weight_per_rarity[0] = (int)strtol(f_w0,NULL,10);
    d.weight_per_rarity[1] = (int)strtol(f_w1,NULL,10);
    d.weight_per_rarity[2] = (int)strtol(f_w2,NULL,10);
    d.weight_per_rarity[3] = (int)strtol(f_w3,NULL,10);
    d.weight_per_rarity[4] = (int)strtol(f_w4,NULL,10);
    g_affixes[g_affix_count++] = d;
    return 1;
}

int rogue_affixes_load_from_cfg(const char* path){
    FILE* f=NULL; int added=0;
#if defined(_MSC_VER)
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f){ fprintf(stderr,"AFFIX_OPEN_FAIL %s\n", path?path:"(null)"); return -1; }
    char line[512];
    while(fgets(line,sizeof line,f)){
        char work[512];
#if defined(_MSC_VER)
        strncpy_s(work,sizeof work,line,_TRUNCATE);
#else
        strncpy(work,line,sizeof work -1); work[sizeof work -1]='\0';
#endif
    int r = parse_line(work); if(r>0) added++; else if(r<0){ /* ignore malformed */ }
    }
    fclose(f); return added;
}

int rogue_affix_roll(RogueAffixType type, int rarity, unsigned int* rng_state){
    if(rarity<0 || rarity>4) return -1; if(!rng_state) return -1;
    int total=0; for(int i=0;i<g_affix_count;i++){ if(g_affixes[i].type==type){ int w=g_affixes[i].weight_per_rarity[rarity]; if(w>0) total += w; }}
    if(total<=0) return -1;
    *rng_state = (*rng_state * 1664525u) + 1013904223u;
    unsigned int pick = *rng_state % (unsigned)total;
    int acc=0; for(int i=0;i<g_affix_count;i++){ if(g_affixes[i].type==type){ int w=g_affixes[i].weight_per_rarity[rarity]; if(w<=0) continue; acc += w; if(pick < (unsigned)acc) return i; }}
    return -1;
}

int rogue_affix_roll_value(int affix_index, unsigned int* rng_state){
    if(!rng_state) return -1; if(affix_index < 0 || affix_index >= g_affix_count) return -1;
    const RogueAffixDef* d = &g_affixes[affix_index];
    int span = d->max_value - d->min_value + 1; if(span <= 0) return d->min_value;
    *rng_state = (*rng_state * 1664525u) + 1013904223u;
    unsigned int r = *rng_state % (unsigned)span;
    return d->min_value + (int)r;
}

int rogue_affix_roll_value_scaled(int affix_index, unsigned int* rng_state, float quality_scalar){
    if(!rng_state) return -1; if(affix_index < 0 || affix_index >= g_affix_count) return -1;
    if(quality_scalar < 0.0f) quality_scalar = 0.0f; /* clamp */
    const RogueAffixDef* d = &g_affixes[affix_index];
    int span = d->max_value - d->min_value + 1; if(span <= 0) return d->min_value;
    /* Derive an exponent shaping factor from quality_scalar: qc=1 -> exp=1 (uniform). Larger qc -> smaller decay -> higher values. */
    float exp = (quality_scalar <= 1.0f)? 1.0f : (1.0f / quality_scalar); /* >1 quality compresses lower region */
    *rng_state = (*rng_state * 1664525u) + 1013904223u;
    /* Convert RNG to [0,1) */
    unsigned int raw = *rng_state & 0x00FFFFFFu; /* 24 bits */
    float u = (float)raw / (float)0x01000000u; /* [0,1) */
    /* Skew distribution: y = u^(exp) then map to integer. Since exp<1 for quality>1, pushes upward. */
    float y = 0.0f; if(u <= 0.0f) y = 0.0f; else {
        /* simple powf replacement (avoid math.h pow potential differences) using exp/log series for small set; fallback linear if unavailable */
        /* For portability without powf, approximate via expf(exp*log(u)). We'll include math.h. */
    }
    /* Because we avoided powf above for portability, implement manual fast approximation: use 3rd order polynomial around (u). For exp in [0.25,1], approximate u^exp ~ u * (1 + (1-exp)*(1-u)). */
    if(exp >= 0.25f && exp <= 1.0f){
        y = u * (1.0f + (1.0f - exp) * (1.0f - u));
    } else {
        /* fallback to uniform if outside expected range */
        y = u;
    }
    int offset = (int)(y * (float)span);
    if(offset >= span) offset = span-1;
    return d->min_value + offset;
}

int rogue_affixes_export_json(char* buf, int cap){
    if(!buf || cap<=0) return -1;
    int w=0; buf[0]='\0';
    int n = snprintf(buf+w, (w<cap? cap-w:0), "["); if(n>0) w+=n;
    int first=1;
    for(int i=0;i<g_affix_count;i++){
        const RogueAffixDef* a=&g_affixes[i];
        n = snprintf(buf+(w<cap?w:cap), (w<cap? cap-w:0), "%s{\"id\":\"%s\",\"type\":%d,\"stat\":%d,\"min\":%d,\"max\":%d,\"w\":[%d,%d,%d,%d,%d]}",
            first?"":",", a->id, a->type, a->stat, a->min_value, a->max_value,
            a->weight_per_rarity[0], a->weight_per_rarity[1], a->weight_per_rarity[2], a->weight_per_rarity[3], a->weight_per_rarity[4]);
        if(n>0) w+=n; first=0;
    }
    n = snprintf(buf+(w<cap?w:cap), (w<cap? cap-w:0), "]"); if(n>0) w+=n;
    if(w>=cap) buf[cap-1]='\0';
    return w;
}
