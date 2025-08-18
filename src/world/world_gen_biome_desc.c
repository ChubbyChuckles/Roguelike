#include "world/world_gen_biome_desc.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <dirent.h>
#endif

static int ensure_capacity(RogueBiomeRegistry* reg, int need){
    if(reg->capacity >= need) return 1;
    int newcap = reg->capacity? reg->capacity*2 : 8; while(newcap < need) newcap*=2;
    RogueBiomeDescriptor* nb = (RogueBiomeDescriptor*)realloc(reg->biomes, (size_t)newcap * sizeof(RogueBiomeDescriptor));
    if(!nb) return 0; reg->biomes = nb; reg->capacity = newcap; return 1;
}

void rogue_biome_registry_init(RogueBiomeRegistry* reg){ if(!reg) return; memset(reg,0,sizeof *reg); }
void rogue_biome_registry_free(RogueBiomeRegistry* reg){ if(!reg) return; free(reg->biomes); reg->biomes=NULL; reg->count=reg->capacity=0; }

static void safe_copy(char* dst,size_t cap,const char* src){
    if(!dst||!cap) return; if(!src){ dst[0]='\0'; return; }
#ifdef _MSC_VER
    strncpy_s(dst,cap,src,_TRUNCATE);
#else
    strncpy(dst,src,cap-1); dst[cap-1]='\0';
#endif
}
static void desc_defaults(RogueBiomeDescriptor* d){
    memset(d,0,sizeof *d); d->vegetation_density=0.3f; d->decoration_density=0.2f; d->ambient_color[0]=70; d->ambient_color[1]=70; d->ambient_color[2]=70; safe_copy(d->music_track, sizeof d->music_track, "default"); d->allow_structures=1; d->allow_weather=1;
}

static int parse_kv(char* line, char** k, char** v){
    char* p=line; while(isspace((unsigned char)*p)) p++; if(*p=='#' || *p=='\0') return 0; *k=p; while(*p && *p!='=' && *p!='\n' && *p!='\r') p++; if(*p!='=') return 0; *p='\0'; p++; *v=p; while(*p && *p!='\n' && *p!='\r') p++; *p='\0';
    /* trim */
    char* e=*k+strlen(*k); while(e>*k && isspace((unsigned char)e[-1])) *--e='\0';
    e=*v+strlen(*v); while(e>*v && isspace((unsigned char)e[-1])) *--e='\0';
    return 1;
}

int rogue_biome_descriptor_parse_cfg(const char* text, RogueBiomeDescriptor* out_desc, char* err, size_t err_sz){
    if(err && err_sz) err[0]='\0'; if(!text||!out_desc) return 0; desc_defaults(out_desc); safe_copy(out_desc->name,sizeof out_desc->name,"unnamed");
    char* dup = _strdup(text); if(!dup) return 0; char* ctx_tok=NULL; char* line = strtok_s(dup,"\n", &ctx_tok); int seen_tile=0; while(line){ char* k,*v; if(parse_kv(line,&k,&v)) { if(strcmp(k,"name")==0){ safe_copy(out_desc->name,sizeof out_desc->name,v); }
        else if(strcmp(k,"music")==0){ safe_copy(out_desc->music_track,sizeof out_desc->music_track,v); }
        else if(strcmp(k,"vegetation_density")==0){ out_desc->vegetation_density=(float)atof(v); }
        else if(strcmp(k,"decoration_density")==0){ out_desc->decoration_density=(float)atof(v); }
        else if(strcmp(k,"ambient_color")==0){ int r,g,b; if(sscanf_s(v,"%d,%d,%d",&r,&g,&b)==3){ out_desc->ambient_color[0]=(unsigned char)r; out_desc->ambient_color[1]=(unsigned char)g; out_desc->ambient_color[2]=(unsigned char)b; } }
        else if(strcmp(k,"allow_structures")==0){ out_desc->allow_structures=(unsigned char)(atoi(v)!=0); }
        else if(strcmp(k,"allow_weather")==0){ out_desc->allow_weather=(unsigned char)(atoi(v)!=0); }
        else if(strncmp(k,"tile_",5)==0){ int t = -1; /* map tile name suffix to tile enum */ const char* suf=k+5; 
            /* simple mapping */
            for(int i=0;i<ROGUE_TILE_MAX;i++){ /* create static table names */ }
            if(strcmp(suf,"grass")==0) t=ROGUE_TILE_GRASS; else if(strcmp(suf,"forest")==0) t=ROGUE_TILE_FOREST; else if(strcmp(suf,"water")==0) t=ROGUE_TILE_WATER; else if(strcmp(suf,"mountain")==0) t=ROGUE_TILE_MOUNTAIN; else if(strcmp(suf,"swamp")==0) t=ROGUE_TILE_SWAMP; else if(strcmp(suf,"snow")==0) t=ROGUE_TILE_SNOW; else if(strcmp(suf,"river")==0) t=ROGUE_TILE_RIVER;
            if(t>=0){ out_desc->tile_weights[t]=(float)atof(v); seen_tile=1; }
        } }
        line = strtok_s(NULL,"\n", &ctx_tok);
    }
    free(dup);
    if(!seen_tile){ if(err && err_sz) safe_copy(err,err_sz,"no tile_* entries"); return 0; }
    /* normalize */
    double sum=0.0; for(int i=0;i<ROGUE_TILE_MAX;i++) sum+=out_desc->tile_weights[i]; if(sum<=0.0){ if(err&&err_sz) safe_copy(err,err_sz,"tile weights sum zero"); return 0; }
    out_desc->tile_weight_count=0; for(int i=0;i<ROGUE_TILE_MAX;i++){ if(out_desc->tile_weights[i]>0){ out_desc->tile_weights[i]/=(float)sum; out_desc->tile_weight_count++; } }
    if(out_desc->vegetation_density<0) out_desc->vegetation_density=0; if(out_desc->vegetation_density>1) out_desc->vegetation_density=1;
    if(out_desc->decoration_density<0) out_desc->decoration_density=0; if(out_desc->decoration_density>1) out_desc->decoration_density=1;
    return 1;
}

