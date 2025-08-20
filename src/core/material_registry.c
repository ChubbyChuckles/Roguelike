#include "core/material_registry.h"
#include "core/path_utils.h"
#include "core/loot_item_defs.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

static RogueMaterialDef g_materials[ROGUE_MATERIAL_REGISTRY_CAP];
static int g_material_count = 0;

void rogue_material_registry_reset(void){ g_material_count=0; }
int rogue_material_count(void){ return g_material_count; }
const RogueMaterialDef* rogue_material_get(int idx){ if(idx<0||idx>=g_material_count) return NULL; return &g_materials[idx]; }

static int category_from_str(const char* s){ if(!s) return -1; if(strcmp(s,"ore")==0) return ROGUE_MAT_ORE; if(strcmp(s,"plant")==0) return ROGUE_MAT_PLANT; if(strcmp(s,"essence")==0) return ROGUE_MAT_ESSENCE; if(strcmp(s,"component")==0) return ROGUE_MAT_COMPONENT; if(strcmp(s,"currency")==0) return ROGUE_MAT_CURRENCY; return -1; }

const RogueMaterialDef* rogue_material_find(const char* id){ if(!id) return NULL; for(int i=0;i<g_material_count;i++){ if(strcmp(g_materials[i].id,id)==0) return &g_materials[i]; } return NULL; }
const RogueMaterialDef* rogue_material_find_by_item(int item_def_index){ if(item_def_index<0) return NULL; for(int i=0;i<g_material_count;i++){ if(g_materials[i].item_def_index==item_def_index) return &g_materials[i]; } return NULL; }
int rogue_material_prefix_search(const char* prefix, int* out_indices, int max){ if(!prefix||!out_indices||max<=0) return 0; int n=0; size_t plen=strlen(prefix); for(int i=0;i<g_material_count && n<max;i++){ if(strncmp(g_materials[i].id,prefix,plen)==0) out_indices[n++]=i; } return n; }

static void trim(char* s){ size_t n=strlen(s); while(n>0 && (s[n-1]=='\r'||s[n-1]=='\n'||s[n-1]==' '||s[n-1]=='\t')) s[--n]='\0'; char* p=s; while(*p==' '||*p=='\t') p++; if(p!=s) memmove(s,p,strlen(p)+1); }

int rogue_material_registry_load_path(const char* path){ FILE* f=NULL; 
#if defined(_MSC_VER)
    fopen_s(&f,path,"rb");
#else
    f=fopen(path,"rb");
#endif
    if(!f){ fprintf(stderr,"material_registry: cannot open %s\n", path); return -1; }
        char line[256]; int added=0; while(fgets(line,sizeof line,f)){
        trim(line); if(line[0]=='#' || line[0]=='\0') continue; if(g_material_count>=ROGUE_MATERIAL_REGISTRY_CAP){ fprintf(stderr,"material_registry: capacity reached\n"); break; }
        /* id,item_def_id,tier,category,base_value */
        char* tok_ctx=NULL; char buf[256];
        { size_t ln = strlen(line); if(ln >= sizeof buf) ln = sizeof buf -1; memcpy(buf,line,ln); buf[ln]='\0'; }
#if defined(_MSC_VER)
        char* tok = strtok_s(buf, ",", &tok_ctx);
#else
        char* tok = strtok(buf, ",");
#endif
        if(!tok) continue; char id[32]; memset(id,0,sizeof id); { size_t ilen=strlen(tok); if(ilen>=sizeof id) ilen=sizeof id-1; memcpy(id,tok,ilen); id[ilen]='\0'; }
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &tok_ctx);
#else
        tok = strtok(NULL, ",");
#endif
        if(!tok) continue; int item_def = rogue_item_def_index(tok); if(item_def<0){ fprintf(stderr,"material_registry: unknown item def %s\n", tok); continue; }
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &tok_ctx);
#else
        tok = strtok(NULL, ",");
#endif
        if(!tok) continue; int tier = atoi(tok); if(tier<0) tier=0; if(tier>50) tier=50; /* sanity */
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &tok_ctx);
#else
        tok = strtok(NULL, ",");
#endif
        if(!tok) continue; int cat = category_from_str(tok); if(cat<0){ fprintf(stderr,"material_registry: bad category %s\n", tok); continue; }
#if defined(_MSC_VER)
        tok = strtok_s(NULL, ",", &tok_ctx);
#else
        tok = strtok(NULL, ",");
#endif
        if(!tok) continue; int base_val = atoi(tok); if(base_val<0) base_val=0;
        /* duplicate id check */
        if(rogue_material_find(id)){ fprintf(stderr,"material_registry: duplicate %s skipped\n", id); continue; }
        RogueMaterialDef* m=&g_materials[g_material_count]; memset(m,0,sizeof *m); { size_t ilen=strlen(id); if(ilen>=sizeof m->id) ilen=sizeof m->id -1; memcpy(m->id,id,ilen); m->id[ilen]='\0'; } m->item_def_index=item_def; m->tier=tier; m->category=cat; m->base_value=base_val; g_material_count++; added++;
    }
    fclose(f); return added; }

int rogue_material_registry_load_default(void){
        char path[256];
        /* Adjusted search path to actual repo location assets/items/materials.cfg */
        if(!rogue_find_asset_path("items/materials.cfg", path, sizeof path)){
                /* fallback to legacy path if ever moved */
                if(!rogue_find_asset_path("materials/materials.cfg", path, sizeof path)) return -1;
        }
        return rogue_material_registry_load_path(path);
}

unsigned int rogue_material_seed_mix(unsigned int world_seed, int material_index){ /* FNV-1a 32bit mix */ unsigned int h=2166136261u; h^=world_seed; h*=16777619u; h^=(unsigned int)material_index; h*=16777619u; return h; }
