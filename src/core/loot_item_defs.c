#include "core/loot_item_defs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef ROGUE_ITEM_DEF_CAP
#define ROGUE_ITEM_DEF_CAP 512
#endif

static RogueItemDef g_item_defs[ROGUE_ITEM_DEF_CAP];
static int g_item_def_count = 0;
/* Phase 17.4: open-address hash index (power-of-two sized) for cache-friendly id->index lookup */
static int g_hash_cap = 0; /* number of slots */
static int* g_hash_slots = NULL; /* -1 empty, -2 tombstone (unused) or index into g_item_defs */

static unsigned int hash_str(const char* s){ unsigned int h=2166136261u; while(*s){ unsigned char c=(unsigned char)*s++; h ^= c; h *= 16777619u; } return h; }

int rogue_item_defs_build_index(void){ if(g_hash_slots){ free(g_hash_slots); g_hash_slots=NULL; g_hash_cap=0; }
    if(g_item_def_count==0) return 0; int target = 1; while(target < g_item_def_count*2) target <<=1; g_hash_cap = target; g_hash_slots=(int*)malloc(sizeof(int)*g_hash_cap); if(!g_hash_slots){ g_hash_cap=0; return -1; } for(int i=0;i<g_hash_cap;i++) g_hash_slots[i]=-1; for(int i=0;i<g_item_def_count;i++){ unsigned int h=hash_str(g_item_defs[i].id); int mask=g_hash_cap-1; int pos=(int)(h & mask); int probes=0; while(g_hash_slots[pos]>=0){ pos=(pos+1)&mask; if(++probes>g_hash_cap){ return -2; } } g_hash_slots[pos]=i; }
    return 0; }

int rogue_item_def_index_fast(const char* id){ if(!id) return -1; if(!g_hash_slots || g_hash_cap==0) return rogue_item_def_index(id); unsigned int h=hash_str(id); int mask=g_hash_cap-1; int pos=(int)(h & mask); int probes=0; while(probes<g_hash_cap){ int v=g_hash_slots[pos]; if(v==-1) return -1; if(v>=0){ if(strcmp(g_item_defs[v].id,id)==0) return v; } pos=(pos+1)&mask; probes++; } return -1; }

void rogue_item_defs_reset(void){ g_item_def_count = 0; }
int rogue_item_defs_count(void){ return g_item_def_count; }

static char* next_field(char** cur){
    if(!*cur) return NULL; char* start=*cur; char* p=*cur; while(*p && *p!=','){ p++; }
    if(*p==','){ *p='\0'; p++; } else if(*p=='\0'){ /* end */ }
    *cur = (*p)? p : NULL; return start;
}

static int parse_line(char* line, RogueItemDef* out){
    for(char* p=line; *p; ++p){ if(*p=='\r'||*p=='\n'){ *p='\0'; break; } }
    if(line[0]=='#' || line[0]=='\0') return 0;
    char* cursor=line; char* fields[15]; int nf=0; while(cursor && nf<15){ fields[nf++] = next_field(&cursor); }
    if(nf<14) return -1; /* rarity optional (15th field) */
    RogueItemDef d; memset(&d,0,sizeof d);
#if defined(_MSC_VER)
    strncpy_s(d.id,sizeof d.id, fields[0], _TRUNCATE);
    strncpy_s(d.name,sizeof d.name, fields[1], _TRUNCATE);
    strncpy_s(d.sprite_sheet,sizeof d.sprite_sheet, fields[9], _TRUNCATE);
#else
    strncpy(d.id, fields[0], sizeof d.id -1);
    strncpy(d.name, fields[1], sizeof d.name -1);
    strncpy(d.sprite_sheet, fields[9], sizeof d.sprite_sheet -1);
#endif
    d.category = (RogueItemCategory)strtol(fields[2],NULL,10);
    d.level_req = (int)strtol(fields[3],NULL,10);
    d.stack_max = (int)strtol(fields[4],NULL,10); if(d.stack_max<=0) d.stack_max=1;
    d.base_value = (int)strtol(fields[5],NULL,10);
    d.base_damage_min = (int)strtol(fields[6],NULL,10);
    d.base_damage_max = (int)strtol(fields[7],NULL,10);
    d.base_armor = (int)strtol(fields[8],NULL,10);
    d.sprite_tx = (int)strtol(fields[10],NULL,10);
    d.sprite_ty = (int)strtol(fields[11],NULL,10);
    d.sprite_tw = (int)strtol(fields[12],NULL,10); if(d.sprite_tw<=0) d.sprite_tw=1;
    d.sprite_th = (int)strtol(fields[13],NULL,10); if(d.sprite_th<=0) d.sprite_th=1;
    d.rarity = 0; if(nf>=15){ d.rarity = (int)strtol(fields[14],NULL,10); if(d.rarity<0) d.rarity=0; }
    *out=d; return 1;
}

