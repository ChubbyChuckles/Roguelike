#include "core/loot_item_defs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef ROGUE_ITEM_DEF_CAP
#define ROGUE_ITEM_DEF_CAP 512
#endif

static RogueItemDef g_item_defs[ROGUE_ITEM_DEF_CAP];
static int g_item_def_count = 0;

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
    char* cursor=line; char* fields[14]; int nf=0; while(cursor && nf<14){ fields[nf++] = next_field(&cursor); }
    if(nf<14) return -1;
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
    *out=d; return 1;
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
    return added;
}

const RogueItemDef* rogue_item_def_by_id(const char* id){ if(!id) return NULL; for(int i=0;i<g_item_def_count;i++){ if(strcmp(g_item_defs[i].id,id)==0) return &g_item_defs[i]; } return NULL; }
int rogue_item_def_index(const char* id){ if(!id) return -1; for(int i=0;i<g_item_def_count;i++){ if(strcmp(g_item_defs[i].id,id)==0) return i; } return -1; }