int rogue_biome_registry_add(RogueBiomeRegistry* reg, const RogueBiomeDescriptor* desc){ if(!reg||!desc) return -1; if(!ensure_capacity(reg, reg->count+1)) return -1; reg->biomes[reg->count]=*desc; reg->biomes[reg->count].id=reg->count; return reg->count++; }

#ifdef _WIN32
static int ends_with(const char* s, const char* suf){ size_t ls=strlen(s), lsu=strlen(suf); return ls>=lsu && _stricmp(s+ls-lsu,suf)==0; }
#else
static int ends_with(const char* s, const char* suf){ size_t ls=strlen(s), lsu=strlen(suf); return ls>=lsu && strcasecmp(s+ls-lsu,suf)==0; }
#endif

int rogue_biome_registry_load_dir(RogueBiomeRegistry* reg, const char* dir_path, char* err, size_t err_sz){
    if(err&&err_sz) err[0]='\0'; if(!reg||!dir_path) return -1; int loaded=0;
#ifdef _WIN32
    char pattern[512]; snprintf(pattern,sizeof pattern,"%s\\*.biome.cfg", dir_path);
    WIN32_FIND_DATAA fd; HANDLE h=FindFirstFileA(pattern,&fd); if(h==INVALID_HANDLE_VALUE){ if(err&&err_sz) safe_copy(err,err_sz,"no biome cfg files"); return 0; }
    do {
        if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue; if(!ends_with(fd.cFileName, ".biome.cfg")) continue;
        char full[512]; snprintf(full,sizeof full,"%s\\%s", dir_path, fd.cFileName);
    FILE* f=NULL; fopen_s(&f,full,"rb"); if(!f){ continue; }
        fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET); char* buf=(char*)malloc((size_t)sz+1); if(!buf){ fclose(f); continue; } fread(buf,1,(size_t)sz,f); buf[sz]='\0'; fclose(f);
        RogueBiomeDescriptor d; char perr[128]; if(rogue_biome_descriptor_parse_cfg(buf,&d,perr,sizeof perr)){ rogue_biome_registry_add(reg,&d); loaded++; } else { /* skip invalid */ }
        free(buf);
    } while(FindNextFileA(h,&fd));
    FindClose(h);
#else
    DIR* dir=opendir(dir_path); if(!dir){ if(err&&err_sz) safe_copy(err,err_sz,"open dir failed"); return -1; }
    struct dirent* de; while((de=readdir(dir))){ if(de->d_type==DT_DIR) continue; if(!ends_with(de->d_name, ".biome.cfg")) continue; char full[512]; snprintf(full,sizeof full,"%s/%s", dir_path, de->d_name); FILE* f=fopen(full,"rb"); if(!f) continue; fseek(f,0,SEEK_END); long sz=ftell(f); fseek(f,0,SEEK_SET); char* buf=(char*)malloc((size_t)sz+1); if(!buf){ fclose(f); continue; } fread(buf,1,(size_t)sz,f); buf[sz]='\0'; fclose(f); RogueBiomeDescriptor d; char perr[128]; if(rogue_biome_descriptor_parse_cfg(buf,&d,perr,sizeof perr)){ rogue_biome_registry_add(reg,&d); loaded++; } free(buf); }
    closedir(dir);
#endif
    return loaded;
}

void rogue_biome_blend_palettes(const RogueBiomeDescriptor* a, const RogueBiomeDescriptor* b, float t, float* out_weights){
    if(t<0) t=0; if(t>1) t=1; float one_minus_t=1.0f-t; double sum=0.0; for(int i=0;i<ROGUE_TILE_MAX;i++){ float v = a->tile_weights[i]*one_minus_t + b->tile_weights[i]*t; out_weights[i]=v; sum+=v; }
    if(sum>0){ for(int i=0;i<ROGUE_TILE_MAX;i++) out_weights[i]/=(float)sum; }
}