int rogue_item_defs_validate_file(const char* path, int* out_lines, int cap){
    FILE* f=NULL; int collected=0; int malformed=0;
#if defined(_MSC_VER)
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f) return -1;
    char line[512]; int lineno=0; while(fgets(line,sizeof line,f)){
        lineno++; char work[512];
#if defined(_MSC_VER)
        strncpy_s(work,sizeof work,line,_TRUNCATE);
#else
        strncpy(work,line,sizeof work -1); work[sizeof work -1]='\0';
#endif
        RogueItemDef tmp; int r=parse_line(work,&tmp);
        if(r<0){ malformed++; if(out_lines && collected<cap){ out_lines[collected++]=lineno; } }
    }
    fclose(f);
    return malformed;
}

int rogue_item_defs_load_from_cfg(const char* path){
    FILE* f=NULL;
#if defined(_MSC_VER)
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f) return -1;
    char line[512]; int added=0; int lineno=0;
    while(fgets(line,sizeof line,f)){
        lineno++;
        char work[512];
#if defined(_MSC_VER)
        strncpy_s(work,sizeof work,line,_TRUNCATE);
#else
        strncpy(work,line,sizeof work -1); work[sizeof work -1]='\0';
#endif
    RogueItemDef d; int r=parse_line(work,&d);
        if(r<0){ /* malformed */ fprintf(stderr,"item_defs: malformed line %d in %s\n", lineno,path); continue; }
        if(r==0) continue; /* skip */
        if(g_item_def_count>=ROGUE_ITEM_DEF_CAP){ fprintf(stderr,"item_defs: cap reached (%d)\n", ROGUE_ITEM_DEF_CAP); break; }
        g_item_defs[g_item_def_count++] = d; added++;
    }
    fclose(f);
    /* Rebuild hash index after each file load to keep fast path current (cost acceptable for small counts) */
    rogue_item_defs_build_index();
    return added;
}

const RogueItemDef* rogue_item_def_by_id(const char* id){ if(!id) return NULL; for(int i=0;i<g_item_def_count;i++){ if(strcmp(g_item_defs[i].id,id)==0) return &g_item_defs[i]; } return NULL; }
int rogue_item_def_index(const char* id){ if(!id) return -1; for(int i=0;i<g_item_def_count;i++){ if(strcmp(g_item_defs[i].id,id)==0) return i; } return -1; }
const RogueItemDef* rogue_item_def_at(int index){ if(index<0 || index>=g_item_def_count) return NULL; return &g_item_defs[index]; }

int rogue_item_defs_load_directory(const char* dir_path){
    if(!dir_path) return -1;
    /* Static list of expected category files; user can remove or add (silently skipped if missing). */
    const char* files[] = {"swords.cfg","potions.cfg","armor.cfg","gems.cfg","materials.cfg","misc.cfg"};
    char path[512]; int total=0;
    for(size_t i=0;i<sizeof(files)/sizeof(files[0]); ++i){
        int n = snprintf(path,sizeof path, "%s/%s", dir_path, files[i]);
        if(n <= 0 || n >= (int)sizeof path) continue; /* skip overly long */
        int before = g_item_def_count;
        int added = rogue_item_defs_load_from_cfg(path);
        if(added>0) total += (g_item_def_count - before); /* ensure we count only actually added lines */
    }
    rogue_item_defs_build_index();
    return total;
}
